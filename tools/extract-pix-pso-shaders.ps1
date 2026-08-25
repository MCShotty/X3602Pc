param(
    [Parameter(Mandatory)]
    [string]$ExportRoot,
    [Parameter(Mandatory)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$exportRootPath = [IO.Path]::GetFullPath($ExportRoot)
$sourcePath = Join-Path $exportRootPath 'CreatePSOs.cpp'
$resourcePath = Join-Path $exportRootPath 'resources.bin'
foreach ($requiredPath in @($sourcePath, $resourcePath)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required PIX export input is missing: $requiredPath"
    }
}

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

if (-not ('PixPsoCompressionNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class PixPsoCompressionNative
{
    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool CreateDecompressor(
        uint algorithm,
        IntPtr allocationRoutines,
        out IntPtr decompressor);

    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool Decompress(
        IntPtr decompressor,
        IntPtr compressedData,
        UIntPtr compressedDataSize,
        IntPtr uncompressedBuffer,
        UIntPtr uncompressedBufferSize,
        out UIntPtr uncompressedDataSize);

    [DllImport("cabinet.dll", SetLastError = true)]
    public static extern bool CloseDecompressor(IntPtr decompressor);
}
'@
}

function Expand-PixChunk {
    param(
        [Parameter(Mandatory)]
        [byte[]]$Compressed,
        [Parameter(Mandatory)]
        [IntPtr]$Decompressor
    )

    $compressedPointer = [IntPtr]::Zero
    $decompressedPointer = [IntPtr]::Zero
    try {
        $compressedPointer =
            [Runtime.InteropServices.Marshal]::AllocHGlobal($Compressed.Length)
        [Runtime.InteropServices.Marshal]::Copy(
            $Compressed,
            0,
            $compressedPointer,
            $Compressed.Length)

        $decompressedSize = [UIntPtr]::Zero
        $queryResult = [PixPsoCompressionNative]::Decompress(
            $Decompressor,
            $compressedPointer,
            [UIntPtr]::new([uint64]$Compressed.Length),
            [IntPtr]::Zero,
            [UIntPtr]::Zero,
            [ref]$decompressedSize)
        $queryError = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        if ($queryResult -or $queryError -ne 122) {
            throw "Decompress size query failed with error $queryError."
        }

        $decompressedLength = $decompressedSize.ToUInt64()
        if ($decompressedLength -eq 0 -or
            $decompressedLength -gt [int]::MaxValue) {
            throw "Invalid decompressed size: $decompressedLength."
        }
        $decompressedPointer =
            [Runtime.InteropServices.Marshal]::AllocHGlobal(
                [int]$decompressedLength)
        $actualSize = [UIntPtr]::Zero
        if (-not [PixPsoCompressionNative]::Decompress(
                $Decompressor,
                $compressedPointer,
                [UIntPtr]::new([uint64]$Compressed.Length),
                $decompressedPointer,
                [UIntPtr]::new($decompressedLength),
                [ref]$actualSize)) {
            $errorCode =
                [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            throw "Decompress failed with error $errorCode."
        }
        if ($actualSize.ToUInt64() -ne $decompressedLength) {
            throw (
                "Decompressed size mismatch: expected {0}, received {1}." -f
                $decompressedLength,
                $actualSize.ToUInt64()
            )
        }

        $decompressed = [byte[]]::new([int]$decompressedLength)
        [Runtime.InteropServices.Marshal]::Copy(
            $decompressedPointer,
            $decompressed,
            0,
            $decompressed.Length)
        $decompressed
    }
    finally {
        if ($decompressedPointer -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::FreeHGlobal(
                $decompressedPointer)
        }
        if ($compressedPointer -ne [IntPtr]::Zero) {
            [Runtime.InteropServices.Marshal]::FreeHGlobal($compressedPointer)
        }
    }
}

$source = Get-Content -LiteralPath $sourcePath -Raw
$sectionPattern =
    '(?ms)^// ApiObjectId\s+=\s+(?<id>\d+)\s*\r?\n' +
    '(?<body>.*?)(?=^// ApiObjectId\s+=|\z)'
$readPattern =
    'g_resourceReader->Read\([^,]+,\s*(?<size>\d+)\s*\);'
$stagePattern =
    'psoDesc\.(?<stage>VS|PS|DS|HS|GS|CS|AS|MS)\s*=\s*' +
    '\{\s*reinterpret_cast<BYTE\*>\(&data\[offset\]\),\s*' +
    '(?<size>\d+)\s*\};'

$stream = [IO.File]::Open(
    $resourcePath,
    [IO.FileMode]::Open,
    [IO.FileAccess]::Read,
    [IO.FileShare]::Read)
$decompressor = [IntPtr]::Zero
$manifest = [Collections.Generic.List[object]]::new()
[uint64]$compressedOffset = 0
try {
    if (-not [PixPsoCompressionNative]::CreateDecompressor(
            3,
            [IntPtr]::Zero,
            [ref]$decompressor)) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "CreateDecompressor failed with error $errorCode."
    }

    foreach ($section in [regex]::Matches($source, $sectionPattern)) {
        $psoId = [uint32]$section.Groups['id'].Value
        $body = $section.Groups['body'].Value
        $reads = [regex]::Matches($body, $readPattern)
        if ($reads.Count -eq 0) {
            continue
        }

        foreach ($read in $reads) {
            $compressedSize = [uint32]$read.Groups['size'].Value
            $compressed = [byte[]]::new($compressedSize)
            [void]$stream.Seek(
                [int64]$compressedOffset,
                [IO.SeekOrigin]::Begin)
            $readCount = $stream.Read(
                $compressed,
                0,
                $compressed.Length)
            if ($readCount -ne $compressed.Length) {
                throw (
                    "Short read at offset {0}: expected {1}, read {2}." -f
                    $compressedOffset,
                    $compressed.Length,
                    $readCount
                )
            }
            $decompressed = Expand-PixChunk `
                -Compressed $compressed `
                -Decompressor $decompressor

            $shaderOffset = 0
            foreach ($stageMatch in [regex]::Matches($body, $stagePattern)) {
                $stage = $stageMatch.Groups['stage'].Value
                $shaderSize = [uint32]$stageMatch.Groups['size'].Value
                if (($shaderOffset + $shaderSize) -gt
                    $decompressed.Length) {
                    throw (
                        "PSO {0} {1} overruns its chunk: {2}+{3}>{4}." -f
                        $psoId,
                        $stage,
                        $shaderOffset,
                        $shaderSize,
                        $decompressed.Length
                    )
                }

                $shader = [byte[]]::new($shaderSize)
                [Array]::Copy(
                    $decompressed,
                    $shaderOffset,
                    $shader,
                    0,
                    $shaderSize)
                $fileName = 'pso-{0}-{1}.dxil' -f (
                    $psoId,
                    $stage.ToLowerInvariant())
                $outputPath = Join-Path $outputRoot $fileName
                [IO.File]::WriteAllBytes($outputPath, $shader)
                $manifest.Add([pscustomobject]@{
                    PsoId = $psoId
                    Stage = $stage
                    ShaderSize = $shaderSize
                    Sha256 = (
                        Get-FileHash `
                            -LiteralPath $outputPath `
                            -Algorithm SHA256
                    ).Hash
                    FileName = $fileName
                    CompressedOffset = $compressedOffset
                    CompressedSize = $compressedSize
                    DecompressedSize = $decompressed.Length
                })
                $shaderOffset += $shaderSize
            }
            $compressedOffset += $compressedSize
        }
    }
}
finally {
    $stream.Dispose()
    if ($decompressor -ne [IntPtr]::Zero) {
        [void][PixPsoCompressionNative]::CloseDecompressor($decompressor)
    }
}

$manifestPath = Join-Path $outputRoot 'manifest.json'
$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8

[pscustomobject]@{
    ExportRoot = $exportRootPath
    OutputDirectory = $outputRoot
    ShaderCount = $manifest.Count
    PsoCount = @($manifest.PsoId | Sort-Object -Unique).Count
    ManifestPath = $manifestPath
    CompressedPrefixSize = $compressedOffset
}

param(
    [Parameter(Mandatory)]
    [string]$CacheRoot,
    [Parameter(Mandatory)]
    [string]$OutputDirectory,
    [string[]]$CacheFileName = @()
)

$ErrorActionPreference = 'Stop'

$cachePath = [IO.Path]::GetFullPath($CacheRoot)
if (-not (Test-Path -LiteralPath $cachePath -PathType Container)) {
    throw "XeO3 shader-cache directory is missing: $cachePath"
}

$outputPath = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($outputPath) | Out-Null

$files = if ($CacheFileName.Count -gt 0) {
    foreach ($name in $CacheFileName) {
        if ([IO.Path]::GetFileName($name) -ne $name) {
            throw "CacheFileName must not contain a directory: $name"
        }
        $path = Join-Path $cachePath $name
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "XeO3 shader-cache file is missing: $path"
        }
        Get-Item -LiteralPath $path
    }
} else {
    Get-ChildItem -LiteralPath $cachePath -Filter '*.shc' -File
}

$manifest = [Collections.Generic.List[object]]::new()
foreach ($file in $files) {
    $bytes = [IO.File]::ReadAllBytes($file.FullName)
    $containerOffset = -1
    $searchLimit = [Math]::Min(128, $bytes.Length - 4)
    for ($offset = 0; $offset -le $searchLimit; ++$offset) {
        if ($bytes[$offset] -eq 0x44 -and
            $bytes[$offset + 1] -eq 0x58 -and
            $bytes[$offset + 2] -eq 0x42 -and
            $bytes[$offset + 3] -eq 0x43) {
            $containerOffset = $offset
            break
        }
    }
    if ($containerOffset -lt 0 -or
        $containerOffset + 28 -gt $bytes.Length) {
        throw "No complete DXBC header was found in $($file.FullName)."
    }

    $containerSize = [BitConverter]::ToUInt32(
        $bytes,
        $containerOffset + 24)
    if ($containerSize -lt 32 -or
        $containerOffset + $containerSize -gt $bytes.Length) {
        throw (
            "Invalid DXBC size in {0}: offset={1}, size={2}, file={3}." -f
            $file.FullName,
            $containerOffset,
            $containerSize,
            $bytes.Length
        )
    }

    $container = [byte[]]::new($containerSize)
    [Array]::Copy(
        $bytes,
        $containerOffset,
        $container,
        0,
        $container.Length)
    $outputName = $file.BaseName + '.dxil'
    $shaderPath = Join-Path $outputPath $outputName
    [IO.File]::WriteAllBytes($shaderPath, $container)

    $nameMatch = [regex]::Match(
        $file.Name,
        '^V[^_]+_[^_]+_[^_]+_(?<variant>[0-9A-F]{16})_' +
        '(?<program>[0-9A-F]{16})\.\d+\.shc$',
        [Text.RegularExpressions.RegexOptions]::IgnoreCase)
    $manifest.Add([pscustomobject]@{
        CacheFile = $file.Name
        CacheBytes = $file.Length
        ContainerOffset = $containerOffset
        ContainerSize = $containerSize
        ContainerSha256 = (
            Get-FileHash -LiteralPath $shaderPath -Algorithm SHA256
        ).Hash
        VariantKey = if ($nameMatch.Success) {
            $nameMatch.Groups['variant'].Value.ToUpperInvariant()
        } else {
            $null
        }
        ProgramKey = if ($nameMatch.Success) {
            $nameMatch.Groups['program'].Value.ToUpperInvariant()
        } else {
            $null
        }
        OutputFile = $outputName
    })
}

$manifestPath = Join-Path $outputPath 'manifest.json'
$manifest |
    ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $manifestPath -Encoding utf8

[pscustomobject]@{
    CacheRoot = $cachePath
    OutputDirectory = $outputPath
    ShaderCount = $manifest.Count
    ManifestPath = $manifestPath
}

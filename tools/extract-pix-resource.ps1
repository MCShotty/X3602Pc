param(
    [Parameter(Mandatory)]
    [string]$ExportRoot,

    [Parameter(Mandatory)]
    [int]$ResourceId,

    [ValidateSet('Resource', 'PSO', 'ComputePSO')]
    [string]$AssetKind = 'Resource',

    [Parameter(Mandatory)]
    [long]$DecompressedOffset,

    [Parameter(Mandatory)]
    [long]$Length,

    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$export = (Resolve-Path -LiteralPath $ExportRoot).Path
$resourcesBin = Join-Path $export 'resources.bin'
$frameResources = Join-Path $export 'FrameResources_000.cpp'
if (-not (Test-Path -LiteralPath $resourcesBin) -or
    -not (Test-Path -LiteralPath $frameResources)) {
    throw "The PIX C++ export is missing resources.bin or FrameResources_000.cpp: $export"
}

$compressedSizes = @{}
Get-ChildItem -LiteralPath $export -Filter 'CreateAndInitResources_*.cpp' |
    Sort-Object Name |
    ForEach-Object {
        $source = Get-Content -LiteralPath $_.FullName -Raw
        [regex]::Matches(
            $source,
            'void\s+CreateAndInitResource_(\d+)\s*\(\)\s*\{(?s:(.*?))(?=\r?\n// ApiObjectId|\z)') |
            ForEach-Object {
                $id = [int]$_.Groups[1].Value
                $read = [regex]::Match(
                    $_.Groups[2].Value,
                    'g_resourceReader->Read\(uncompressedData,\s*(\d+)\s*\);')
                $compressedSizes["Resource_$id"] = if ($read.Success) {
                    [long]$read.Groups[1].Value
                } else {
                    [long]0
                }
            }
    }

$psoSource = Get-Content -LiteralPath (Join-Path $export 'CreatePSOs.cpp') -Raw
[regex]::Matches(
    $psoSource,
    'void\s+CreateGraphicsPipelineState_(\d+)\s*\(\)\s*\{(?s:(.*?))(?=\r?\n// ApiObjectId|\z)') |
    ForEach-Object {
        $id = [int]$_.Groups[1].Value
        $read = [regex]::Match(
            $_.Groups[2].Value,
            'g_resourceReader->Read\(data,\s*(\d+)\s*\);')
        if (-not $read.Success) {
            throw "CreateGraphicsPipelineState_$id has no captured data read"
        }
        $compressedSizes["PSO_$id"] = [long]$read.Groups[1].Value
    }
[regex]::Matches(
    $psoSource,
    'void\s+CreateComputePipelineState_(\d+)\s*\(\)\s*\{(?s:(.*?))(?=\r?\n// ApiObjectId|\z)') |
    ForEach-Object {
        $id = [int]$_.Groups[1].Value
        $read = [regex]::Match(
            $_.Groups[2].Value,
            'g_resourceReader->Read\(data,\s*(\d+)\s*\);')
        if (-not $read.Success) {
            throw "CreateComputePipelineState_$id has no captured data read"
        }
        $compressedSizes["ComputePSO_$id"] = [long]$read.Groups[1].Value
    }

$frameSource = Get-Content -LiteralPath $frameResources -Raw
$assetCalls = [regex]::Matches(
    $frameSource,
    'Create(GraphicsPipelineState|ComputePipelineState|AndInitResource)_(\d+)\(\);') |
    ForEach-Object {
        [pscustomobject]@{
            Kind = switch ($_.Groups[1].Value) {
                'GraphicsPipelineState' { 'PSO' }
                'ComputePipelineState' { 'ComputePSO' }
                default { 'Resource' }
            }
            Id = [int]$_.Groups[2].Value
        }
    }

$compressedOffset = [long]0
$compressedSize = $null
foreach ($asset in $assetCalls) {
    $key = "$($asset.Kind)_$($asset.Id)"
    if (-not $compressedSizes.ContainsKey($key)) {
        throw "$key has no matching generated definition"
    }
    if ($asset.Kind -eq $AssetKind -and $asset.Id -eq $ResourceId) {
        $compressedSize = [long]$compressedSizes[$key]
        break
    }
    $compressedOffset += [long]$compressedSizes[$key]
}

if ($null -eq $compressedSize) {
    throw "$AssetKind $ResourceId is not present in the generated creation order"
}
if ($compressedSize -le 0) {
    throw "$AssetKind $ResourceId has no captured initialization data"
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$toolSource = Join-Path $PSScriptRoot 'extract-pix-resource.cpp'
$toolDirectory = Join-Path $repoRoot 'out\tools'
$tool = Join-Path $toolDirectory 'extract-pix-resource.exe'
if (-not (Test-Path -LiteralPath $tool) -or
    (Get-Item -LiteralPath $tool).LastWriteTimeUtc -lt
        (Get-Item -LiteralPath $toolSource).LastWriteTimeUtc) {
    New-Item -ItemType Directory -Force -Path $toolDirectory | Out-Null
    . (Join-Path $PSScriptRoot 'enter-native-build-env.ps1')
    & $env:XEO3_CLANG_CL `
        /nologo `
        /std:c++20 `
        /EHsc `
        /O2 `
        /W4 `
        /WX `
        /D_AMD64_ `
        /D_WIN64 `
        $toolSource `
        /Fe:$tool `
        Cabinet.lib
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to build $tool"
    }
}

$resolvedOutput = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    [System.IO.Path]::GetFullPath($OutputPath)
} else {
    [System.IO.Path]::GetFullPath((Join-Path (Get-Location).Path $OutputPath))
}

& $tool `
    $resourcesBin `
    $compressedOffset `
    $compressedSize `
    $DecompressedOffset `
    $Length `
    $resolvedOutput
if ($LASTEXITCODE -ne 0) {
    throw "Resource extraction failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    AssetKind = $AssetKind
    ResourceId = $ResourceId
    CompressedOffset = $compressedOffset
    CompressedSize = $compressedSize
    DecompressedOffset = $DecompressedOffset
    Length = $Length
    OutputPath = $resolvedOutput
}

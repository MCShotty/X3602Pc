[CmdletBinding()]
param(
    [string]$SourcePath =
        'D:\Games\AC6 shit\XeO3-AC6-lab\ProbeLogs\ac6-shader-19748-0100-ps_6_0-original.hlsl',
    [string]$OutputDirectory = '',
    [string]$DxcPath =
        'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'out\pix\pso716-tone-map-probe'
}

$expectedSourceHash =
    'F219FC5751975AB632C20F049044F01A90831029DD7F59A406CDA5349089D06C'
$expectedCommonHeaderHash =
    '881C42FC826E9FD6E64730B3ED78702F464356A490327E2AB10E0DDA3044BA57'
$commonHeader = Join-Path $repoRoot 'out\vgpudx12\common_header.h'
$probeIncludes =
    Join-Path $repoRoot 'tools\vgpudx12-includes\ac6-vpos-probe'
$nativeComposite =
    'OutV.oC0.xyz = mad( gpr1.xyz, gpr0.zzz, gpr0.xyw );'
$fixedComposite =
    'OutV.oC0.xyz = mad( gpr1.xyz, float3(1.0f, 1.0f, 1.0f), gpr0.xyw );'

foreach ($requiredPath in @(
    $SourcePath,
    $DxcPath,
    $commonHeader,
    $probeIncludes
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required path is missing: $requiredPath"
    }
}

$sourceHash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash
if ($sourceHash -ne $expectedSourceHash) {
    throw "Unexpected PSO716 source hash: $sourceHash"
}
$commonHeaderHash =
    (Get-FileHash -LiteralPath $commonHeader -Algorithm SHA256).Hash
if ($commonHeaderHash -ne $expectedCommonHeaderHash) {
    throw "Unexpected VGPUDX12 common-header hash: $commonHeaderHash"
}

$source = [IO.File]::ReadAllText($SourcePath)
$matchCount = ([regex]::Matches(
        $source,
        [regex]::Escape($nativeComposite),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)).Count
if ($matchCount -ne 1) {
    throw "Expected one PSO716 composite site, found $matchCount."
}

[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$patchedSourcePath = Join-Path $OutputDirectory 'pso716-tone-map-z1.hlsl'
$compiledShaderPath = Join-Path $OutputDirectory 'pso716-tone-map-z1.dxil'
$patchedSource = $source.Replace($nativeComposite, $fixedComposite)
[IO.File]::WriteAllText(
    $patchedSourcePath,
    $patchedSource,
    [Text.UTF8Encoding]::new($false))

& $DxcPath `
    -E xenon_pixel_shader `
    -T ps_6_0 `
    -I $probeIncludes `
    -I (Split-Path -Parent $commonHeader) `
    -Fo $compiledShaderPath `
    $patchedSourcePath
if ($LASTEXITCODE -ne 0) {
    throw "DXC failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    SourcePath = (Resolve-Path -LiteralPath $patchedSourcePath).Path
    SourceHash = (Get-FileHash -LiteralPath $patchedSourcePath).Hash
    ShaderPath = (Resolve-Path -LiteralPath $compiledShaderPath).Path
    ShaderHash = (Get-FileHash -LiteralPath $compiledShaderPath).Hash
    ShaderSize = (Get-Item -LiteralPath $compiledShaderPath).Length
}

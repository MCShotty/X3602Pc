[CmdletBinding()]
param(
    [string]$SourcePath =
        'D:\Games\AC6 shit\XeO3-AC6-lab\ProbeLogs\ac6-shader-39504-0039-vs_6_0-original.hlsl',
    [string]$OutputDirectory = '',
    [string]$DxcPath =
        'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'out\pix\ac6-exposure-guard'
}

$expectedSourceHash =
    '1297027B92531D70F61A5EFD10F1C387675100E4B10A1B0B590FDB4132427E09'
$expectedCommonHeaderHash =
    '881C42FC826E9FD6E64730B3ED78702F464356A490327E2AB10E0DDA3044BA57'
$commonHeader = Join-Path $repoRoot 'out\vgpudx12\common_header.h'
$probeIncludes =
    Join-Path $repoRoot 'tools\vgpudx12-includes\ac6-vpos-probe'
$sampleAssignment = 'gpr0.x = tmp0.x;'
$guard = @'
gpr0.x = tmp0.x;
gpr0.x = (isfinite(gpr0.x) && gpr0.x > 0.0f && gpr0.x <= 65536.0f) ? gpr0.x : c(106).y;
'@

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
    throw "Unexpected AC6 exposure VS source hash: $sourceHash"
}
$commonHeaderHash =
    (Get-FileHash -LiteralPath $commonHeader -Algorithm SHA256).Hash
if ($commonHeaderHash -ne $expectedCommonHeaderHash) {
    throw "Unexpected VGPUDX12 common-header hash: $commonHeaderHash"
}

$source = [IO.File]::ReadAllText($SourcePath)
$matchCount = ([regex]::Matches(
        $source,
        [regex]::Escape($sampleAssignment),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)).Count
if ($matchCount -ne 1) {
    throw "Expected one AC6 exposure sample assignment, found $matchCount."
}

[IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null
$patchedSourcePath = Join-Path $OutputDirectory 'ac6-exposure-guard.hlsl'
$compiledShaderPath = Join-Path $OutputDirectory 'ac6-exposure-guard.dxil'
$patchedSource = $source.Replace($sampleAssignment, $guard.TrimEnd())
[IO.File]::WriteAllText(
    $patchedSourcePath,
    $patchedSource,
    [Text.UTF8Encoding]::new($false))

& $DxcPath `
    -E xenon_vertex_shader `
    -T vs_6_0 `
    -I $probeIncludes `
    -I (Split-Path -Parent $commonHeader) `
    -Fo $compiledShaderPath `
    $patchedSourcePath
if ($LASTEXITCODE -ne 0) {
    throw "DXC failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    SourcePath = (Resolve-Path -LiteralPath $patchedSourcePath).Path
    SourceHash =
        (Get-FileHash -LiteralPath $patchedSourcePath -Algorithm SHA256).Hash
    ShaderPath = (Resolve-Path -LiteralPath $compiledShaderPath).Path
    ShaderHash =
        (Get-FileHash -LiteralPath $compiledShaderPath -Algorithm SHA256).Hash
    ShaderSize = (Get-Item -LiteralPath $compiledShaderPath).Length
}

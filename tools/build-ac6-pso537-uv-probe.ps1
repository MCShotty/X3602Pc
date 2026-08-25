[CmdletBinding()]
param(
    [string]$SourcePath = (
        'out\pix\ac6-vpos-probe\pso537-vpos-one.hlsl'
    ),

    [string]$OutputDirectory = (
        'out\pix\pso537-uv-probe'
    ),

    [string]$DxcPath = (
        'C:\Program Files (x86)\Windows Kits\10\bin\' +
        '10.0.26100.0\x64\dxc.exe'
    )
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$source = (Resolve-Path -LiteralPath $SourcePath).Path
$expectedSourceHash =
    '9508D1FF52D3698C4BD7931632446707AA4E9CA7AD03947F0E2DFBFADAA0BDE0'
$sourceHash = (
    Get-FileHash -LiteralPath $source -Algorithm SHA256
).Hash
if ($sourceHash -ne $expectedSourceHash) {
    throw "Unexpected PSO 537 unit-source hash: $sourceHash"
}

$needle = 'gpr1.xy = gpr0.xy * c(255).xy;'
$replacement = @'
gpr1.xy = gpr0.xy * c(255).xy;
gpr1.x = gpr1.x * 0.5f;
'@.TrimEnd()
$sourceText = [IO.File]::ReadAllText($source)
$matchCount = (
    [regex]::Matches(
        $sourceText,
        [regex]::Escape($needle),
        [Text.RegularExpressions.RegexOptions]::CultureInvariant
    )
).Count
if ($matchCount -ne 1) {
    throw "Expected one PSO 537 normalized-UV site, found $matchCount."
}

$outputRoot = [IO.Path]::GetFullPath(
    $(if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        $OutputDirectory
    } else {
        Join-Path $repoRoot $OutputDirectory
    })
)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$patchedSourcePath = Join-Path $outputRoot 'pso537-uv-x0_5.hlsl'
$compiledShaderPath = Join-Path $outputRoot 'pso537-uv-x0_5.dxil'
[IO.File]::WriteAllText(
    $patchedSourcePath,
    $sourceText.Replace($needle, $replacement),
    [Text.UTF8Encoding]::new($false))

$probeIncludes =
    Join-Path $repoRoot 'tools\vgpudx12-includes\ac6-vpos-probe'
$commonHeaderDirectory =
    Join-Path $repoRoot 'out\vgpudx12'
& $DxcPath `
    -E xenon_pixel_shader `
    -T ps_6_0 `
    -O3 `
    -I $probeIncludes `
    -I $commonHeaderDirectory `
    -Fo $compiledShaderPath `
    $patchedSourcePath
if ($LASTEXITCODE -ne 0) {
    throw "DXC failed with exit code $LASTEXITCODE."
}

[pscustomobject]@{
    SourcePath = $patchedSourcePath
    SourceSha256 = (
        Get-FileHash -LiteralPath $patchedSourcePath -Algorithm SHA256
    ).Hash
    ShaderPath = $compiledShaderPath
    ShaderSha256 = (
        Get-FileHash -LiteralPath $compiledShaderPath -Algorithm SHA256
    ).Hash
    ShaderBytes = (Get-Item -LiteralPath $compiledShaderPath).Length
}

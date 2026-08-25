[CmdletBinding()]
param(
    [string]$SourcePath = (
        'out\pix\vpos-pso600-sweep\x1-y1\' +
        'pso600-vpos-x1_0-y1_0.hlsl'
    ),

    [string]$OutputDirectory = (
        'out\pix\pso600-uv-probe'
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
    'EA46138D9D2D48EFD45DE65648977ACCE5E6D3BA3380E464BB90496BEAB14383'
$sourceHash = (
    Get-FileHash -LiteralPath $source -Algorithm SHA256
).Hash
if ($sourceHash -ne $expectedSourceHash) {
    throw "Unexpected PSO 600 unit-source hash: $sourceHash"
}

$needle = 'gpr0.xy = mad( gpr0.xy, c(254).xy, gpr1.xy );'
$replacement = @'
gpr0.xy = mad( gpr0.xy, c(254).xy, gpr1.xy );
gpr0.x = gpr0.x * 0.5f;
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
    throw "Expected one PSO 600 normalized-UV site, found $matchCount."
}

$outputRoot = [IO.Path]::GetFullPath(
    $(if ([IO.Path]::IsPathRooted($OutputDirectory)) {
        $OutputDirectory
    } else {
        Join-Path $repoRoot $OutputDirectory
    })
)
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null
$patchedSourcePath = Join-Path $outputRoot 'pso600-uv-x0_5.hlsl'
$compiledShaderPath = Join-Path $outputRoot 'pso600-uv-x0_5.dxil'
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

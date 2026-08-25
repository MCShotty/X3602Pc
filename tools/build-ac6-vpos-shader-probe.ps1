[CmdletBinding()]
param(
    [string]$SourcePath = 'D:\Games\AC6 shit\XeO3-AC6-lab\ProbeLogs\ac6-shader-40656-0152-ps_6_0-original.hlsl',
    [string]$OutputDirectory = '',
    [string]$DxcPath = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\dxc.exe',
    [double]$ScaleX = 1.0,
    [double]$ScaleY = 1.0,
    [switch]$SceneHalfWidthUv
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot 'out\pix\ac6-vpos-probe'
}

$supportedSourceHashes = @{
    '100A6BAAAC3357530C7B6994E1FFD58379B077CD66E59DADF6D2063FC93511F9' = 'pso537'
    '9508D1FF52D3698C4BD7931632446707AA4E9CA7AD03947F0E2DFBFADAA0BDE0' = 'pso537'
    'B58723E2DF25EC130987CA675B6BC4782135E279F1A854B00E03FEDD65126FD6' = 'particle-ps'
    '0643E670AB271C9B2BB92C3A21595BA2DFC5593E660ABCDDC8757F9040A3B243' = 'particle-gs'
    'F3B7618A100551F94B4397743E62B98E8AA90C707B97389C67A339F206638E5C' = 'pso600'
    'EA46138D9D2D48EFD45DE65648977ACCE5E6D3BA3380E464BB90496BEAB14383' = 'pso600'
    '254A83751AA42FDF588075E086C2BF8DB051028D2DE98AED0E4FEC5122D3B635' = 'pso524'
    '9DA072776EBF7956E7BA10B1F8F5C6D41DB804947F28CD77BFAFB52DDF76FBBA' = 'pso524'
    '72E769F92AC91604C584C1DEE3457BBB3927AEBC1DCC4AF05565A012FA8CBEF5' = 'pso735'
    '54A5F75660384957834CB3B2DCF3C8FEB5393AEA6D31D17C100000F372313F24' = 'pso735'
}
$expectedCommonHeaderHash =
    '881C42FC826E9FD6E64730B3ED78702F464356A490327E2AB10E0DDA3044BA57'
$commonHeader = Join-Path $repoRoot 'out\vgpudx12\common_header.h'
$probeIncludes =
    Join-Path $repoRoot 'tools\vgpudx12-includes\ac6-vpos-probe'
$sceneUvNeedle = 'gpr1.xy = gpr0.xy * c(255).xy;'
$scaleXText = $ScaleX.ToString(
    '0.################',
    [Globalization.CultureInfo]::InvariantCulture)
$scaleYText = $ScaleY.ToString(
    '0.################',
    [Globalization.CultureInfo]::InvariantCulture)
if (-not $scaleXText.Contains('.')) {
    $scaleXText += '.0'
}
if (-not $scaleYText.Contains('.')) {
    $scaleYText += '.0'
}
$replacement = (
    '(float2({0}f, {1}f))' -f
    $scaleXText,
    $scaleYText
)

foreach ($requiredPath in @($SourcePath, $DxcPath, $commonHeader, $probeIncludes)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required path is missing: $requiredPath"
    }
}

$sourceHash = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash
if (-not $supportedSourceHashes.ContainsKey($sourceHash)) {
    throw "Unsupported VPOS probe source hash: $sourceHash"
}
$psoName = $supportedSourceHashes[$sourceHash]
if ($SceneHalfWidthUv -and $psoName -ne 'pso537') {
    throw 'The half-width scene UV correction is fingerprinted for PSO537 only.'
}
$probeName = (
    '{0}-vpos-x{1}-y{2}' -f
    $psoName,
    $scaleXText.Replace('-', 'm').Replace('.', '_'),
    $scaleYText.Replace('-', 'm').Replace('.', '_')
)
if ($SceneHalfWidthUv) {
    $probeName += '-scene-uv-half-width'
}
$commonHeaderHash =
    (Get-FileHash -LiteralPath $commonHeader -Algorithm SHA256).Hash
if ($commonHeaderHash -ne $expectedCommonHeaderHash) {
    throw "Unexpected VGPUDX12 common-header hash: $commonHeaderHash"
}

$source = [IO.File]::ReadAllText($SourcePath)
$nativeMatchCount = ([regex]::Matches(
        $source,
        '(?<![A-Za-z0-9_])vpos_Scale(?![A-Za-z0-9_])',
        [Text.RegularExpressions.RegexOptions]::CultureInvariant)).Count
if ($nativeMatchCount -eq 0) {
    throw 'The source contains no screen-space VPOS scale sites.'
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$patchedSourcePath = Join-Path $OutputDirectory "$probeName.hlsl"
$compiledShaderPath = Join-Path $OutputDirectory "$probeName.dxil"
$patchedSource = [regex]::Replace(
    $source,
    '(?<![A-Za-z0-9_])vpos_Scale(?![A-Za-z0-9_])',
    $replacement,
    [Text.RegularExpressions.RegexOptions]::CultureInvariant)
if ($SceneHalfWidthUv) {
    $sceneUvMatchCount = ([regex]::Matches(
            $patchedSource,
            [regex]::Escape($sceneUvNeedle),
            [Text.RegularExpressions.RegexOptions]::CultureInvariant)).Count
    if ($sceneUvMatchCount -ne 1) {
        throw "Expected exactly one PSO537 scene UV site, found $sceneUvMatchCount."
    }
    $patchedSource = $patchedSource.Replace(
        $sceneUvNeedle,
        "$sceneUvNeedle`ngpr1.x = gpr1.x * 0.5f;")
}
[IO.File]::WriteAllText(
    $patchedSourcePath,
    $patchedSource,
    [Text.UTF8Encoding]::new($false))

$entryPoint = 'xenon_pixel_shader'
$targetProfile = 'ps_6_0'
if ($source.Contains('void gsmain(')) {
    $entryPoint = 'gsmain'
    $targetProfile = 'gs_6_0'
}

& $DxcPath `
    -E $entryPoint `
    -T $targetProfile `
    -I $probeIncludes `
    -I (Split-Path -Parent $commonHeader) `
    -Fo $compiledShaderPath `
    $patchedSourcePath
if ($LASTEXITCODE -ne 0) {
    throw "DXC failed with exit code $LASTEXITCODE"
}

[pscustomobject]@{
    SourcePath = (Resolve-Path -LiteralPath $patchedSourcePath).Path
    SourceHash = (Get-FileHash -LiteralPath $patchedSourcePath -Algorithm SHA256).Hash
    ShaderPath = (Resolve-Path -LiteralPath $compiledShaderPath).Path
    ShaderHash = (Get-FileHash -LiteralPath $compiledShaderPath -Algorithm SHA256).Hash
    ShaderSize = (Get-Item -LiteralPath $compiledShaderPath).Length
    Pso = $psoName
    ScaleX = $ScaleX
    ScaleY = $ScaleY
    SceneHalfWidthUv = [bool]$SceneHalfWidthUv
    VposPatchCount = $nativeMatchCount
    EntryPoint = $entryPoint
    TargetProfile = $targetProfile
}

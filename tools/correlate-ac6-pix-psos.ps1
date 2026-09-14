[CmdletBinding()]
param(
    [string]$PsoShaderDirectory =
        'out\pix\ac6-vpos-exposure-gameplay-export-build\ac6-pso-shaders',
    [string]$CaptureIndex =
        'out\shader-cache\ac6-xenia-31580\capture-map.tsv',
    [string]$ShaderUsage =
        'out\shader-cache\ac6-correlated-31580\shader-usage.tsv',
    [string]$OutputDirectory =
        'out\shader-cache\ac6-pix-pso-correlation-31580'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-InputPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
}

$psoShaderPath = Resolve-InputPath $PsoShaderDirectory
$captureIndexPath = Resolve-InputPath $CaptureIndex
$shaderUsagePath = Resolve-InputPath $ShaderUsage
$outputPath = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$captureRows = @(Import-Csv -LiteralPath $captureIndexPath -Delimiter "`t")
if ($captureRows.Count -eq 0) {
    throw "Capture index is empty: $captureIndexPath"
}
$firstUcodePath = [string]$captureRows[0].UcodePath
$probeDirectory = Split-Path -Parent $firstUcodePath
$ucodeName = Split-Path -Leaf $firstUcodePath
if ($ucodeName -notmatch '^ac6-xenos-ucode-(?<pid>\d+)-') {
    throw "Unable to recover capture PID from $ucodeName"
}
$capturePid = [uint32]$Matches.pid

$usageRows = @(Import-Csv -LiteralPath $shaderUsagePath -Delimiter "`t")
$usageByUcode = @{}
foreach ($usage in $usageRows | Where-Object { -not [string]::IsNullOrWhiteSpace($_.UcodeSha256) }) {
    $usageByUcode[('{0}:{1}' -f $usage.Stage, $usage.UcodeSha256).ToUpperInvariant()] = $usage
}

$compiledByHash = @{}
foreach ($capture in $captureRows) {
    if ($capture.Stage -notin @('vertex', 'pixel')) {
        continue
    }
    $hlslName = Split-Path -Leaf ([string]$capture.HlslPath)
    if ($hlslName -notmatch '-original\.hlsl$') {
        continue
    }
    $compiledName = $hlslName -replace '-original\.hlsl$', '-compiled.dxil'
    $compiledPath = Join-Path $probeDirectory $compiledName
    if (-not (Test-Path -LiteralPath $compiledPath -PathType Leaf)) {
        continue
    }
    $compiledHash = (Get-FileHash -LiteralPath $compiledPath -Algorithm SHA256).Hash
    if (-not $compiledByHash.ContainsKey($compiledHash)) {
        $compiledByHash[$compiledHash] = [System.Collections.Generic.List[object]]::new()
    }
    $compiledByHash[$compiledHash].Add([pscustomobject]@{
        Capture = $capture
        CompiledPath = $compiledPath
    })
}

$psoRows = foreach ($file in Get-ChildItem -LiteralPath $psoShaderPath -File) {
    if ($file.Name -notmatch '^pso-(?<pso>\d+)-(?<stage>vs|ps|gs|hs|ds)\.dxil$') {
        continue
    }
    $apiObjectId = [uint32]$Matches.pso
    $shortStage = $Matches.stage
    $stage = switch ($shortStage) {
        'vs' { 'vertex' }
        'ps' { 'pixel' }
        default { $shortStage }
    }
    $dxilHash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    $compiledMatches = $compiledByHash[$dxilHash]
    if ($null -eq $compiledMatches -or $compiledMatches.Count -eq 0) {
        [pscustomobject]@{
            ApiObjectId = $apiObjectId
            Stage = $stage
            DxilSize = $file.Length
            DxilSha256 = $dxilHash
            MatchCount = 0
            CompileSequence = ''
            XenosSequence = ''
            XeniaHash = ''
            XeniaDrawCount = ''
            UcodeSha256 = ''
            XeO3HlslPath = ''
            XenosRecompHlslPath = ''
            PsoShaderPath = $file.FullName
        }
        continue
    }

    foreach ($match in $compiledMatches) {
        $capture = $match.Capture
        $usageKey = ('{0}:{1}' -f $stage, $capture.UcodeSha256).ToUpperInvariant()
        $usage = $usageByUcode[$usageKey]
        [pscustomobject]@{
            ApiObjectId = $apiObjectId
            Stage = $stage
            DxilSize = $file.Length
            DxilSha256 = $dxilHash
            MatchCount = $compiledMatches.Count
            CompileSequence = [uint64]$capture.CompileSequence
            XenosSequence = [uint64]$capture.XenosSequence
            XeniaHash = if ($null -ne $usage) { $usage.XeniaHash } else { '' }
            XeniaDrawCount = if ($null -ne $usage) { $usage.DrawCount } else { '' }
            UcodeSha256 = $capture.UcodeSha256
            XeO3HlslPath = $capture.HlslPath
            XenosRecompHlslPath = if ($null -ne $usage) { $usage.XenosRecompHlslPath } else { '' }
            PsoShaderPath = $file.FullName
        }
    }
}

$indexPath = Join-Path $outputPath 'pso-shaders.tsv'
$psoRows |
    Sort-Object ApiObjectId, Stage, CompileSequence |
    Export-Csv -LiteralPath $indexPath -Delimiter "`t" -NoTypeInformation

$unmatched = @($psoRows | Where-Object MatchCount -eq 0)
$matchedPsoStages = @(
    $psoRows |
        Where-Object MatchCount -ne 0 |
        Group-Object ApiObjectId,Stage
).Count
$summary = [ordered]@{
    schema = 'ac6_pix_pso_correlation_v1'
    capture_pid = $capturePid
    pso_shader_directory = $psoShaderPath
    capture_index = $captureIndexPath
    dumped_pso_stages = @(Get-ChildItem -LiteralPath $psoShaderPath -File).Count
    correlated_pso_stages = $matchedPsoStages
    unmatched_pso_stages = $unmatched.Count
    unmatched = @($unmatched | ForEach-Object { 'pso-{0}-{1}' -f $_.ApiObjectId, $_.Stage })
    output_index = $indexPath
}
$summaryPath = Join-Path $outputPath 'summary.json'
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding utf8
$summary

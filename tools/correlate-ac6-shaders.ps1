[CmdletBinding()]
param(
    [string]$CaptureIndex = 'out\shader-cache\ac6-xenia-31580\capture-map.tsv',
    [string]$XeniaDraws = 'out\ac6\xenia-trace\mission01-state.draws.tsv',
    [string]$XeniaTextures = 'out\ac6\xenia-trace\mission01-state.textures.tsv',
    [string]$XenosRecompIndex =
        'out\shader-cache\ac6-xenosrecomp-live-31580\xenosrecomp-index.tsv',
    [string]$HashTool = 'build\tools\hash-xenos-ucode.exe',
    [string]$OutputDirectory = 'out\shader-cache\ac6-correlated-31580'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-InputPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
}

function Format-Histogram($Rows, [string]$Property) {
    return (@(
        $Rows |
            Group-Object -Property $Property |
            Sort-Object @{ Expression = 'Count'; Descending = $true }, Name |
            ForEach-Object { '{0}:{1}' -f $_.Name, $_.Count }
    ) -join ',')
}

$captureIndexPath = Resolve-InputPath $CaptureIndex
$xeniaDrawsPath = Resolve-InputPath $XeniaDraws
$xeniaTexturesPath = Resolve-InputPath $XeniaTextures
$xenosRecompIndexPath = Resolve-InputPath $XenosRecompIndex
$hashToolPath = Resolve-InputPath $HashTool
$outputPath = if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    [IO.Path]::GetFullPath($OutputDirectory)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$captureRows = @(Import-Csv -LiteralPath $captureIndexPath -Delimiter "`t")
$uniqueCaptures = @(
    $captureRows |
        Group-Object Stage,UcodeSha256 |
        ForEach-Object { $_.Group | Sort-Object XenosSequence | Select-Object -First 1 }
)

$hashByPath = @{}
for ($offset = 0; $offset -lt $uniqueCaptures.Count; $offset += 48) {
    $last = [Math]::Min($offset + 47, $uniqueCaptures.Count - 1)
    $paths = @($uniqueCaptures[$offset..$last] | ForEach-Object UcodePath)
    $hashOutput = @(& $hashToolPath @paths)
    if ($LASTEXITCODE -ne 0) {
        throw "Shader hash helper failed for capture batch starting at $offset"
    }
    $hashRows = @($hashOutput | ConvertFrom-Csv -Delimiter "`t")
    foreach ($hashRow in $hashRows) {
        $hashByPath[[IO.Path]::GetFullPath($hashRow.path)] = $hashRow
    }
}

$recompRows = @(Import-Csv -LiteralPath $xenosRecompIndexPath -Delimiter "`t")
$recompByKey = @{}
foreach ($row in $recompRows) {
    $recompByKey[('{0}:{1}' -f $row.Stage, $row.UcodeSha256).ToUpperInvariant()] = $row
}

$captureByXeniaKey = @{}
foreach ($row in $uniqueCaptures) {
    $hashRow = $hashByPath[[IO.Path]::GetFullPath($row.UcodePath)]
    if ($null -eq $hashRow) {
        throw "No XXH3 result for $($row.UcodePath)"
    }
    $xeniaKey = ('{0}:{1}' -f $row.Stage, $hashRow.xxh3_raw).ToUpperInvariant()
    if ($captureByXeniaKey.ContainsKey($xeniaKey) -and
        $captureByXeniaKey[$xeniaKey].UcodeSha256 -ne $row.UcodeSha256) {
        throw "Conflicting captured shaders have Xenia key $xeniaKey"
    }
    $captureByXeniaKey[$xeniaKey] = $row
}

$drawRows = @(Import-Csv -LiteralPath $xeniaDrawsPath -Delimiter "`t")
$textureRows = @(Import-Csv -LiteralPath $xeniaTexturesPath -Delimiter "`t")
$usage = @{}
foreach ($draw in $drawRows | Where-Object type -eq 'draw') {
    foreach ($stage in @('vertex', 'pixel')) {
        $hash = if ($stage -eq 'vertex') { $draw.vs_hash } else { $draw.ps_hash }
        if ([string]::IsNullOrWhiteSpace($hash) -or $hash -eq '0000000000000000') {
            continue
        }
        $key = ('{0}:{1}' -f $stage, $hash).ToUpperInvariant()
        if (-not $usage.ContainsKey($key)) {
            $usage[$key] = [System.Collections.Generic.List[object]]::new()
        }
        $usage[$key].Add($draw)
    }
}

$texturesByKey = @{}
foreach ($texture in $textureRows) {
    $stage = switch ($texture.stage) {
        'vs' { 'vertex' }
        'ps' { 'pixel' }
        default { continue }
    }
    $key = ('{0}:{1}' -f $stage, $texture.shader_hash).ToUpperInvariant()
    if (-not $texturesByKey.ContainsKey($key)) {
        $texturesByKey[$key] = [System.Collections.Generic.List[object]]::new()
    }
    $texturesByKey[$key].Add($texture)
}

$allKeys = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase)
foreach ($key in $captureByXeniaKey.Keys) { [void]$allKeys.Add($key) }
foreach ($key in $usage.Keys) { [void]$allKeys.Add($key) }
foreach ($key in $texturesByKey.Keys) { [void]$allKeys.Add($key) }

$shaderRows = foreach ($key in $allKeys) {
    $keyParts = $key.Split(':', 2)
    $stage = $keyParts[0].ToLowerInvariant()
    $xeniaHash = $keyParts[1]
    $capture = $captureByXeniaKey[$key]
    $drawUsage = @($usage[$key])
    $textureUsage = @($texturesByKey[$key])
    $recomp = $null
    if ($null -ne $capture) {
        $recomp = $recompByKey[('{0}:{1}' -f $stage, $capture.UcodeSha256).ToUpperInvariant()]
    }

    [pscustomobject]@{
        Stage = $stage
        XeniaHash = $xeniaHash
        DrawCount = $drawUsage.Count
        FirstCommand = if ($drawUsage.Count) { [uint32]($drawUsage | Measure-Object command -Minimum).Minimum } else { '' }
        LastCommand = if ($drawUsage.Count) { [uint32]($drawUsage | Measure-Object command -Maximum).Maximum } else { '' }
        TextureBindingCount = $textureUsage.Count
        TextureEndianUsage = Format-Histogram $textureUsage 'endian'
        TextureFormatUsage = Format-Histogram $textureUsage 'format'
        UcodeSize = if ($null -ne $capture) { $capture.UcodeSize } else { '' }
        UcodeSha256 = if ($null -ne $capture) { $capture.UcodeSha256 } else { '' }
        CaptureOccurrences = if ($null -ne $capture) {
            @($captureRows | Where-Object {
                $_.Stage -eq $stage -and $_.UcodeSha256 -eq $capture.UcodeSha256
            }).Count
        } else { 0 }
        XenosRecompExitCode = if ($null -ne $recomp) { $recomp.ExitCode } else { '' }
        XenosRecompHlslPath = if ($null -ne $recomp) { $recomp.HlslPath } else { '' }
        XeO3HlslPath = if ($null -ne $capture) { $capture.HlslPath } else { '' }
        UcodePath = if ($null -ne $capture) { $capture.UcodePath } else { '' }
    }
}

$pairRows = @(
    $drawRows |
        Where-Object type -eq 'draw' |
        Group-Object vs_hash,ps_hash |
        ForEach-Object {
            $first = $_.Group | Sort-Object { [uint32]$_.command } | Select-Object -First 1
            $last = $_.Group | Sort-Object { [uint32]$_.command } | Select-Object -Last 1
            [pscustomobject]@{
                VertexShaderHash = $first.vs_hash
                PixelShaderHash = $first.ps_hash
                DrawCount = $_.Count
                FirstCommand = [uint32]$first.command
                LastCommand = [uint32]$last.command
                VertexTextureCount = [uint32]$first.vs_texture_count
                PixelTextureCount = [uint32]$first.ps_texture_count
            }
        } |
        Sort-Object DrawCount -Descending
)

$shaderIndexPath = Join-Path $outputPath 'shader-usage.tsv'
$pairIndexPath = Join-Path $outputPath 'pipeline-pairs.tsv'
$shaderRows |
    Sort-Object @{ Expression = 'DrawCount'; Descending = $true }, Stage, XeniaHash |
    Export-Csv -LiteralPath $shaderIndexPath -Delimiter "`t" -NoTypeInformation
$pairRows | Export-Csv -LiteralPath $pairIndexPath -Delimiter "`t" -NoTypeInformation

$traceKeys = @($usage.Keys | Sort-Object -Unique)
$unmatchedKeys = @($traceKeys | Where-Object { -not $captureByXeniaKey.ContainsKey($_) })
$summary = [ordered]@{
    schema = 'ac6_shader_correlation_v1'
    capture_index = $captureIndexPath
    xenia_draws = $xeniaDrawsPath
    xenia_textures = $xeniaTexturesPath
    xenosrecomp_index = $xenosRecompIndexPath
    captured_rows = $captureRows.Count
    captured_unique_shaders = $uniqueCaptures.Count
    xenia_draw_commands = @($drawRows | Where-Object type -eq 'draw').Count
    xenia_unique_shader_keys = $traceKeys.Count
    matched_xenia_shader_keys = $traceKeys.Count - $unmatchedKeys.Count
    unmatched_xenia_shader_keys = $unmatchedKeys
    xenia_pipeline_pairs = $pairRows.Count
    shader_usage = $shaderIndexPath
    pipeline_pairs = $pairIndexPath
}
$summaryPath = Join-Path $outputPath 'summary.json'
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding utf8
$summary

param(
    [string]$CaptureIndex =
        'out\shader-cache\ac6-xenia-31580\capture-map.tsv',
    [string]$XenosRecomp =
        '.tools\build\XenosRecomp-ac6-990d03-v2\XenosRecomp\XenosRecomp.exe',
    [string]$XenosRecompWorktree = '.tools\work\XenosRecomp-ac6',
    [string]$OutputDirectory =
        'out\shader-cache\ac6-xenosrecomp-live-31580',
    [ValidateRange(1, 32)]
    [int]$ThrottleLimit = 8
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Resolve-RepoPath([string]$Path) {
    if ([IO.Path]::IsPathRooted($Path)) {
        return (Resolve-Path -LiteralPath $Path).Path
    }
    return (Resolve-Path -LiteralPath (Join-Path $repoRoot $Path)).Path
}

$captureIndexPath = Resolve-RepoPath $CaptureIndex
$xenosRecompPath = Resolve-RepoPath $XenosRecomp
$xenosRecompWorktreePath = Resolve-RepoPath $XenosRecompWorktree
if ([IO.Path]::IsPathRooted($OutputDirectory)) {
    $outputPath = [IO.Path]::GetFullPath($OutputDirectory)
} else {
    $outputPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
}
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null

$baseCommit = (& git -C $xenosRecompWorktreePath rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to identify the XenosRecomp base commit.'
}
$compilerHash = (Get-FileHash -LiteralPath $xenosRecompPath -Algorithm SHA256).Hash
$captureRows = @(Import-Csv -LiteralPath $captureIndexPath -Delimiter "`t")
if ($captureRows.Count -eq 0) {
    throw "Capture index is empty: $captureIndexPath"
}

$uniqueRows = @(
    $captureRows |
        Group-Object Stage,UcodeSha256 |
        ForEach-Object { $_.Group | Select-Object -First 1 } |
        Sort-Object Stage,UcodeSha256
)

$inputs = foreach ($row in $uniqueRows) {
    if ($row.Stage -notin @('vertex', 'pixel')) {
        throw "Unsupported shader stage '$($row.Stage)'."
    }
    if (-not (Test-Path -LiteralPath $row.UcodePath -PathType Leaf)) {
        throw "Captured ucode is missing: $($row.UcodePath)"
    }
    $file = Get-Item -LiteralPath $row.UcodePath
    if ($file.Length -ne [uint64]$row.UcodeSize -or
        ($file.Length % 12) -ne 0) {
        throw "Invalid captured ucode size: $($row.UcodePath)"
    }
    $hash = (Get-FileHash -LiteralPath $row.UcodePath -Algorithm SHA256).Hash
    if ($hash -ne $row.UcodeSha256) {
        throw "Captured ucode hash mismatch: $($row.UcodePath)"
    }
    [pscustomobject]@{
        Stage = [string]$row.Stage
        UcodeSha256 = [string]$row.UcodeSha256
        UcodeSize = [uint64]$row.UcodeSize
        UcodePath = [string]$row.UcodePath
        XeniaUcodePath = [string]$row.XeniaUcodePath
        XeO3HlslPath = [string]$row.HlslPath
    }
}

$results = @(
    $inputs | ForEach-Object -Parallel {
        $shader = $_
        $baseName = '{0}-{1}' -f $shader.Stage, $shader.UcodeSha256
        $hlslPath = Join-Path $using:outputPath "$baseName.hlsl"
        $logPath = Join-Path $using:outputPath "$baseName.log"
        $output = & $using:xenosRecompPath --raw-ucode `
            $shader.UcodePath $shader.Stage $hlslPath 2>&1
        $exitCode = $LASTEXITCODE
        $output | Set-Content -LiteralPath $logPath -Encoding utf8
        $hlslSize = if (Test-Path -LiteralPath $hlslPath -PathType Leaf) {
            (Get-Item -LiteralPath $hlslPath).Length
        } else {
            0
        }
        [pscustomobject]@{
            Stage = $shader.Stage
            UcodeSha256 = $shader.UcodeSha256
            UcodeSize = $shader.UcodeSize
            UcodePath = $shader.UcodePath
            ExitCode = $exitCode
            HlslPath = $hlslPath
            HlslSize = $hlslSize
            LogPath = $logPath
            XeniaUcodePath = $shader.XeniaUcodePath
            XeO3HlslPath = $shader.XeO3HlslPath
        }
    } -ThrottleLimit $ThrottleLimit
)

$failures = @(
    $results | Where-Object {
        $_.ExitCode -ne 0 -or $_.HlslSize -eq 0 -or
        $_.HlslSize -gt (16 * 1024 * 1024)
    }
)
$indexPath = Join-Path $outputPath 'xenosrecomp-index.tsv'
$results |
    Sort-Object Stage,UcodeSha256 |
    Export-Csv -LiteralPath $indexPath -Delimiter "`t" -NoTypeInformation

$summary = [ordered]@{
    xenosrecomp_base_commit = $baseCommit
    xenosrecomp_exe = $xenosRecompPath
    xenosrecomp_exe_sha256 = $compilerHash
    capture_index = $captureIndexPath
    capture_rows = $captureRows.Count
    unique_shaders = $uniqueRows.Count
    vertex_shaders = @($results | Where-Object Stage -eq 'vertex').Count
    pixel_shaders = @($results | Where-Object Stage -eq 'pixel').Count
    failures = $failures.Count
    output_index = $indexPath
}
$summaryPath = Join-Path $outputPath 'summary.json'
$summary | ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath $summaryPath -Encoding utf8
$summary

if ($failures.Count -ne 0) {
    $failedShaders = ($failures | ForEach-Object {
        "$($_.Stage):$($_.UcodeSha256):exit=$($_.ExitCode):size=$($_.HlslSize)"
    }) -join ', '
    throw "XenosRecomp failed for $($failures.Count) shader(s): $failedShaders"
}

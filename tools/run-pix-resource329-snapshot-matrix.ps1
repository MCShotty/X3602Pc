[CmdletBinding()]
param(
    [string]$ReplayPath = 'out\pix\ac6-vpos-exposure-gameplay-export-build\Emu.exe',
    [string]$WorkingDirectory = 'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [string]$OutputDirectory = 'out\pix\resource329-snapshot-matrix-endian0',
    [ValidateRange(10, 180)]
    [int]$ReadinessTimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$replay = (Resolve-Path (Join-Path $repoRoot $ReplayPath)).Path
$workingRoot = (Resolve-Path (Join-Path $repoRoot $WorkingDirectory)).Path
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$captureScript = Join-Path $PSScriptRoot 'capture-process-window.ps1'
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

if (Get-Process -Name Emu -ErrorAction SilentlyContinue) {
    throw 'An Emu process is already running.'
}

$saved = @{
    Warp = $env:PIX_REPLAY_USE_WARP
    EarlyEndian = $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE
    FinalEndian = $env:AC6_PSO397_ENDIAN_OVERRIDE
    Resource242 = $env:AC6_FINAL_SOURCE_RESOURCE242
    Resource246 = $env:AC6_FINAL_SOURCE_RESOURCE246
    Resource329 = $env:AC6_FINAL_SOURCE_RESOURCE329
    Snapshot = $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT
    SnapshotStage = $env:AC6_RESOURCE329_SNAPSHOT_STAGE
}
$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = '1'
    $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO397_ENDIAN_OVERRIDE = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE242 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE246 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = '1'
    foreach ($stage in 1..6) {
        $env:AC6_RESOURCE329_SNAPSHOT_STAGE = [string]$stage
        $startedUtc = [DateTime]::UtcNow
        $process = $null
        try {
            $process = Start-Process `
                -FilePath $replay `
                -WorkingDirectory $workingRoot `
                -WindowStyle Normal `
                -PassThru
            $deadline = $startedUtc.AddSeconds($ReadinessTimeoutSeconds)
            $constantsPath = Join-Path $workingRoot 'pso397-cb0.txt'
            do {
                Start-Sleep -Milliseconds 500
                $process.Refresh()
                if ($process.HasExited) {
                    throw "Replay for snapshot stage $stage exited with code $($process.ExitCode)."
                }
                $constants = Get-Item -LiteralPath $constantsPath -ErrorAction SilentlyContinue
                $constantsReady = $constants -and $constants.LastWriteTimeUtc -ge $startedUtc
                $memoryReady = $process.WorkingSet64 -ge 3000MB
            } while ((-not $constantsReady -or -not $memoryReady) -and [DateTime]::UtcNow -lt $deadline)
            if (-not $constantsReady -or -not $memoryReady) {
                throw "Replay for snapshot stage $stage did not reach the captured frame."
            }

            Start-Sleep -Seconds 2
            $imagePath = Join-Path $outputRoot "stage-$stage.png"
            & powershell.exe `
                -NoProfile `
                -ExecutionPolicy Bypass `
                -File $captureScript `
                -ProcessName Emu `
                -OutputPath $imagePath
            if ($LASTEXITCODE -ne 0) {
                throw "Window capture failed for snapshot stage $stage."
            }
            $results.Add([pscustomobject]@{
                Stage = $stage
                WorkingSetMB = [Math]::Round($process.WorkingSet64 / 1MB, 1)
                Screenshot = $imagePath
                ScreenshotSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $imagePath).Hash
            })
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    $env:PIX_REPLAY_USE_WARP = $saved.Warp
    $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE = $saved.EarlyEndian
    $env:AC6_PSO397_ENDIAN_OVERRIDE = $saved.FinalEndian
    $env:AC6_FINAL_SOURCE_RESOURCE242 = $saved.Resource242
    $env:AC6_FINAL_SOURCE_RESOURCE246 = $saved.Resource246
    $env:AC6_FINAL_SOURCE_RESOURCE329 = $saved.Resource329
    $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = $saved.Snapshot
    $env:AC6_RESOURCE329_SNAPSHOT_STAGE = $saved.SnapshotStage
}

$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding utf8
$results

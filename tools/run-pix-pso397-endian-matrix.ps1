[CmdletBinding()]
param(
    [string]$ReplayPath = 'out\pix\ac6-vpos-exposure-gameplay-export-build\Emu.exe',
    [string]$WorkingDirectory = 'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [string]$OutputDirectory = 'out\pix\pso397-endian-matrix',
    [ValidateSet(0, 1, 2, 3)]
    [int[]]$Mode = @(0, 1, 2, 3),
    [ValidateRange(512, 16384)]
    [int]$MinimumWorkingSetMB = 3000,
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

$oldWarp = $env:PIX_REPLAY_USE_WARP
$oldEndian = $env:AC6_PSO397_ENDIAN_OVERRIDE
$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = '1'
    foreach ($value in $Mode) {
        $env:AC6_PSO397_ENDIAN_OVERRIDE = [string]$value
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
                    throw "Replay for endian mode $value exited with code $($process.ExitCode)."
                }
                $constants = Get-Item -LiteralPath $constantsPath -ErrorAction SilentlyContinue
                $constantsReady = $constants -and $constants.LastWriteTimeUtc -ge $startedUtc
                $memoryReady = $process.WorkingSet64 -ge ($MinimumWorkingSetMB * 1MB)
            } while ((-not $constantsReady -or -not $memoryReady) -and [DateTime]::UtcNow -lt $deadline)

            if (-not $constantsReady -or -not $memoryReady) {
                throw "Replay for endian mode $value did not reach the captured frame."
            }

            Start-Sleep -Seconds 2
            $imagePath = Join-Path $outputRoot "endian-$value.png"
            & powershell.exe `
                -NoProfile `
                -ExecutionPolicy Bypass `
                -File $captureScript `
                -ProcessName Emu `
                -OutputPath $imagePath
            if ($LASTEXITCODE -ne 0) {
                throw "Window capture failed for endian mode $value."
            }

            Copy-Item `
                -LiteralPath $constantsPath `
                -Destination (Join-Path $outputRoot "endian-$value-cb0.txt") `
                -Force
            $results.Add([pscustomobject]@{
                Mode = $value
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
    $env:PIX_REPLAY_USE_WARP = $oldWarp
    $env:AC6_PSO397_ENDIAN_OVERRIDE = $oldEndian
}

$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding utf8
$results

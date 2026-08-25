[CmdletBinding()]
param(
    [string]$ReplayPath = 'out\pix\ac6-vpos-exposure-gameplay-export-build\Emu.exe',
    [string]$WorkingDirectory = 'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [string]$OutputDirectory = 'out\pix\list599-section-matrix',
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
$resolveShader = (Resolve-Path (
    Join-Path $workingRoot 'pso523-resolve-4x-upscale.dxil')).Path
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

if (Get-Process -Name Emu -ErrorAction SilentlyContinue) {
    throw 'An Emu process is already running.'
}

$names = @(
    'PIX_REPLAY_USE_WARP',
    'AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE',
    'AC6_PSO397_ENDIAN_OVERRIDE',
    'AC6_PSO523_RESOLVE_PS',
    'AC6_PSO523_EVENT2051_DIRECT_RESOLVE',
    'AC6_LIST599_MAX_SECTION',
    'AC6_RESOURCE329_SNAPSHOT_STAGE',
    'AC6_FINAL_SOURCE_RESOURCE242',
    'AC6_FINAL_SOURCE_RESOURCE246',
    'AC6_FINAL_SOURCE_RESOURCE329',
    'AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT'
)
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = '1'
    $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO397_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO523_RESOLVE_PS = $resolveShader
    $env:AC6_PSO523_EVENT2051_DIRECT_RESOLVE = '1'
    $env:AC6_RESOURCE329_SNAPSHOT_STAGE = '2'
    $env:AC6_FINAL_SOURCE_RESOURCE242 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE246 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = '1'

    foreach ($section in -1..5) {
        $env:AC6_LIST599_MAX_SECTION = [string]$section
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
                    throw "Replay for list 599 section $section exited with code $($process.ExitCode)."
                }
                $constants = Get-Item -LiteralPath $constantsPath -ErrorAction SilentlyContinue
                $constantsReady = $constants -and
                    $constants.LastWriteTimeUtc -ge $startedUtc
                $memoryReady = $process.WorkingSet64 -ge 3000MB
            } while ((-not $constantsReady -or -not $memoryReady) -and
                     [DateTime]::UtcNow -lt $deadline)
            if (-not $constantsReady -or -not $memoryReady) {
                throw "Replay for list 599 section $section did not reach the captured frame."
            }

            Start-Sleep -Seconds 2
            $label = if ($section -lt 0) { 'init-only' } else { "through-$section" }
            $imagePath = Join-Path $outputRoot "$label.png"
            & powershell.exe `
                -NoProfile `
                -ExecutionPolicy Bypass `
                -File $captureScript `
                -TargetProcessId $process.Id `
                -OutputPath $imagePath
            if ($LASTEXITCODE -ne 0) {
                throw "Window capture failed for list 599 section $section."
            }
            $results.Add([pscustomobject]@{
                Section = $section
                WorkingSetMB = [Math]::Round($process.WorkingSet64 / 1MB, 1)
                Screenshot = $imagePath
                ScreenshotSha256 =
                    (Get-FileHash -Algorithm SHA256 -LiteralPath $imagePath).Hash
            })
        } finally {
            if ($process -and -not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
                Wait-Process -Id $process.Id -ErrorAction SilentlyContinue
            }
        }
    }
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}

$results |
    ConvertTo-Json -Depth 3 |
    Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding utf8
$results

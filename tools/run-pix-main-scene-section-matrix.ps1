[CmdletBinding()]
param(
    [string]$ReplayPath =
        'out\pix\ac6-vpos-exposure-gameplay-export-build\Emu.exe',
    [string]$WorkingDirectory =
        'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [string]$OutputDirectory =
        'out\pix\main-scene-section-matrix',
    [string[]]$Cases = @(
        '0:0',
        '3:0',
        '6:0',
        '9:0',
        '10:0',
        '10:1',
        '10:2'
    ),
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
$loadShader = (Resolve-Path (
    Join-Path $repoRoot 'build\ac6-aot\generated\ac6_edram_load_fix.dxil')).Path
$scaleShader = (Resolve-Path (
    Join-Path $repoRoot 'build\ac6-aot\generated\ac6_edram_scale_fix.dxil')).Path
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

if (Get-Process -Name Emu -ErrorAction SilentlyContinue) {
    throw 'An Emu process is already running.'
}

$environmentNames = @(
    'PIX_REPLAY_USE_WARP',
    'AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE',
    'AC6_PSO397_ENDIAN_OVERRIDE',
    'AC6_PSO523_PS_OVERRIDE',
    'AC6_PSO523_RESOLVE_PS',
    'AC6_PSO523_EVENT2051_DIRECT_RESOLVE',
    'AC6_PSO620_PS_OVERRIDE',
    'AC6_CL526_MAX_SECTION',
    'AC6_CL564_MAX_SECTION',
    'AC6_RESOURCE329_SNAPSHOT_STAGE',
    'AC6_FINAL_SOURCE_RESOURCE242',
    'AC6_FINAL_SOURCE_RESOURCE246',
    'AC6_FINAL_SOURCE_RESOURCE329',
    'AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT'
)
$savedEnvironment = @{}
foreach ($name in $environmentNames) {
    $savedEnvironment[$name] =
        [Environment]::GetEnvironmentVariable($name, 'Process')
}

$results = [Collections.Generic.List[object]]::new()
try {
    $env:PIX_REPLAY_USE_WARP = '1'
    $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO397_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO523_PS_OVERRIDE = $loadShader
    $env:AC6_PSO523_RESOLVE_PS = $null
    $env:AC6_PSO523_EVENT2051_DIRECT_RESOLVE = '0'
    $env:AC6_PSO620_PS_OVERRIDE = $scaleShader
    $env:AC6_RESOURCE329_SNAPSHOT_STAGE = '1'
    $env:AC6_FINAL_SOURCE_RESOURCE242 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE246 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = '1'

    foreach ($case in $Cases) {
        if ($case -notmatch '^(\d+):(\d+)$') {
            throw "Invalid case '$case'; expected CL526:CL564."
        }
        $section526 = [int]$Matches[1]
        $section564 = [int]$Matches[2]
        if ($section526 -gt 10 -or $section564 -gt 2) {
            throw "Invalid case '$case'; ranges are 0..10 and 0..2."
        }

        $env:AC6_CL526_MAX_SECTION = [string]$section526
        $env:AC6_CL564_MAX_SECTION = [string]$section564
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
                    throw "Replay for case $case exited with code $($process.ExitCode)."
                }
                $constants = Get-Item `
                    -LiteralPath $constantsPath `
                    -ErrorAction SilentlyContinue
                $constantsReady = $constants -and
                    $constants.LastWriteTimeUtc -ge $startedUtc
                $memoryReady = $process.WorkingSet64 -ge 3000MB
            } while ((-not $constantsReady -or -not $memoryReady) -and
                     [DateTime]::UtcNow -lt $deadline)
            if (-not $constantsReady -or -not $memoryReady) {
                throw "Replay for case $case did not reach the captured frame."
            }

            Start-Sleep -Seconds 2
            $imagePath = Join-Path $outputRoot (
                "cl526-$section526-cl564-$section564.png")
            & powershell.exe `
                -NoProfile `
                -ExecutionPolicy Bypass `
                -File $captureScript `
                -TargetProcessId $process.Id `
                -OutputPath $imagePath
            if ($LASTEXITCODE -ne 0) {
                throw "Window capture failed for case $case."
            }
            $results.Add([pscustomobject]@{
                CommandList526Section = $section526
                CommandList564Section = $section564
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
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable(
            $name, $savedEnvironment[$name], 'Process')
    }
}

$results |
    ConvertTo-Json -Depth 3 |
    Set-Content `
        -LiteralPath (Join-Path $outputRoot 'manifest.json') `
        -Encoding utf8
$results

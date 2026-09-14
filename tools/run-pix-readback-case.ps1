[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$OutputDirectory,
    [uint32]$CommandList526MaxGlobalId = 1462,
    [uint32]$CommandList564MaxGlobalId = 1513,
    [hashtable]$Overrides = @{},
    [string]$ReplayDirectory = 'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [switch]$UseCapturedShaders,
    [ValidateSet('Captured', 'Warp')]
    [string]$Adapter = 'Captured',
    [switch]$RequireAdapterEvidence,
    [ValidateRange(10, 300)]
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$workingRoot = (Resolve-Path -LiteralPath (Join-Path $repoRoot $ReplayDirectory)).Path
$replay = Join-Path $workingRoot 'Emu.exe'
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
if (Get-Process -Name Emu -ErrorAction SilentlyContinue) {
    throw 'An Emu process is already running.'
}
if (Test-Path -LiteralPath $outputRoot) {
    throw "Use a new output directory to preserve earlier evidence: $outputRoot"
}
[IO.Directory]::CreateDirectory($outputRoot) | Out-Null

$framePath = Join-Path $outputRoot 'frame.rgba'
$adapterPath = Join-Path $outputRoot 'adapter.json'
$environment = [ordered]@{
    # Older generated players ignored this variable and selected the captured
    # AMD adapter. A Warp request now requires fresh device-derived evidence.
    PIX_REPLAY_USE_WARP = if ($Adapter -eq 'Warp') { '1' } else { '0' }
    AC6_REPLAY_ADAPTER_LOG = $adapterPath
    AC6_CL526_MAX_SECTION = '10'
    AC6_CL564_MAX_SECTION = '2'
    AC6_CL526_MAX_GLOBAL_ID = [string]$CommandList526MaxGlobalId
    AC6_CL564_MAX_GLOBAL_ID = [string]$CommandList564MaxGlobalId
    AC6_RESOURCE329_SNAPSHOT_STAGE = '1'
    AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = '1'
}
if (-not $UseCapturedShaders) {
    $environment['AC6_PSO523_PS_OVERRIDE'] = (Resolve-Path -LiteralPath (
        Join-Path $repoRoot 'build\ac6-aot\generated\ac6_edram_load_fix.dxil')).Path
    $environment['AC6_PSO620_PS_OVERRIDE'] = (Resolve-Path -LiteralPath (
        Join-Path $repoRoot 'build\ac6-aot\generated\ac6_edram_scale_fix.dxil')).Path
}
foreach ($name in $Overrides.Keys) {
    if ($name -notmatch '^AC6_[A-Z0-9_]+$') {
        throw "Unsupported replay environment name: $name"
    }
    $environment[$name] = [string]$Overrides[$name]
}
$environment['AC6_REPLAY_SINGLE_FRAME'] = '1'
$environment['AC6_REPLAY_FRAME_DUMP'] = $framePath
$environment['AC6_DUMP_PSO_SHADERS'] = '1'

$shaders = [Collections.Generic.List[object]]::new()
foreach ($name in @($environment.Keys)) {
    if ($name -notmatch '^AC6_PSO(\d+)_(VS|PS|GS|DS|HS)_OVERRIDE$') {
        continue
    }
    $pipeline = $Matches[1]
    $stage = $Matches[2].ToLowerInvariant()
    $shaderPath = $environment[$name]
    if (-not [IO.Path]::IsPathRooted($shaderPath)) {
        $shaderPath = Join-Path $repoRoot $shaderPath
    }
    $shaderPath = (Resolve-Path -LiteralPath $shaderPath).Path
    $environment[$name] = $shaderPath
    $shaders.Add([pscustomobject]@{
        EnvironmentName = $name
        Source = $shaderPath
        ExpectedSha256 = (Get-FileHash -LiteralPath $shaderPath -Algorithm SHA256).Hash
        BoundPath = Join-Path $workingRoot "ac6-pso-shaders\pso-$pipeline-$stage.dxil"
        BoundSha256 = $null
    })
}

$launch = [Diagnostics.ProcessStartInfo]::new()
$launch.FileName = $replay
$launch.WorkingDirectory = $workingRoot
$launch.UseShellExecute = $false
$launch.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
# Ambient experiments must not silently contaminate an A/B run.
foreach ($name in @($launch.Environment.Keys)) {
    if ($name -like 'AC6_*') {
        $launch.Environment.Remove($name) | Out-Null
    }
}
foreach ($name in $environment.Keys) {
    $launch.Environment[$name] = $environment[$name]
}

$startedUtc = [DateTime]::UtcNow
$process = $null
$failure = $null
$exitCode = $null
$adapterEvidence = $null
try {
    $process = [Diagnostics.Process]::Start($launch)
    $deadline = $startedUtc.AddSeconds($TimeoutSeconds)
    while (-not $process.WaitForExit(250)) {
        if ([DateTime]::UtcNow -ge $deadline) {
            throw "Replay process $($process.Id) exceeded $TimeoutSeconds seconds."
        }
    }
    $exitCode = $process.ExitCode
    if ($exitCode -ne 0) {
        throw "Replay exited with code $exitCode."
    }

    # Resource 847 in the pinned capture is 1280 x 960 RGBA8. Its readback
    # happens after the GPU fence, so window visibility and OSDs are irrelevant.
    $frame = Get-Item -LiteralPath $framePath
    if ($frame.LastWriteTimeUtc -lt $startedUtc -or $frame.Length -ne 4915200) {
        throw "Replay did not produce a fresh, complete RGBA8 frame: $($frame.Length) bytes."
    }
    if (Test-Path -LiteralPath $adapterPath) {
        $adapterFile = Get-Item -LiteralPath $adapterPath
        if ($adapterFile.LastWriteTimeUtc -lt $startedUtc) {
            throw 'Replay adapter evidence is stale.'
        }
        $adapterEvidence = Get-Content -LiteralPath $adapterPath -Raw | ConvertFrom-Json
        if ($adapterEvidence.software -isnot [bool] -or
            $adapterEvidence.warp_requested -isnot [bool] -or
            [string]::IsNullOrWhiteSpace($adapterEvidence.description)) {
            throw 'Replay adapter evidence is incomplete.'
        }
        if ($Adapter -eq 'Warp' -and
            (-not $adapterEvidence.software -or -not $adapterEvidence.warp_requested)) {
            throw 'WARP was requested but the actual replay adapter is not verified software.'
        }
    } elseif ($RequireAdapterEvidence -or $Adapter -eq 'Warp') {
        throw 'Replay did not emit device-derived adapter evidence; adapter selection is unverified.'
    }
    foreach ($shader in $shaders) {
        $bound = Get-Item -LiteralPath $shader.BoundPath
        if ($bound.LastWriteTimeUtc -lt $startedUtc) {
            throw "Replay did not record a fresh shader binding: $($shader.EnvironmentName)"
        }
        $shader.BoundSha256 = (Get-FileHash -LiteralPath $bound.FullName -Algorithm SHA256).Hash
        if ($shader.BoundSha256 -ne $shader.ExpectedSha256) {
            throw "Replay bound the wrong shader for $($shader.EnvironmentName)."
        }
        Copy-Item -LiteralPath $bound.FullName -Destination (
            Join-Path $outputRoot $bound.Name)
    }
} catch {
    $failure = $_.Exception.Message
    throw
} finally {
    if ($process -and -not $process.HasExited) {
        $process.Kill()
        $process.WaitForExit()
        $exitCode = $process.ExitCode
    }
    $manifest = [ordered]@{
        StartedUtc = $startedUtc.ToString('o')
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
        ProcessId = if ($process) { $process.Id } else { $null }
        ExitCode = $exitCode
        Failure = $failure
        ReplayPath = $replay
        ReplaySha256 = (Get-FileHash -LiteralPath $replay -Algorithm SHA256).Hash
        RequestedAdapter = $Adapter
        ActualAdapter = $adapterEvidence
        AdapterVerified = $null -ne $adapterEvidence
        UseCapturedShaders = [bool]$UseCapturedShaders
        Environment = $environment
        ShaderBindings = $shaders.ToArray()
        FramePath = $framePath
        FrameWidth = 1280
        FrameHeight = 960
        FrameFormat = 'RGBA8'
        FrameSha256 = if (Test-Path -LiteralPath $framePath) {
            (Get-FileHash -LiteralPath $framePath -Algorithm SHA256).Hash
        } else { $null }
    }
    $manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (
        Join-Path $outputRoot 'manifest.json') -Encoding utf8
}
[pscustomobject]$manifest

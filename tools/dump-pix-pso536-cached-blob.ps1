param(
    [string]$ReplayPath = '.\out\pix\ac6-rcp-cold-gameplay-export-build2\Emu.exe',
    [string]$WorkingDirectory = '.\out\pix\ac6-rcp-cold-gameplay-export',
    [string]$OutputPath = '.\out\pix\pso536-diagnostic\pso536-amd-cached.bin',
    [ValidateRange(1, 120)]
    [int]$TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'

$resolvedReplay = (Resolve-Path -LiteralPath $ReplayPath).Path
$resolvedWorkingDirectory = (Resolve-Path -LiteralPath $WorkingDirectory).Path
$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
Remove-Item -LiteralPath $resolvedOutput -Force -ErrorAction SilentlyContinue

$oldDumpPath = $env:PIX_REPLAY_DUMP_PSO536_CACHED_BLOB
$oldUseWarp = $env:PIX_REPLAY_USE_WARP
$process = $null
try {
    $env:PIX_REPLAY_DUMP_PSO536_CACHED_BLOB = $resolvedOutput
    Remove-Item Env:PIX_REPLAY_USE_WARP -ErrorAction SilentlyContinue
    $process = Start-Process `
        -FilePath $resolvedReplay `
        -WorkingDirectory $resolvedWorkingDirectory `
        -WindowStyle Hidden `
        -PassThru

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline -and
           -not (Test-Path -LiteralPath $resolvedOutput) -and
           -not $process.HasExited) {
        Start-Sleep -Milliseconds 200
        $process.Refresh()
    }
} finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    $env:PIX_REPLAY_DUMP_PSO536_CACHED_BLOB = $oldDumpPath
    $env:PIX_REPLAY_USE_WARP = $oldUseWarp
}

if (-not (Test-Path -LiteralPath $resolvedOutput)) {
    $exitCode = if ($process -and $process.HasExited) {
        $process.ExitCode
    } else {
        '<unavailable>'
    }
    throw "PSO536 cached blob was not produced; replay exit=$exitCode"
}

$output = Get-Item -LiteralPath $resolvedOutput
[pscustomobject]@{
    Path = $output.FullName
    Bytes = $output.Length
    Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $output).Hash
    ReplayPid = $process.Id
    ReplayExit = $process.ExitCode
}

param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [string]$DumpRoot,
    [ValidateSet('MiniPlus', 'Full')]
    [string]$DumpType = 'MiniPlus',
    [switch]$AllUnhandled,
    [switch]$ReuseCompatibleShaderCache
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$procdump = Join-Path $repoRoot '.tools\procdump-12.01\procdump64.exe'
$runScript = Join-Path $PSScriptRoot 'run-ac6-cdb.ps1'
$detachCommands = Join-Path $PSScriptRoot 'ac6-detach-after-aot.wds'

foreach ($path in @($procdump, $runScript, $detachCommands)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required crash-capture input is missing: $path"
    }
}

$signature = Get-AuthenticodeSignature -LiteralPath $procdump
if ($signature.Status -ne 'Valid' -or
    $signature.SignerCertificate.Subject -notmatch 'Microsoft Corporation') {
    throw 'The ProcDump binary does not have a valid Microsoft signature.'
}

$existing = Get-Process -Name Emu, cdb, procdump64 -ErrorAction SilentlyContinue
if ($existing) {
    $summary = $existing |
        ForEach-Object { "$($_.ProcessName):$($_.Id)" }
    throw "Stop existing trace processes before arming capture: $($summary -join ', ')"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $DumpRoot) {
    $DumpRoot = Join-Path $repoRoot "out\diagnostics\$stamp-fastfail"
}
$DumpRoot = [System.IO.Path]::GetFullPath($DumpRoot)
New-Item -ItemType Directory -Path $DumpRoot -Force | Out-Null

$tracePath = Join-Path $LabRoot "ProbeLogs\$stamp-detach-for-fastfail.log"
$runArguments = @{
    LabRoot = $LabRoot
    TracePath = $tracePath
    CommandFile = $detachCommands
}
if ($ReuseCompatibleShaderCache) {
    $runArguments.ReuseCompatibleShaderCache = $true
}
& $runScript @runArguments | Out-File `
    -LiteralPath (Join-Path $DumpRoot 'launcher.log') `
    -Encoding utf8

$emuDeadline = (Get-Date).AddSeconds(15)
do {
    $emu = Get-Process -Name Emu -ErrorAction SilentlyContinue |
        Sort-Object StartTime -Descending |
        Select-Object -First 1
    if (-not $emu) {
        Start-Sleep -Milliseconds 200
    }
} while (-not $emu -and (Get-Date) -lt $emuDeadline)
if (-not $emu) {
    throw 'Emu.exe did not remain visible after the debugger detached.'
}

$dumpSwitch = if ($DumpType -eq 'Full') { '-ma' } else { '-mp' }
$arguments = @('-accepteula', $dumpSwitch, '-e')
if (-not $AllUnhandled) {
    $arguments += @('1', '-f', 'C0000409')
}
$arguments += @('-n', '1', $emu.Id, "`"$DumpRoot`"")

$monitor = $null
$monitorDeadline = (Get-Date).AddSeconds(90)
$monitorAttempt = 0
do {
    $monitorAttempt++
    $stdoutPath = Join-Path $DumpRoot (
        'procdump.attempt-{0}.stdout.log' -f $monitorAttempt
    )
    $stderrPath = Join-Path $DumpRoot (
        'procdump.attempt-{0}.stderr.log' -f $monitorAttempt
    )
    $monitor = Start-Process -FilePath $procdump `
        -ArgumentList $arguments `
        -RedirectStandardOutput $stdoutPath `
        -RedirectStandardError $stderrPath `
        -WindowStyle Hidden `
        -PassThru

    if (-not $monitor.WaitForExit(1500)) {
        break
    }

    $monitorOutput = @(
        Get-Content -LiteralPath $stdoutPath -Raw -ErrorAction SilentlyContinue
        Get-Content -LiteralPath $stderrPath -Raw -ErrorAction SilentlyContinue
    ) -join [Environment]::NewLine
    $monitorOutput = $monitorOutput.Replace("`0", '')
    if ($monitorOutput -notmatch 'already being debugged') {
        throw "ProcDump exited before monitoring Emu.exe: $monitorOutput"
    }
    if ((Get-Date) -ge $monitorDeadline) {
        throw 'ProcDump could not attach within 90 seconds after CDB detached.'
    }
    Start-Sleep -Seconds 2
} while ($true)

$state = [ordered]@{
    armedAt = (Get-Date).ToString('o')
    emuPid = $emu.Id
    emuStartTime = $emu.StartTime.ToString('o')
    procdumpPid = $monitor.Id
    procdumpAttachAttempt = $monitorAttempt
    procdumpStdout = $stdoutPath
    procdumpStderr = $stderrPath
    dumpType = $DumpType
    exceptionFilter = if ($AllUnhandled) { 'all-unhandled' } else { 'C0000409' }
    dumpRoot = $DumpRoot
    detachTrace = $tracePath
    procdumpPath = $procdump
}
$statePath = Join-Path $DumpRoot 'capture-state.json'
$state | ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding utf8

[pscustomobject]$state

[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source = Get-Content -LiteralPath (Join-Path $repoRoot 'tools/invoke-ac6-pix-capture.ps1') -Raw
$start = $source.IndexOf('$deadline = [DateTime]::UtcNow.AddSeconds(')
if ($start -lt 0) { throw 'Production capture-completion block was not found.' }
# Execute the production completion block, without invoking PIX or a process.
$end = $source.IndexOf('$completedCapture = Get-Item -LiteralPath $resolvedOutput', $start)
if ($end -lt 0) { throw 'End of capture-completion block was not found.' }
$completion = [scriptblock]::Create($source.Substring($start, $end - $start) +
    'Get-Item -LiteralPath $resolvedOutput')
$caseRoot = Join-Path $repoRoot ('out/pix/capture-completion-tests-' + [Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($caseRoot) | Out-Null
$results = [Collections.Generic.List[object]]::new()

function Test-CompletionCase([string]$Name, [int]$Bytes, [bool]$HoldWriter, [bool]$ExpectReady) {
    $resolvedOutput = Join-Path $caseRoot ($Name + '.wpix')
    $CaptureTimeoutSeconds = if ($ExpectReady) { 5 } else { 2 }
    $writer = $null
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $ready = $false
    try {
        if ($HoldWriter) {
            $writer = [IO.File]::Open($resolvedOutput, [IO.FileMode]::CreateNew,
                [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
            $writer.SetLength($Bytes)
            $writer.Flush()
        } else {
            [IO.File]::WriteAllBytes($resolvedOutput, [byte[]]::new($Bytes))
        }
        try {
            $result = & $completion
            $ready = $result.Length -eq $Bytes
        } catch {
            if ($_.Exception.Message -notmatch '^PIX capture did not finish within ') { throw }
        }
    } finally {
        if ($writer) { $writer.Dispose() }
        $timer.Stop()
    }
    if ($ready -ne $ExpectReady) { throw "Unexpected completion result for $Name" }
    if ($timer.Elapsed.TotalSeconds -lt 1) { throw "Completion timer returned prematurely for $Name" }
    $results.Add([pscustomobject]@{ Case=$Name; Ready=$ready; Seconds=$timer.Elapsed.TotalSeconds; Passed=$true })
}

Test-CompletionCase 'header-only' 1088 $false $false
Test-CompletionCase 'closed-settled-file' 8192 $false $true
Test-CompletionCase 'open-writer-unchanged-length' 8192 $true $false

$copyCapture = [scriptblock]::Create($source.Substring($end))
$resolvedOutput = Join-Path $caseRoot 'closed-settled-file.wpix'
$durableOutput = Join-Path $caseRoot 'durable-copy.wpix'
$originalHash = (Get-FileHash -LiteralPath $resolvedOutput -Algorithm SHA256).Hash
$copy = & $copyCapture
if ($copy.Length -ne 8192 -or
    (Get-FileHash -LiteralPath $durableOutput -Algorithm SHA256).Hash -ne $originalHash) {
    throw 'Durable copy does not preserve the completed capture.'
}
$overwriteRejected = $false
try { & $copyCapture | Out-Null } catch { $overwriteRejected = $true }
if (-not $overwriteRejected -or
    (Get-FileHash -LiteralPath $durableOutput -Algorithm SHA256).Hash -ne $originalHash) {
    throw 'Durable copy overwrite guard failed.'
}
$results.Add([pscustomobject]@{ Case='durable-copy-and-overwrite-guard'; Ready=$true; Seconds=0; Passed=$true })
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $caseRoot 'results.json') -Encoding utf8
$results

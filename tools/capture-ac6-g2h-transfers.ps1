#requires -Version 7.0
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [int]$TargetProcessId,
    [Parameter(Mandatory)]
    [string]$OutputDirectory,
    [ValidateRange(1, 1024)]
    [int]$MaximumRecords = 16,
    [ValidateRange(0, 16384)]
    [int]$Width = 0,
    [ValidateRange(0, 16384)]
    [int]$Height = 0,
    [ValidateRange(5, 300)]
    [int]$TimeoutSeconds = 30,
    [switch]$IncludeCallStack
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function New-Ac6G2HTraceCommands {
    param(
        [uint64]$ModuleBase,
        [int]$MaximumRecords,
        [int]$Width,
        [int]$Height,
        [bool]$IncludeCallStack
    )
    if ($ModuleBase -eq 0 -or $ModuleBase -gt [uint64]::MaxValue - 0xCC3F -or
        $MaximumRecords -lt 1 -or $MaximumRecords -gt 1024 -or
        $Width -lt 0 -or $Width -gt 16384 -or $Height -lt 0 -or $Height -gt 16384 -or
        (($Width -eq 0) -ne ($Height -eq 0))) {
        throw 'Invalid G2H trace command parameters.'
    }
    # This call site, unlike the generic upload entry, guarantees RDI is the
    # already-read texture metadata and RDX is the assembled 0x80-byte CB0.
    # MASM expressions use bitwise &, not C++ &&, and do not short-circuit.
    $condition = '(dwo(@rdx)==0) & (dwo(@rdx+8)==4) & (dwo(@rdx+0x10)==6)'
    if ($Width -ne 0) {
        $condition += ' & (dwo(@rdi+0x48)==0n{0}) & (dwo(@rdi+0x4c)==0n{1})' -f $Width, $Height
    }
    $commands = @'
sxi av
sxi ld
sxi ud
sxi out
r @$t0 = 0
bp __SITE__ ".if (__CONDITION__) { r @$t0 = @$t0+1; .printf \"AC6_G2H|%x|%x|%x|%x|%x|%x|%x|%x|%x\\n\", @$t0, @$tid, dwo(@rdi+4), dwo(@rdx+4), dwo(@rdx+0xc), dwo(@rdx+0x14), dwo(@rdi+0x48), dwo(@rdi+0x4c), dwo(@rdi+0x14); __STACK__ .if (@$t0>=0n__LIMIT__) { bc *; .detach; q } }; gc"
g
'@
    $site = '0x{0:X16}' -f ($ModuleBase + 0xCC3F)
    $stack = if ($IncludeCallStack) { 'k 0n24;' } else { '' }
    return $commands.Replace('__SITE__', $site).Replace('__CONDITION__', $condition).
        Replace('__STACK__', $stack).Replace('__LIMIT__', [string]$MaximumRecords)
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputDirectory))
$localEvidenceRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'out')) + [IO.Path]::DirectorySeparatorChar
if (-not $outputRoot.StartsWith($localEvidenceRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'G2H capture output must stay inside the ignored local out directory.'
}
if (Test-Path -LiteralPath $outputRoot) { throw 'Use a new capture directory; prior evidence is immutable.' }

$target = Get-Process -Id $TargetProcessId -ErrorAction Stop
if ($target.Path -ne 'D:\Games\AC6 shit\XeO3-AC6-lab\Emu.exe') { throw 'The target is not the lab emulator.' }
$targetStartTime = $target.StartTime
$emuHash = (Get-FileHash -LiteralPath $target.Path -Algorithm SHA256).Hash
if ($emuHash -ne 'D1578E07B533E391D8A81C330D5493BA2D45B252490A818EC148DABE1BA24D06') {
    throw 'The lab emulator hash does not match the pinned host.'
}
$vgpu = @($target.Modules | Where-Object ModuleName -eq 'VGPUDX12.dll')
if ($vgpu.Count -ne 1) { throw 'Exactly one loaded VGPUDX12 module is required.' }
$vgpuHash = (Get-FileHash -LiteralPath $vgpu[0].FileName -Algorithm SHA256).Hash
if ($vgpuHash -ne '8306B4C06B100CAE18F91DCCD0468C2210CCA11A02D928025DE59BD827610247' -or
    $vgpu[0].ModuleMemorySize -ne 0x73E000) {
    throw 'The loaded VGPU does not match the supported 2608 host profile.'
}
$moduleBase = [uint64]$vgpu[0].BaseAddress.ToInt64()
$commandText = New-Ac6G2HTraceCommands -ModuleBase $moduleBase -MaximumRecords $MaximumRecords `
    -Width $Width -Height $Height -IncludeCallStack ([bool]$IncludeCallStack)
$cdb = Get-ChildItem -LiteralPath (Join-Path $repoRoot '.tools') -Directory -Filter 'windbg-amd64-*' |
    Sort-Object Name -Descending | ForEach-Object { Join-Path $_.FullName 'cdb.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $cdb) { throw 'The project-local CDB runtime is missing.' }

if (-not ('Ac6G2HTraceNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class Ac6G2HTraceNative {
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool ReadProcessMemory(IntPtr process, IntPtr address, byte[] bytes, UIntPtr size, out UIntPtr read);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool DebugBreakProcess(IntPtr process);
    [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr handle);
}
'@
}
$targetHandle = [Ac6G2HTraceNative]::OpenProcess(0x1F0FFF, $false, $TargetProcessId)
if ($targetHandle -eq [IntPtr]::Zero) { throw 'Could not open the verified lab process.' }
$debugger = $null
$timedOut = $false
$forcedDebuggerExit = $false
$stdoutTask = $null
$stderrTask = $null
$failure = $null
$callSiteRestored = $false
$startedUtc = [DateTime]::UtcNow
try {
    $bytes = [byte[]]::new(5)
    $read = [UIntPtr]::Zero
    if (-not [Ac6G2HTraceNative]::ReadProcessMemory($targetHandle,
        [IntPtr]::new([int64]($moduleBase + 0xCC3F)), $bytes, [UIntPtr]::new(5), [ref]$read) -or
        $read.ToUInt64() -ne 5 -or [Convert]::ToHexString($bytes) -ne 'E83CC6FFFF') {
        throw 'The live G2H call instruction does not match; no breakpoint was installed.'
    }
    [IO.Directory]::CreateDirectory($outputRoot) | Out-Null
    $commandPath = Join-Path $outputRoot 'capture.wds'
    $logPath = Join-Path $outputRoot 'cdb.log'
    [IO.File]::WriteAllText($commandPath, $commandText, [Text.UTF8Encoding]::new($false))
    $start = [Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $cdb
    $start.WorkingDirectory = $repoRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.WindowStyle = [Diagnostics.ProcessWindowStyle]::Hidden
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in @('-pd', '-p', [string]$TargetProcessId, '-cf', $commandPath, '-logo', $logPath)) {
        $start.ArgumentList.Add($argument)
    }
    $debugger = [Diagnostics.Process]::Start($start)
    # Keep stdin open and drain both outputs. An EOF or full output pipe must
    # not terminate the debuggee or strand it at an unexpected breakpoint.
    $stdoutTask = $debugger.StandardOutput.ReadToEndAsync()
    $stderrTask = $debugger.StandardError.ReadToEndAsync()
    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while (-not $debugger.WaitForExit(250)) {
        if ([DateTime]::UtcNow -ge $deadline) { $timedOut = $true; break }
    }
} catch {
    $failure = $_.Exception.Message
} finally {
    if ($debugger -and -not $debugger.HasExited) {
        try {
            [Ac6G2HTraceNative]::DebugBreakProcess($targetHandle) | Out-Null
            $debugger.StandardInput.WriteLine('bc *; .detach; q')
            $debugger.StandardInput.Flush()
            if (-not $debugger.WaitForExit(5000)) {
                $forcedDebuggerExit = $true
                $debugger.Kill()
                $debugger.WaitForExit()
            }
        } catch {
            $failure = $_.Exception.Message
            if (-not $debugger.HasExited) {
                $forcedDebuggerExit = $true
                $debugger.Kill()
                $debugger.WaitForExit()
            }
        }
    }
    if ($debugger) {
        $restoredBytes = [byte[]]::new(5)
        $restoredRead = [UIntPtr]::Zero
        $callSiteRestored = [Ac6G2HTraceNative]::ReadProcessMemory($targetHandle,
            [IntPtr]::new([int64]($moduleBase + 0xCC3F)), $restoredBytes, [UIntPtr]::new(5), [ref]$restoredRead) -and
            $restoredRead.ToUInt64() -eq 5 -and [Convert]::ToHexString($restoredBytes) -eq 'E83CC6FFFF'
    }
    [Ac6G2HTraceNative]::CloseHandle($targetHandle) | Out-Null
    if ($debugger) {
        [IO.File]::WriteAllText((Join-Path $outputRoot 'stdout.log'), $stdoutTask.GetAwaiter().GetResult())
        [IO.File]::WriteAllText((Join-Path $outputRoot 'stderr.log'), $stderrTask.GetAwaiter().GetResult())
        $records = @()
        try {
        $records = @([IO.File]::ReadLines($logPath) | Where-Object { $_ -match '^AC6_G2H\|' } | ForEach-Object {
            $fields = $_.Split('|')
            if ($fields.Count -ne 10) { throw 'Malformed G2H record.' }
            [pscustomobject]@{
                Sequence = [Convert]::ToUInt32($fields[1], 16)
                ThreadId = [Convert]::ToUInt32($fields[2], 16)
                PhysicalAddress = '0x' + $fields[3].ToUpperInvariant().PadLeft(8, '0')
                Endian = [Convert]::ToUInt32($fields[4], 16)
                Layout = [Convert]::ToUInt32($fields[5], 16)
                DispatchGroups = [Convert]::ToUInt32($fields[6], 16)
                Width = [Convert]::ToUInt32($fields[7], 16)
                Height = [Convert]::ToUInt32($fields[8], 16)
                PackedFormat = '0x' + $fields[9].ToUpperInvariant().PadLeft(8, '0')
            }
        })
        } catch { $failure = $_.Exception.Message }
        $survivor = Get-Process -Id $TargetProcessId -ErrorAction SilentlyContinue
        $targetAlive = $survivor -and $survivor.StartTime -eq $targetStartTime
        if (-not $targetAlive -and -not $failure) { $failure = 'The emulator exited during capture.' }
        if (-not $callSiteRestored -and -not $failure) { $failure = 'The G2H breakpoint site was not restored.' }
        if ($forcedDebuggerExit -and -not $failure) { $failure = 'Debugger required forced termination; capture is not accepted.' }
        if (-not $timedOut -and -not $failure -and
            ($debugger.ExitCode -ne 0 -or $records.Count -ne $MaximumRecords)) {
            $failure = 'Debugger exited without the requested complete capture.'
        }
        $result = [pscustomobject]@{
            StartedUtc = $startedUtc.ToString('o')
            CompletedUtc = [DateTime]::UtcNow.ToString('o')
            ProcessId = $TargetProcessId
            EmuSha256 = $emuHash
            VgpuSha256 = $vgpuHash
            CallSiteRva = '0x0000CC3F'
            CallSiteBytes = 'E83CC6FFFF'
            RequestedRecords = $MaximumRecords
            RecordCount = $records.Count
            TimedOut = $timedOut
            ForcedDebuggerExit = $forcedDebuggerExit
            DebuggerExitCode = $debugger.ExitCode
            TargetStillRunning = $targetAlive
            CallSiteRestored = $callSiteRestored
            Failure = $failure
            Records = $records
        }
        $result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'capture.json')
        $result
        $debugger.Dispose()
    }
}
if ($failure) { throw $failure }

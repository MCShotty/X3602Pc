param(
    [Parameter(Mandatory)]
    [int]$TargetProcessId,
    [ValidateRange(250, 60000)]
    [int]$DurationMilliseconds = 5000,
    [ValidateRange(0, 100)]
    [int]$IntervalMilliseconds = 1,
    [ValidateRange(1, 256)]
    [int]$Top = 40,
    [ValidateRange(1, 32)]
    [int]$BusyThreads = 8,
    [ValidateSet(16, 32, 64, 128, 256, 512, 1024)]
    [int]$BucketBytes = 16,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$source = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading;

public sealed class Ac6HostRipSample
{
    public long SuccessfulSamples;
    public long FailedSamples;
    public Dictionary<ulong, long> AddressCounts = new Dictionary<ulong, long>();
    public Dictionary<int, long> ThreadCounts = new Dictionary<int, long>();
    public Dictionary<string, long> ThreadAddressCounts = new Dictionary<string, long>();
    public Dictionary<int, long> ThreadCpuTicks = new Dictionary<int, long>();
}

public static class Ac6HostRipSamplerNative
{
    private const uint ThreadSuspendResume = 0x0002;
    private const uint ThreadGetContext = 0x0008;
    private const uint ThreadQueryInformation = 0x0040;
    private const uint ContextAmd64 = 0x00100000;
    private const uint ContextControl = ContextAmd64 | 0x00000001;
    private const int ContextFlagsOffset = 0x30;
    private const int RipOffset = 0xF8;
    private const int ContextBytes = 0x4D0;

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenThread(
        uint desiredAccess,
        bool inheritHandle,
        uint threadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint SuspendThread(IntPtr thread);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern uint ResumeThread(IntPtr thread);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetThreadContext(IntPtr thread, IntPtr context);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool GetThreadTimes(
        IntPtr thread,
        out long creationTime,
        out long exitTime,
        out long kernelTime,
        out long userTime);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr handle);

    private sealed class ThreadHandle
    {
        public int Id;
        public IntPtr Handle;
        public long StartCpuTicks;
    }

    public static Ac6HostRipSample Sample(
        int[] threadIds,
        int durationMilliseconds,
        int intervalMilliseconds)
    {
        var handles = new List<ThreadHandle>();
        foreach (var threadId in threadIds)
        {
            var handle = OpenThread(
                ThreadSuspendResume | ThreadGetContext | ThreadQueryInformation,
                false,
                unchecked((uint)threadId));
            if (handle != IntPtr.Zero)
            {
                long creationTime;
                long exitTime;
                long kernelTime;
                long userTime;
                GetThreadTimes(
                    handle,
                    out creationTime,
                    out exitTime,
                    out kernelTime,
                    out userTime);
                handles.Add(new ThreadHandle {
                    Id = threadId,
                    Handle = handle,
                    StartCpuTicks = kernelTime + userTime
                });
            }
        }

        var result = new Ac6HostRipSample();
        var rawContext = Marshal.AllocHGlobal(ContextBytes + 15);
        var alignedValue = (rawContext.ToInt64() + 15L) & ~15L;
        var context = new IntPtr(alignedValue);
        var stopwatch = Stopwatch.StartNew();

        try
        {
            while (stopwatch.ElapsedMilliseconds < durationMilliseconds)
            {
                foreach (var thread in handles)
                {
                    var suspendCount = SuspendThread(thread.Handle);
                    if (suspendCount == UInt32.MaxValue)
                    {
                        result.FailedSamples++;
                        continue;
                    }

                    try
                    {
                        Marshal.WriteInt32(
                            context,
                            ContextFlagsOffset,
                            unchecked((int)ContextControl));
                        if (!GetThreadContext(thread.Handle, context))
                        {
                            result.FailedSamples++;
                            continue;
                        }

                        var address = unchecked((ulong)Marshal.ReadInt64(context, RipOffset));
                        long addressCount;
                        result.AddressCounts.TryGetValue(address, out addressCount);
                        result.AddressCounts[address] = addressCount + 1;

                        long threadCount;
                        result.ThreadCounts.TryGetValue(thread.Id, out threadCount);
                        result.ThreadCounts[thread.Id] = threadCount + 1;

                        var threadAddressKey =
                            thread.Id.ToString() + "|" + address.ToString("X16");
                        long threadAddressCount;
                        result.ThreadAddressCounts.TryGetValue(
                            threadAddressKey,
                            out threadAddressCount);
                        result.ThreadAddressCounts[threadAddressKey] =
                            threadAddressCount + 1;
                        result.SuccessfulSamples++;
                    }
                    finally
                    {
                        ResumeThread(thread.Handle);
                    }
                }

                if (intervalMilliseconds > 0)
                {
                    Thread.Sleep(intervalMilliseconds);
                }
            }
        }
        finally
        {
            Marshal.FreeHGlobal(rawContext);
            foreach (var thread in handles)
            {
                long creationTime;
                long exitTime;
                long kernelTime;
                long userTime;
                if (GetThreadTimes(
                        thread.Handle,
                        out creationTime,
                        out exitTime,
                        out kernelTime,
                        out userTime))
                {
                    result.ThreadCpuTicks[thread.Id] =
                        Math.Max(0, kernelTime + userTime - thread.StartCpuTicks);
                }
                CloseHandle(thread.Handle);
            }
        }

        return result;
    }
}
'@

if (-not ('Ac6HostRipSamplerNative' -as [type])) {
    Add-Type -TypeDefinition $source
}

$targetProcess = Get-Process -Id $TargetProcessId -ErrorAction Stop
$threadIds = @($targetProcess.Threads | ForEach-Object Id)
$modules = @(
    $targetProcess.Modules |
        ForEach-Object {
            [pscustomobject]@{
                Name = $_.ModuleName
                Path = $_.FileName
                Base = [uint64]$_.BaseAddress.ToInt64()
                End = [uint64]$_.BaseAddress.ToInt64() + [uint64]$_.ModuleMemorySize
                Size = [uint64]$_.ModuleMemorySize
            }
        } |
        Sort-Object Base
)

$sample = [Ac6HostRipSamplerNative]::Sample(
    $threadIds,
    $DurationMilliseconds,
    $IntervalMilliseconds)

$busyThreadIds = @(
    $sample.ThreadCpuTicks.GetEnumerator() |
        Sort-Object Value -Descending |
        Select-Object -First $BusyThreads |
        ForEach-Object { [int]$_.Key }
)
$filteredSamples = [int64]0
$buckets = @{}
$moduleCounts = @{}
foreach ($entry in $sample.ThreadAddressCounts.GetEnumerator()) {
    $separator = $entry.Key.IndexOf('|')
    $threadId = [int]$entry.Key.Substring(0, $separator)
    if ($threadId -notin $busyThreadIds) {
        continue
    }

    $address = [Convert]::ToUInt64($entry.Key.Substring($separator + 1), 16)
    $count = [int64]$entry.Value
    $filteredSamples += $count
    $module = $modules |
        Where-Object { $address -ge $_.Base -and $address -lt $_.End } |
        Select-Object -First 1

    if ($module) {
        $moduleName = $module.Name
        $rva = $address - $module.Base
        $bucketRva = $rva - ($rva % [uint64]$BucketBytes)
        $bucketKey = '{0}|{1}|{2:X}' -f $threadId, $moduleName, $bucketRva
        $bucket = [pscustomobject]@{
            ThreadId = $threadId
            Module = $moduleName
            Path = $module.Path
            Rva = '0x{0:X}' -f $bucketRva
            Address = '0x{0:X}' -f ($module.Base + $bucketRva)
            Samples = [int64]0
        }
    } else {
        $moduleName = '<unmapped>'
        $bucketAddress = $address - ($address % [uint64]$BucketBytes)
        $bucketKey = '{0}|{1}|{2:X}' -f $threadId, $moduleName, $bucketAddress
        $bucket = [pscustomobject]@{
            ThreadId = $threadId
            Module = $moduleName
            Path = $null
            Rva = $null
            Address = '0x{0:X}' -f $bucketAddress
            Samples = [int64]0
        }
    }

    if (-not $buckets.ContainsKey($bucketKey)) {
        $buckets[$bucketKey] = $bucket
    }
    $buckets[$bucketKey].Samples += $count

    if (-not $moduleCounts.ContainsKey($moduleName)) {
        $moduleCounts[$moduleName] = [int64]0
    }
    $moduleCounts[$moduleName] += $count
}

$moduleRows = @(
    $moduleCounts.GetEnumerator() |
        ForEach-Object {
            [pscustomobject]@{
                Module = $_.Key
                Samples = [int64]$_.Value
                SharePercent = [math]::Round(
                    100.0 * $_.Value / [math]::Max(1, $filteredSamples),
                    3)
            }
        } |
        Sort-Object Samples -Descending
)
$hotspotRows = @(
    $buckets.Values |
        Sort-Object Samples -Descending |
        Select-Object -First $Top |
        ForEach-Object {
            $_ | Add-Member -PassThru NoteProperty SharePercent (
                [math]::Round(
                    100.0 * $_.Samples / [math]::Max(1, $filteredSamples),
                    3))
        }
)
$totalCpuTicks = [int64](
    $sample.ThreadCpuTicks.Values |
        Measure-Object -Sum |
        Select-Object -ExpandProperty Sum
)
$threadRows = @(
    $sample.ThreadCounts.GetEnumerator() |
        ForEach-Object {
            [pscustomobject]@{
                ThreadId = [int]$_.Key
                Samples = [int64]$_.Value
                CpuMilliseconds = [math]::Round(
                    $sample.ThreadCpuTicks[[int]$_.Key] / 10000.0,
                    3)
                CpuSharePercent = [math]::Round(
                    100.0 * $sample.ThreadCpuTicks[[int]$_.Key] /
                        [math]::Max(1, $totalCpuTicks),
                    3)
                SharePercent = [math]::Round(
                    100.0 * $_.Value / [math]::Max(1, $sample.SuccessfulSamples),
                    3)
            }
        } |
        Sort-Object CpuMilliseconds -Descending
)

$report = [ordered]@{
    Timestamp = [DateTimeOffset]::Now.ToString('O')
    ProcessId = $targetProcess.Id
    ProcessStartTime = $targetProcess.StartTime.ToString('O')
    DurationMilliseconds = $DurationMilliseconds
    IntervalMilliseconds = $IntervalMilliseconds
    BucketBytes = $BucketBytes
    ThreadCount = $threadIds.Count
    BusyThreadCount = $busyThreadIds.Count
    FilteredSamples = $filteredSamples
    SuccessfulSamples = $sample.SuccessfulSamples
    FailedSamples = $sample.FailedSamples
    TotalCpuMilliseconds = [math]::Round($totalCpuTicks / 10000.0, 3)
    Modules = $moduleRows
    Threads = $threadRows
    Hotspots = $hotspotRows
}

if ($OutputPath) {
    $resolvedOutput = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
        $OutputPath)
    $outputDirectory = Split-Path -Parent $resolvedOutput
    if ($outputDirectory) {
        New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    }
    $report | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resolvedOutput
}

$moduleRows | Format-Table -AutoSize
$hotspotRows | Format-Table -AutoSize

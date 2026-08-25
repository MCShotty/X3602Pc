param(
    [Parameter(Mandatory)]
    [int]$ProcessId,
    [ValidateRange(1, 256)]
    [int]$MaximumMatches = 64,
    [switch]$IncludePrivateMemory,
    [ValidateRange(1, 2048)]
    [int]$MaximumRegionMiB = 512
)

$ErrorActionPreference = 'Stop'

if (-not ('Ac6Transfer341MemoryScanner' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

public static class Ac6Transfer341MemoryScanner
{
    [StructLayout(LayoutKind.Sequential)]
    private struct MemoryBasicInformation
    {
        public IntPtr BaseAddress;
        public IntPtr AllocationBase;
        public uint AllocationProtect;
        public UIntPtr RegionSize;
        public uint State;
        public uint Protect;
        public uint Type;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(
        uint desiredAccess, bool inheritHandle, int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern UIntPtr VirtualQueryEx(
        IntPtr process, IntPtr address,
        out MemoryBasicInformation information, UIntPtr length);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadProcessMemory(
        IntPtr process, IntPtr address, byte[] buffer, UIntPtr size,
        out UIntPtr bytesRead);

    private const uint ProcessVmRead = 0x0010;
    private const uint ProcessQueryInformation = 0x0400;
    private const uint MemCommit = 0x1000;
    private const uint MemPrivate = 0x20000;
    private const uint MemMapped = 0x40000;
    private const uint PageNoAccess = 0x01;
    private const uint PageGuard = 0x100;
    private const int ChunkSize = 4 * 1024 * 1024;

    private static int[] BuildSkipTable(byte[] pattern)
    {
        var table = new int[256];
        for (var index = 0; index < table.Length; ++index)
            table[index] = pattern.Length;
        for (var index = 0; index + 1 < pattern.Length; ++index)
            table[pattern[index]] = pattern.Length - index - 1;
        return table;
    }

    private static void FindMatches(
        byte[] buffer, int length, byte[] pattern, int[] skip,
        ulong address, List<ulong> matches, int maximumMatches)
    {
        var index = 0;
        var last = pattern.Length - 1;
        while (index + pattern.Length <= length &&
               matches.Count < maximumMatches)
        {
            var patternIndex = last;
            while (patternIndex >= 0 &&
                   buffer[index + patternIndex] == pattern[patternIndex])
                --patternIndex;
            if (patternIndex < 0)
            {
                matches.Add(address + (ulong)index);
                index += pattern.Length;
            }
            else
            {
                index += skip[buffer[index + last]];
            }
        }
    }

    private static bool IsWritable(uint protection)
    {
        switch (protection & 0xFF)
        {
            case 0x04:
            case 0x08:
            case 0x40:
            case 0x80:
                return true;
            default:
                return false;
        }
    }

    public static ulong[] Scan(int processId, byte[] pattern,
                               int maximumMatches, bool includePrivateMemory,
                               ulong maximumRegionSize)
    {
        if (pattern == null || pattern.Length == 0)
            throw new ArgumentException("Pattern is empty.", "pattern");

        var process = OpenProcess(
            ProcessVmRead | ProcessQueryInformation, false, processId);
        if (process == IntPtr.Zero)
            throw new Win32Exception(Marshal.GetLastWin32Error());

        try
        {
            var matches = new List<ulong>();
            var skip = BuildSkipTable(pattern);
            var buffer = new byte[ChunkSize + pattern.Length - 1];
            ulong address = 0;
            const ulong maximumAddress = 0x00007FFFFFFFFFFFUL;
            var informationSize = (UIntPtr)(uint)Marshal.SizeOf(
                typeof(MemoryBasicInformation));

            while (address < maximumAddress &&
                   matches.Count < maximumMatches)
            {
                MemoryBasicInformation information;
                var queried = VirtualQueryEx(
                    process, (IntPtr)(long)address, out information,
                    informationSize);
                if (queried == UIntPtr.Zero)
                    break;

                var regionStart = (ulong)information.BaseAddress.ToInt64();
                var regionSize = information.RegionSize.ToUInt64();
                var regionEnd = regionStart + regionSize;
                if (regionEnd <= address)
                    break;

                var readable = information.State == MemCommit &&
                    (information.Protect & (PageNoAccess | PageGuard)) == 0 &&
                    IsWritable(information.Protect) &&
                    regionSize <= maximumRegionSize &&
                    (information.Type == MemMapped ||
                     (includePrivateMemory && information.Type == MemPrivate));
                if (readable)
                {
                    ulong offset = 0;
                    while (offset < regionSize &&
                           matches.Count < maximumMatches)
                    {
                        var requested = (int)Math.Min(
                            (ulong)ChunkSize, regionSize - offset);
                        UIntPtr bytesRead;
                        var ok = ReadProcessMemory(
                            process,
                            (IntPtr)(long)(regionStart + offset),
                            buffer, (UIntPtr)(uint)requested, out bytesRead);
                        var read = ok ? checked((int)bytesRead.ToUInt64()) : 0;
                        if (read > 0)
                        {
                            FindMatches(
                                buffer, read, pattern, skip,
                                regionStart + offset, matches,
                                maximumMatches);
                        }
                        offset += (ulong)requested;
                    }
                }
                address = regionEnd;
            }
            return matches.ToArray();
        }
        finally
        {
            CloseHandle(process);
        }
    }
}
'@
}

$patternWords = [uint32[]]@(1280, 720, 0x02D00000, 1, 5120, 0, 0, 0)
$pattern = New-Object byte[] ($patternWords.Length * 4)
[Buffer]::BlockCopy($patternWords, 0, $pattern, 0, $pattern.Length)

[Ac6Transfer341MemoryScanner]::Scan(
    $ProcessId,
    $pattern,
    $MaximumMatches,
    $IncludePrivateMemory.IsPresent,
    [uint64]$MaximumRegionMiB * 1MB
) | ForEach-Object {
    [pscustomobject]@{
        ProcessId = $ProcessId
        Address = '0x{0:X16}' -f $_
        AddressValue = $_
    }
}

param(
    [int]$TargetProcessId = 0,
    [ValidateRange(100, 60000)]
    [int]$DurationMilliseconds = 5000,
    [ValidateRange(1, 229)]
    [int]$Top = 20,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$importsPath = Join-Path $PSScriptRoot '..\out\ac6\generated\ac6_imports.inc'
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'

foreach ($requiredPath in @($dllPath, $importsPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required input is missing: $requiredPath"
    }
}

if ($TargetProcessId -eq 0) {
    $targetProcess = Get-Process -Name Emu -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0 |
        Select-Object -First 1
} else {
    $targetProcess = Get-Process -Id $TargetProcessId -ErrorAction Stop
}

$module = $targetProcess.Modules |
    Where-Object ModuleName -eq $dllName |
    Select-Object -First 1
if (-not $module) {
    throw "The AC6 AOT DLL is not loaded in process $($targetProcess.Id)."
}

$exportText = (& $readObj --coff-exports $dllPath) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "llvm-readobj failed with exit code $LASTEXITCODE."
}
$exportMatch = [regex]::Match(
    $exportText,
    'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*BridgeLastImportThunk\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)')
if (-not $exportMatch.Success) {
    throw 'BridgeLastImportThunk is not exported by the deployed AOT DLL.'
}
$exportRva = [Convert]::ToInt64($exportMatch.Groups['rva'].Value, 16)
$address = $module.BaseAddress.ToInt64() + $exportRva

$source = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;

public sealed class Ac6ImportSampleResult
{
    public long TotalReads;
    public long FailedReads;
    public Dictionary<uint, long> Counts = new Dictionary<uint, long>();
}

public static class Ac6ImportSamplerNative
{
    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr OpenProcess(
        uint desiredAccess,
        bool inheritHandle,
        int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool ReadProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        out uint buffer,
        UIntPtr size,
        out UIntPtr bytesRead);

    [DllImport("kernel32.dll")]
    private static extern bool CloseHandle(IntPtr handle);

    public static Ac6ImportSampleResult Sample(
        int processId,
        long address,
        int durationMilliseconds)
    {
        const uint ProcessVmRead = 0x0010;
        const uint ProcessQueryInformation = 0x0400;
        var process = OpenProcess(
            ProcessVmRead | ProcessQueryInformation,
            false,
            processId);
        if (process == IntPtr.Zero)
        {
            throw new System.ComponentModel.Win32Exception(
                Marshal.GetLastWin32Error(),
                "OpenProcess failed");
        }

        var result = new Ac6ImportSampleResult();
        var stopwatch = Stopwatch.StartNew();
        try
        {
            while (stopwatch.ElapsedMilliseconds < durationMilliseconds)
            {
                uint thunk;
                UIntPtr bytesRead;
                if (!ReadProcessMemory(
                        process,
                        new IntPtr(address),
                        out thunk,
                        new UIntPtr(4),
                        out bytesRead) ||
                    bytesRead.ToUInt64() != 4)
                {
                    result.FailedReads++;
                    continue;
                }

                result.TotalReads++;
                long count;
                result.Counts.TryGetValue(thunk, out count);
                result.Counts[thunk] = count + 1;
            }
        }
        finally
        {
            CloseHandle(process);
        }
        return result;
    }
}
'@

if (-not ('Ac6ImportSamplerNative' -as [type])) {
    Add-Type -TypeDefinition $source
}

$imports = @{}
$importPattern =
    '^XEO3_AC6_IMPORT\((?<index>\d+),\s*0X(?<thunk>[0-9A-Fa-f]+)u,\s*(?<name>[^)]+)\)$'
foreach ($line in Get-Content -LiteralPath $importsPath) {
    if ($line -match $importPattern) {
        $thunk = [Convert]::ToUInt32($Matches.thunk, 16)
        $imports[$thunk] = [pscustomobject]@{
            Index = [int]$Matches.index
            Name = $Matches.name
        }
    }
}

$sample = [Ac6ImportSamplerNative]::Sample(
    $targetProcess.Id,
    $address,
    $DurationMilliseconds)
if ($sample.FailedReads -ne 0) {
    Write-Warning "$($sample.FailedReads) ReadProcessMemory calls failed."
}

$rows = foreach ($entry in $sample.Counts.GetEnumerator()) {
    $definition = $imports[[uint32]$entry.Key]
    [pscustomobject]@{
        Samples = [int64]$entry.Value
        SharePercent = [math]::Round(
            100.0 * $entry.Value / [math]::Max(1, $sample.TotalReads),
            3)
        Index = if ($definition) { $definition.Index } else { $null }
        Thunk = '0x{0:X8}' -f [uint32]$entry.Key
        Name = if ($definition) { $definition.Name } else { '<unknown>' }
    }
}

$rows |
    Sort-Object Samples -Descending |
    Select-Object -First $Top

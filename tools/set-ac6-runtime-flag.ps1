param(
    [Parameter(Mandatory)]
    [ValidateSet(
        'BridgeFastCriticalSectionsEnabled',
        'BridgeFastSpinLocksEnabled',
        'BridgeFastSynchronizationTelemetryEnabled',
        'BridgeFastIrqlEnabled',
        'BridgeFastIrqlTelemetryEnabled',
        'BridgeIndirectTelemetryEnabled',
        'BridgeIndirectStateSyncEnabled',
        'BridgeIntegerImportSyncEnabled',
        'BridgeThreadImportTraceEnabled',
        'BridgeAc6AudioPollClockFallbackEnabled',
        'BridgeAc6AudioPollTraceEnabled',
        'BridgeVdSwapTraceEnabled',
        'BridgeVgpuG2HTraceEnabled',
        'BridgeGuestIarTelemetryEnabled',
        'BridgeSynchronousQueueEnabled')]
    [string]$ExportName,
    [ValidateSet(0, 1)]
    [int]$Enabled = 1,
    [int]$TargetProcessId = 0,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
foreach ($requiredPath in @($dllPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required input is missing: $requiredPath"
    }
}

$targetProcess = if ($TargetProcessId -ne 0) {
    Get-Process -Id $TargetProcessId -ErrorAction Stop
} else {
    Get-Process -Name Emu -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0 |
        Select-Object -First 1
}
if (-not $targetProcess) {
    throw 'No running Emu process was found.'
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
$escapedExportName = [regex]::Escape($ExportName)
$exportPattern =
    "Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*$escapedExportName\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)"
$match = [regex]::Match($exportText, $exportPattern)
if (-not $match.Success) {
    throw "The deployed AOT DLL does not export $ExportName."
}
$exportRva = [Convert]::ToUInt64($match.Groups['rva'].Value, 16)

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6RuntimeFlagMemoryNative
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        uint desiredAccess,
        bool inheritHandle,
        int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool WriteProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        byte[] buffer,
        UIntPtr size,
        out UIntPtr bytesWritten);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        byte[] buffer,
        UIntPtr size,
        out UIntPtr bytesRead);

    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@

$processAccess = 0x0008 -bor 0x0010 -bor 0x0020 -bor 0x0400
$processHandle = [Ac6RuntimeFlagMemoryNative]::OpenProcess(
    $processAccess,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

try {
    $address = [IntPtr]($module.BaseAddress.ToInt64() + [int64]$exportRva)
    $writeBuffer = [BitConverter]::GetBytes([uint32]$Enabled)
    $bytesWritten = [UIntPtr]::Zero
    $writeResult = [Ac6RuntimeFlagMemoryNative]::WriteProcessMemory(
        $processHandle,
        $address,
        $writeBuffer,
        [UIntPtr]::new([uint64]$writeBuffer.Length),
        [ref]$bytesWritten)
    if (-not $writeResult -or
        $bytesWritten.ToUInt64() -ne $writeBuffer.Length) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "WriteProcessMemory failed with error $errorCode."
    }

    $readBuffer = [byte[]]::new(4)
    $bytesRead = [UIntPtr]::Zero
    $readResult = [Ac6RuntimeFlagMemoryNative]::ReadProcessMemory(
        $processHandle,
        $address,
        $readBuffer,
        [UIntPtr]::new(4),
        [ref]$bytesRead)
    if (-not $readResult -or $bytesRead.ToUInt64() -ne 4) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "ReadProcessMemory failed with error $errorCode."
    }

    [pscustomobject]@{
        ProcessId = $targetProcess.Id
        Export = $ExportName
        Address = '0x{0:X16}' -f $address.ToInt64()
        Enabled = [BitConverter]::ToUInt32($readBuffer, 0)
    }
} finally {
    [void][Ac6RuntimeFlagMemoryNative]::CloseHandle($processHandle)
}

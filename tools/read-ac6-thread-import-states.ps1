param(
    [int]$TargetProcessId = 0,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'

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
$pattern =
    'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*BridgeThreadImportStates\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)'
$match = [regex]::Match($exportText, $pattern)
if (-not $match.Success) {
    throw 'The deployed AOT DLL does not export BridgeThreadImportStates.'
}
$exportRva = [Convert]::ToUInt64($match.Groups['rva'].Value, 16)

if (-not ('Ac6ThreadImportMemoryNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6ThreadImportMemoryNative
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        uint desiredAccess,
        bool inheritHandle,
        int processId);

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
}

$stateSize = 96
$stateCount = 64
$buffer = [byte[]]::new($stateSize * $stateCount)
$processHandle = [Ac6ThreadImportMemoryNative]::OpenProcess(
    0x0010 -bor 0x0400,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    throw "OpenProcess failed with error $(
        [Runtime.InteropServices.Marshal]::GetLastWin32Error())."
}

try {
    $bytesRead = [UIntPtr]::Zero
    $address = [IntPtr](
        $module.BaseAddress.ToInt64() + [int64]$exportRva)
    if (-not [Ac6ThreadImportMemoryNative]::ReadProcessMemory(
            $processHandle,
            $address,
            $buffer,
            [UIntPtr]::new([uint64]$buffer.Length),
            [ref]$bytesRead) -or
        $bytesRead.ToUInt64() -ne $buffer.Length) {
        throw "ReadProcessMemory failed with error $(
            [Runtime.InteropServices.Marshal]::GetLastWin32Error())."
    }
} finally {
    [void][Ac6ThreadImportMemoryNative]::CloseHandle($processHandle)
}

for ($index = 0; $index -lt $stateCount; $index++) {
    $offset = $index * $stateSize
    $threadId = [BitConverter]::ToUInt32($buffer, $offset + 32)
    if ($threadId -eq 0) {
        continue
    }

    [pscustomobject]@{
        Slot = $index
        Sequence = [BitConverter]::ToUInt64($buffer, $offset)
        ImportCount = [BitConverter]::ToUInt64($buffer, $offset + 8)
        CpuState = '0x{0:X16}' -f
            [BitConverter]::ToUInt64($buffer, $offset + 16)
        GuestMemory = '0x{0:X16}' -f
            [BitConverter]::ToUInt64($buffer, $offset + 24)
        HostThreadId = $threadId
        LastGuestIar = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 36)
        LastThunk = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 40)
        LastTarget = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 44)
        CallerLr = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 48)
        R1 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 52)
        R3 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 56)
        R4 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 60)
        R5 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 64)
        R6 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 68)
        R7 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 72)
        R8 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 76)
        R9 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 80)
        R10 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 84)
        R13 = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 88)
        Result = '0x{0:X8}' -f
            [BitConverter]::ToUInt32($buffer, $offset + 92)
    }
}

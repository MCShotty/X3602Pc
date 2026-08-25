param(
    [Parameter(Mandatory = $true)]
    [ValidateRange(0, 256)]
    [int]$Selector,
    [ValidateSet(
        'Restore',
        'HostTransfer',
        'HostRestoreSkip',
        'FullscreenScissor',
        'VposScale',
        'Exposure',
        'EdramScale',
        'EdramLoad',
        'TextureEndianEnabled',
        'TextureEndianBudget',
        'TextureEndianReplacement',
        'Transfer341Width'
    )]
    [string]$Experiment = 'Restore',
    [int]$TargetProcessId = 0,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
foreach ($requiredPath in @($dllPath, $readObj)) {
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
$exportName = switch ($Experiment) {
    'Restore' { 'BridgeVgpuEdramRestoreExperimentSelector' }
    'HostTransfer' { 'BridgeVgpuHostTransferExperimentSelector' }
    'HostRestoreSkip' { 'BridgeVgpuHostEdramRestoreDrawSkipEnabled' }
    'FullscreenScissor' { 'BridgeVgpuFullscreenScissorFixEnabled' }
    'VposScale' { 'BridgeVgpuVposScaleFixEnabled' }
    'Exposure' { 'BridgeVgpuExposureFixEnabled' }
    'EdramScale' { 'BridgeVgpuEdramScaleFixEnabled' }
    'EdramLoad' { 'BridgeVgpuEdramLoadFixEnabled' }
    'TextureEndianEnabled' { 'BridgeVgpuTextureEndianFixEnabled' }
    'TextureEndianBudget' { 'BridgeVgpuTextureEndianFixBudget' }
    'TextureEndianReplacement' { 'BridgeVgpuTextureEndianReplacement' }
    'Transfer341Width' { 'BridgeVgpuTransfer341WidthFixEnabled' }
}
$exportPattern =
    "Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*$exportName\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)"
$match = [regex]::Match($exportText, $exportPattern)
if (-not $match.Success) {
    throw "The deployed AOT DLL does not export $exportName."
}
$exportRva = [Convert]::ToUInt64($match.Groups['rva'].Value, 16)

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6VgpuExperimentMemoryNative
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

$processVmOperation = 0x0008
$processVmRead = 0x0010
$processVmWrite = 0x0020
$processQueryInformation = 0x0400
$processHandle = [Ac6VgpuExperimentMemoryNative]::OpenProcess(
    $processVmOperation -bor $processVmRead -bor $processVmWrite -bor
        $processQueryInformation,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

try {
    $address = [IntPtr]($module.BaseAddress.ToInt64() + [int64]$exportRva)
    $buffer = [BitConverter]::GetBytes([uint32]$Selector)
    $bytesWritten = [UIntPtr]::Zero
    if (-not [Ac6VgpuExperimentMemoryNative]::WriteProcessMemory(
            $processHandle,
            $address,
            $buffer,
            [UIntPtr]::new([uint64]$buffer.Length),
            [ref]$bytesWritten) -or
        $bytesWritten.ToUInt64() -ne $buffer.Length) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "WriteProcessMemory failed with error $errorCode."
    }

    $readBuffer = [byte[]]::new(4)
    $bytesRead = [UIntPtr]::Zero
    if (-not [Ac6VgpuExperimentMemoryNative]::ReadProcessMemory(
            $processHandle,
            $address,
            $readBuffer,
            [UIntPtr]::new(4),
            [ref]$bytesRead) -or
        $bytesRead.ToUInt64() -ne 4) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "ReadProcessMemory failed with error $errorCode."
    }

    [pscustomobject]@{
        ProcessId = $targetProcess.Id
        Address = ('0x{0:X16}' -f $address.ToInt64())
        Experiment = $Experiment
        Selector = [BitConverter]::ToUInt32($readBuffer, 0)
    }
} finally {
    [void][Ac6VgpuExperimentMemoryNative]::CloseHandle($processHandle)
}

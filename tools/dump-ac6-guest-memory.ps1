param(
    [Parameter(Mandatory = $true)]
    [uint32]$GuestAddress,
    [Parameter(Mandatory = $true)]
    [ValidateRange(1, 67108864)]
    [int]$Length,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath,
    [int]$TargetProcessId = 0,
    [uint64]$HostBaseOverride = 0,
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
$match = [regex]::Match(
    $exportText,
    'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*BridgeLastGuestMemory\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)')
if (-not $match.Success) {
    throw 'The deployed AOT DLL does not export BridgeLastGuestMemory.'
}
$exportRva = [Convert]::ToUInt64($match.Groups['rva'].Value, 16)

if (-not ('Ac6GuestMemoryNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6GuestMemoryNative
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

$processVmRead = 0x0010
$processQueryInformation = 0x0400
$processHandle = [Ac6GuestMemoryNative]::OpenProcess(
    $processVmRead -bor $processQueryInformation,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

try {
    $basePointerBytes = [byte[]]::new(8)
    $bytesRead = [UIntPtr]::Zero
    $basePointerAddress = [IntPtr](
        $module.BaseAddress.ToInt64() + [int64]$exportRva)
    if (-not [Ac6GuestMemoryNative]::ReadProcessMemory(
            $processHandle,
            $basePointerAddress,
            $basePointerBytes,
            [UIntPtr]::new(8),
            [ref]$bytesRead) -or
        $bytesRead.ToUInt64() -ne 8) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "Reading BridgeLastGuestMemory failed with error $errorCode."
    }

    $publishedGuestMemoryBase = [BitConverter]::ToUInt64($basePointerBytes, 0)
    $guestMemoryBase = if ($HostBaseOverride -ne 0) {
        $HostBaseOverride
    } else {
        $publishedGuestMemoryBase
    }
    if ($guestMemoryBase -eq 0) {
        throw 'BridgeLastGuestMemory has not been published yet.'
    }
    $hostAddress = $guestMemoryBase + [uint64]$GuestAddress
    if ($hostAddress -gt [uint64][long]::MaxValue) {
        throw 'The resolved host address is outside the supported pointer range.'
    }

    $buffer = [byte[]]::new($Length)
    $bytesRead = [UIntPtr]::Zero
    if (-not [Ac6GuestMemoryNative]::ReadProcessMemory(
            $processHandle,
            [IntPtr][long]$hostAddress,
            $buffer,
            [UIntPtr]::new([uint64]$buffer.Length),
            [ref]$bytesRead) -or
        $bytesRead.ToUInt64() -ne [uint64]$buffer.Length) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "Reading guest memory failed with error $errorCode."
    }

    $resolvedOutputPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath(
        $OutputPath)
    $outputDirectory = Split-Path -Parent $resolvedOutputPath
    if ($outputDirectory) {
        New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
    }
    [IO.File]::WriteAllBytes($resolvedOutputPath, $buffer)

    [pscustomobject]@{
        ProcessId = $targetProcess.Id
        PublishedGuestMemoryBase = ('0x{0:X16}' -f $publishedGuestMemoryBase)
        GuestMemoryBase = ('0x{0:X16}' -f $guestMemoryBase)
        GuestAddress = ('0x{0:X8}' -f $GuestAddress)
        HostAddress = ('0x{0:X16}' -f $hostAddress)
        Length = $buffer.Length
        OutputPath = $resolvedOutputPath
        Sha256 = (Get-FileHash -LiteralPath $resolvedOutputPath -Algorithm SHA256).Hash
    }
} finally {
    [void][Ac6GuestMemoryNative]::CloseHandle($processHandle)
}

param(
    [Parameter(Mandatory)]
    [bool]$Enabled,
    [ValidateSet(0, 1, 3)]
    [uint32]$Replacement = 0,
    [uint32]$Budget = [uint32]::MaxValue,
    [int]$TargetProcessId = 0,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
foreach ($requiredPath in @($dllPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Required runtime-control input is missing: $requiredPath"
    }
}

$targetProcess = if ($TargetProcessId -eq 0) {
    Get-Process -Name Emu -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0 |
        Select-Object -First 1
} else {
    Get-Process -Id $TargetProcessId -ErrorAction Stop
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
function Get-ExportRva {
    param([Parameter(Mandatory)][string]$Name)

    $exportPattern =
        'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*' +
        [regex]::Escape($Name) + '\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)'
    $exportMatch = [regex]::Match($exportText, $exportPattern)
    if (-not $exportMatch.Success) {
        throw "The deployed AOT DLL does not export $Name."
    }
    [Convert]::ToUInt64($exportMatch.Groups['rva'].Value, 16)
}

$gateRva = Get-ExportRva -Name 'BridgeVgpuTextureEndianFixEnabled'
$budgetRva = Get-ExportRva -Name 'BridgeVgpuTextureEndianFixBudget'
$replacementRva = Get-ExportRva -Name 'BridgeVgpuTextureEndianReplacement'

if (-not ('Ac6RuntimeControlNative' -as [type])) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6RuntimeControlNative
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

    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@
}

$processVmOperation = 0x0008
$processVmWrite = 0x0020
$processQueryInformation = 0x0400
$processHandle = [Ac6RuntimeControlNative]::OpenProcess(
    $processVmOperation -bor $processVmWrite -bor $processQueryInformation,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

try {
    function Write-RemoteUInt32 {
        param(
            [Parameter(Mandatory)][uint64]$Rva,
            [Parameter(Mandatory)][uint32]$Value
        )

        $buffer = [BitConverter]::GetBytes($Value)
        $bytesWritten = [UIntPtr]::Zero
        $address = [IntPtr]($module.BaseAddress.ToInt64() + [int64]$Rva)
        if (-not [Ac6RuntimeControlNative]::WriteProcessMemory(
                $processHandle,
                $address,
                $buffer,
                [UIntPtr]::new([uint64]$buffer.Length),
                [ref]$bytesWritten) -or
            $bytesWritten.ToUInt64() -ne $buffer.Length) {
            $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
            throw "WriteProcessMemory failed with error $errorCode."
        }
        $address
    }

    $gateAddress = Write-RemoteUInt32 -Rva $gateRva -Value 0
    if ($Enabled) {
        $replacementAddress = Write-RemoteUInt32 `
            -Rva $replacementRva `
            -Value $Replacement
        $budgetAddress = Write-RemoteUInt32 -Rva $budgetRva -Value $Budget
        [void](Write-RemoteUInt32 -Rva $gateRva -Value 1)
    }

    [pscustomobject]@{
        ProcessId = $targetProcess.Id
        BridgeVgpuTextureEndianFixEnabled = [uint32]$Enabled
        BridgeVgpuTextureEndianReplacement = if ($Enabled) {
            $Replacement
        } else {
            $null
        }
        BridgeVgpuTextureEndianFixBudget = if ($Enabled) {
            $Budget
        } else {
            $null
        }
        GateAddress = ('0x{0:X16}' -f $gateAddress.ToInt64())
        ReplacementAddress = if ($Enabled) {
            '0x{0:X16}' -f $replacementAddress.ToInt64()
        } else {
            $null
        }
        BudgetAddress = if ($Enabled) {
            '0x{0:X16}' -f $budgetAddress.ToInt64()
        } else {
            $null
        }
    }
}
finally {
    [void][Ac6RuntimeControlNative]::CloseHandle($processHandle)
}

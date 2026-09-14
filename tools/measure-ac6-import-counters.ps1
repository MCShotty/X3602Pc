param(
    [int]$TargetProcessId = 0,
    [ValidateRange(1, 60)]
    [int]$DurationSeconds = 5,
    [ValidateRange(1, 229)]
    [int]$Top = 30,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$importsPath = Join-Path $PSScriptRoot '..\out\ac6\generated\ac6_imports.inc'
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
foreach ($requiredPath in @($dllPath, $importsPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required import-counter input is missing: $requiredPath"
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
$exportRvas = @{}
$exportPattern =
    'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*(?<name>\S+)\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)'
foreach ($match in [regex]::Matches($exportText, $exportPattern)) {
    $exportRvas[$match.Groups['name'].Value] =
        [Convert]::ToUInt64($match.Groups['rva'].Value, 16)
}
foreach ($name in @(
    'BridgeImportCountersEnabled',
    'BridgeImportCounterCount',
    'BridgeImportCounts')) {
    if (-not $exportRvas.ContainsKey($name)) {
        throw "The deployed AOT DLL does not export $name."
    }
}

$imports = @(
    foreach ($line in Get-Content -LiteralPath $importsPath) {
        if ($line -match '^XEO3_AC6_IMPORT\((?<index>\d+),\s*0X(?<thunk>[0-9A-Fa-f]+)u,\s*(?<name>[A-Za-z0-9_]+)\)') {
            [pscustomobject]@{
                Index = [int]$Matches.index
                Thunk = [Convert]::ToUInt32($Matches.thunk, 16)
                Name = $Matches.name
            }
        }
    }
)

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6ImportCounterNative
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

function Read-TargetBytes {
    param(
        [IntPtr]$Handle,
        [IntPtr]$Address,
        [int]$Length
    )
    $buffer = [byte[]]::new($Length)
    $bytesRead = [UIntPtr]::Zero
    $result = [Ac6ImportCounterNative]::ReadProcessMemory(
        $Handle,
        $Address,
        $buffer,
        [UIntPtr]::new([uint64]$Length),
        [ref]$bytesRead)
    if (-not $result -or $bytesRead.ToUInt64() -ne $Length) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "ReadProcessMemory failed at $Address with error $errorCode."
    }
    return $buffer
}

function Write-TargetBytes {
    param(
        [IntPtr]$Handle,
        [IntPtr]$Address,
        [byte[]]$Buffer
    )
    $bytesWritten = [UIntPtr]::Zero
    $result = [Ac6ImportCounterNative]::WriteProcessMemory(
        $Handle,
        $Address,
        $Buffer,
        [UIntPtr]::new([uint64]$Buffer.Length),
        [ref]$bytesWritten)
    if (-not $result -or $bytesWritten.ToUInt64() -ne $Buffer.Length) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "WriteProcessMemory failed at $Address with error $errorCode."
    }
}

$processAccess = 0x0010 -bor 0x0020 -bor 0x0008 -bor 0x0400
$processHandle = [Ac6ImportCounterNative]::OpenProcess(
    $processAccess,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

$moduleBase = $module.BaseAddress.ToInt64()
$enabledAddress = [IntPtr](
    $moduleBase + [int64]$exportRvas.BridgeImportCountersEnabled)
$countAddress = [IntPtr](
    $moduleBase + [int64]$exportRvas.BridgeImportCounterCount)
$countsAddress = [IntPtr](
    $moduleBase + [int64]$exportRvas.BridgeImportCounts)
$disabled = [BitConverter]::GetBytes([uint32]0)
$enabled = [BitConverter]::GetBytes([uint32]1)
$stopwatch = [Diagnostics.Stopwatch]::new()

try {
    $counterCountBytes = Read-TargetBytes $processHandle $countAddress 4
    $counterCount = [BitConverter]::ToUInt32($counterCountBytes, 0)
    if ($counterCount -ne $imports.Count) {
        throw "Counter/import mismatch: DLL=$counterCount generated=$($imports.Count)."
    }
    for ($index = 0; $index -lt $imports.Count; ++$index) {
        if ($imports[$index].Index -ne $index) {
            throw "Generated import index $($imports[$index].Index) is not contiguous at $index."
        }
    }

    Write-TargetBytes $processHandle $enabledAddress $disabled
    Start-Sleep -Milliseconds 50
    $zeroCounts = [byte[]]::new([int]$counterCount * 8)
    Write-TargetBytes -Handle $processHandle -Address $countsAddress -Buffer $zeroCounts
    $stopwatch.Start()
    Write-TargetBytes $processHandle $enabledAddress $enabled
    Start-Sleep -Milliseconds ($DurationSeconds * 1000)
    Write-TargetBytes $processHandle $enabledAddress $disabled
    $stopwatch.Stop()
    Start-Sleep -Milliseconds 50

    $countsBuffer = Read-TargetBytes -Handle $processHandle -Address $countsAddress -Length ([int]$counterCount * 8)
    $elapsedSeconds = $stopwatch.Elapsed.TotalSeconds
    [uint64]$totalCount = 0
    $measurements = for ($index = 0; $index -lt $counterCount; ++$index) {
        $count = [BitConverter]::ToUInt64($countsBuffer, $index * 8)
        $totalCount += $count
        [pscustomobject]@{
            Index = $index
            Thunk = ('0x{0:X8}' -f $imports[$index].Thunk)
            Name = $imports[$index].Name
            Count = $count
            PerSecond = [Math]::Round($count / $elapsedSeconds, 2)
        }
    }

    [pscustomobject]@{
        ProcessId = $targetProcess.Id
        DurationSeconds = [Math]::Round($elapsedSeconds, 3)
        TotalImports = $totalCount
        ImportsPerSecond = [Math]::Round($totalCount / $elapsedSeconds, 2)
        TopImports = @(
            $measurements |
                Where-Object Count -ne 0 |
                Sort-Object Count -Descending |
                Select-Object -First $Top
        )
    }
} finally {
    try {
        Write-TargetBytes $processHandle $enabledAddress $disabled
    } catch {
    }
    [void][Ac6ImportCounterNative]::CloseHandle($processHandle)
}

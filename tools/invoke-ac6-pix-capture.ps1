param(
    [int]$TargetProcessId = 0,
    [string]$OutputPath = 'out\pix\ac6-frame.wpix',
    [ValidateRange(1, 16)]
    [uint32]$FrameCount = 1,
    [ValidateRange(15, 300)]
    [int]$CaptureTimeoutSeconds = 120,
    [string]$PixToolPath =
        'C:\Program Files\Microsoft PIX\2603.25\pixtool.exe'
)

$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
foreach ($requiredPath in @($PixToolPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required PIX capture input is missing: $requiredPath"
    }
}

$resolvedOutput = if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath
} else {
    Join-Path $repoRoot $OutputPath
}
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
if (Test-Path -LiteralPath $resolvedOutput) {
    throw "Refusing to overwrite an existing PIX capture: $resolvedOutput"
}
if ($outputDirectory) {
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
}
$durableOutput = $resolvedOutput
# Treat CaptureNextFrame's file as session-owned. Return an independent,
# verified copy so PIX's session lifecycle cannot invalidate the caller's file.
$resolvedOutput = Join-Path $outputDirectory (
    '.ac6-pix-session-' + [Guid]::NewGuid().ToString('N') + '.wpix')

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

$pixModule = $targetProcess.Modules |
    Where-Object ModuleName -eq 'WinPixGpuCapturer.dll' |
    Select-Object -First 1
if (-not $pixModule) {
    throw (
        'WinPixGpuCapturer.dll is not loaded in the target. ' +
        'Launch XeO3 through run-ac6.ps1 -PixToolPath first.'
    )
}

$pixCapturerPath = $pixModule.FileName
if (-not (Test-Path -LiteralPath $pixCapturerPath)) {
    $pixCapturerPath = Join-Path `
        ([System.IO.Path]::GetDirectoryName($PixToolPath)) `
        'WinPixGpuCapturer.dll'
}
$exportText = (& $readObj --coff-exports $pixCapturerPath) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "llvm-readobj failed with exit code $LASTEXITCODE."
}
$exportMatch = [regex]::Match(
    $exportText,
    'Name:\s*CaptureNextFrame\s+RVA:\s*0x(?<rva>[0-9A-Fa-f]+)')
if (-not $exportMatch.Success) {
    throw 'WinPixGpuCapturer.dll does not export CaptureNextFrame.'
}
$captureNextFrameRva = [Convert]::ToUInt32(
    $exportMatch.Groups['rva'].Value,
    16)

if (-not ('Ac6PixRemoteCapture' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

public static class Ac6PixRemoteCapture
{
    const uint ProcessCreateThread = 0x0002;
    const uint ProcessQueryInformation = 0x0400;
    const uint ProcessVmOperation = 0x0008;
    const uint ProcessVmWrite = 0x0020;
    const uint ProcessVmRead = 0x0010;
    const uint MemCommit = 0x1000;
    const uint MemReserve = 0x2000;
    const uint PageReadWrite = 0x04;
    const uint PageExecuteRead = 0x20;
    const uint WaitObject0 = 0;

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr OpenProcess(
        uint access,
        bool inherit,
        int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr VirtualAllocEx(
        IntPtr process,
        IntPtr address,
        UIntPtr size,
        uint allocationType,
        uint protection);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool VirtualProtectEx(
        IntPtr process,
        IntPtr address,
        UIntPtr size,
        uint newProtection,
        out uint oldProtection);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool WriteProcessMemory(
        IntPtr process,
        IntPtr address,
        byte[] buffer,
        UIntPtr size,
        out UIntPtr written);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool FlushInstructionCache(
        IntPtr process,
        IntPtr address,
        UIntPtr size);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern IntPtr CreateRemoteThread(
        IntPtr process,
        IntPtr attributes,
        UIntPtr stackSize,
        IntPtr startAddress,
        IntPtr parameter,
        uint flags,
        out uint threadId);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    static extern bool GetExitCodeThread(IntPtr thread, out uint exitCode);

    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    static void Check(bool result, string operation)
    {
        if (!result)
        {
            throw new InvalidOperationException(
                operation + " failed with Win32 error " +
                Marshal.GetLastWin32Error());
        }
    }

    public static uint CaptureNextFrames(
        int processId,
        long moduleBase,
        uint captureNextFrameRva,
        string outputPath,
        uint frameCount)
    {
        uint access =
            ProcessCreateThread |
            ProcessQueryInformation |
            ProcessVmOperation |
            ProcessVmWrite |
            ProcessVmRead;
        IntPtr process = OpenProcess(access, false, processId);
        if (process == IntPtr.Zero)
        {
            throw new InvalidOperationException(
                "OpenProcess failed with Win32 error " +
                Marshal.GetLastWin32Error());
        }

        try
        {
            byte[] path = Encoding.Unicode.GetBytes(outputPath + "\0");
            int codeOffset = (path.Length + 15) & ~15;
            UIntPtr allocationSize =
                new UIntPtr((uint)(codeOffset + 64));
            IntPtr allocation = VirtualAllocEx(
                process,
                IntPtr.Zero,
                allocationSize,
                MemCommit | MemReserve,
                PageReadWrite);
            if (allocation == IntPtr.Zero)
            {
                throw new InvalidOperationException(
                    "VirtualAllocEx failed with Win32 error " +
                    Marshal.GetLastWin32Error());
            }

            UIntPtr written;
            Check(
                WriteProcessMemory(
                    process,
                    allocation,
                    path,
                    new UIntPtr((uint)path.Length),
                    out written) &&
                    written.ToUInt64() == (ulong)path.Length,
                "WriteProcessMemory(path)");

            long functionAddress =
                checked(moduleBase + captureNextFrameRva);
            var code = new List<byte>();
            code.AddRange(new byte[] { 0x48, 0x83, 0xEC, 0x28 });
            code.AddRange(new byte[] { 0x48, 0xB9 });
            code.AddRange(BitConverter.GetBytes(allocation.ToInt64()));
            code.Add(0xBA);
            code.AddRange(BitConverter.GetBytes(frameCount));
            code.AddRange(new byte[] { 0x48, 0xB8 });
            code.AddRange(BitConverter.GetBytes(functionAddress));
            code.AddRange(
                new byte[] {
                    0xFF, 0xD0,
                    0x48, 0x83, 0xC4, 0x28,
                    0xC3
                });
            byte[] codeBytes = code.ToArray();
            IntPtr codeAddress =
                new IntPtr(allocation.ToInt64() + codeOffset);
            Check(
                WriteProcessMemory(
                    process,
                    codeAddress,
                    codeBytes,
                    new UIntPtr((uint)codeBytes.Length),
                    out written) &&
                    written.ToUInt64() == (ulong)codeBytes.Length,
                "WriteProcessMemory(code)");

            uint oldProtection;
            Check(
                VirtualProtectEx(
                    process,
                    codeAddress,
                    new UIntPtr((uint)codeBytes.Length),
                    PageExecuteRead,
                    out oldProtection),
                "VirtualProtectEx");
            Check(
                FlushInstructionCache(
                    process,
                    codeAddress,
                    new UIntPtr((uint)codeBytes.Length)),
                "FlushInstructionCache");

            uint threadId;
            IntPtr thread = CreateRemoteThread(
                process,
                IntPtr.Zero,
                UIntPtr.Zero,
                codeAddress,
                IntPtr.Zero,
                0,
                out threadId);
            if (thread == IntPtr.Zero)
            {
                throw new InvalidOperationException(
                    "CreateRemoteThread failed with Win32 error " +
                    Marshal.GetLastWin32Error());
            }

            try
            {
                uint wait = WaitForSingleObject(thread, 10000);
                if (wait != WaitObject0)
                {
                    throw new InvalidOperationException(
                        "Capture thread wait failed: 0x" +
                        wait.ToString("X8"));
                }
                uint exitCode;
                Check(
                    GetExitCodeThread(thread, out exitCode),
                    "GetExitCodeThread");
                return exitCode;
            }
            finally
            {
                CloseHandle(thread);
            }
        }
        finally
        {
            CloseHandle(process);
        }
    }
}
'@
}

$result = [Ac6PixRemoteCapture]::CaptureNextFrames(
    $targetProcess.Id,
    $pixModule.BaseAddress.ToInt64(),
    $captureNextFrameRva,
    $resolvedOutput,
    $FrameCount)
if ($result -ne 0) {
    throw "CaptureNextFrame failed with HRESULT 0x$($result.ToString('X8'))."
}

$deadline = [DateTime]::UtcNow.AddSeconds($CaptureTimeoutSeconds)
$lastLength = [long]-1
$lastWriteUtc = [DateTime]::MinValue
$stableSinceUtc = [DateTime]::MinValue
$captureReady = $false
do {
    # PIX creates a 1088-byte header before recording the frame. Existence and
    # length > 1024 used to report success while the capture was still empty.
    # Require a settled file and an open that excludes concurrent writers.
    if (Test-Path -LiteralPath $resolvedOutput) {
        $capture = Get-Item -LiteralPath $resolvedOutput
        if ($capture.Length -gt 4096) {
            if ($capture.Length -eq $lastLength -and
                $capture.LastWriteTimeUtc -eq $lastWriteUtc) {
                if (([DateTime]::UtcNow - $stableSinceUtc).TotalSeconds -ge 1) {
                    $stream = $null
                    try {
                        $stream = [IO.File]::Open($resolvedOutput,
                            [IO.FileMode]::Open, [IO.FileAccess]::Read,
                            [IO.FileShare]::Read)
                        $captureReady = $stream.Length -eq $lastLength
                    } catch [IO.IOException] {
                        # A writer may still own the file even if its size has
                        # not changed during this polling interval.
                    } finally {
                        if ($stream) { $stream.Dispose() }
                    }
                    if ($captureReady) { break }
                }
            } else {
                $lastLength = $capture.Length
                $lastWriteUtc = $capture.LastWriteTimeUtc
                $stableSinceUtc = [DateTime]::UtcNow
            }
        }
    }
    Start-Sleep -Milliseconds 250
} while ([DateTime]::UtcNow -lt $deadline)
if (-not $captureReady) {
    throw "PIX capture did not finish within $CaptureTimeoutSeconds seconds. The target is left running for inspection."
}

$completedCapture = Get-Item -LiteralPath $resolvedOutput
$captureStream = $null
$durableStream = $null
$captureHasher = $null
$captureHash = $null
try {
    $captureStream = [IO.File]::Open($resolvedOutput, [IO.FileMode]::Open,
        [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $durableStream = [IO.File]::Open($durableOutput, [IO.FileMode]::CreateNew,
        [IO.FileAccess]::Write, [IO.FileShare]::None)
    $captureStream.CopyTo($durableStream)
    $durableStream.Flush($true)
    $captureStream.Position = 0
    $captureHasher = [Security.Cryptography.SHA256]::Create()
    $captureHash = [BitConverter]::ToString(
        $captureHasher.ComputeHash($captureStream)).Replace('-', '')
} finally {
    if ($captureHasher) { $captureHasher.Dispose() }
    if ($durableStream) { $durableStream.Dispose() }
    if ($captureStream) { $captureStream.Dispose() }
}
$durableCapture = Get-Item -LiteralPath $durableOutput
if ($durableCapture.Length -ne $completedCapture.Length -or
    $captureHash -ne
    (Get-FileHash -LiteralPath $durableOutput -Algorithm SHA256).Hash) {
    throw "Durable PIX capture verification failed: $durableOutput"
}
$durableCapture

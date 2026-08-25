param(
    [Parameter(Mandatory, ParameterSetName = 'Name')]
    [string]$ProcessName,
    [Parameter(Mandatory, ParameterSetName = 'Id')]
    [int]$TargetProcessId,
    [Parameter(Mandatory)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class WindowCaptureNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);

    [DllImport("user32.dll")]
    public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
}
'@

$process = if ($PSCmdlet.ParameterSetName -eq 'Id') {
    Get-Process -Id $TargetProcessId -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0
} else {
    Get-Process -Name $ProcessName -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0 |
        Select-Object -First 1
}
if (-not $process) {
    $target = if ($PSCmdlet.ParameterSetName -eq 'Id') {
        "process $TargetProcessId"
    } else {
        $ProcessName
    }
    throw "No visible $target window was found."
}

$rect = New-Object WindowCaptureNative+Rect
if (-not [WindowCaptureNative]::GetWindowRect(
        $process.MainWindowHandle,
        [ref]$rect)) {
    throw "GetWindowRect failed for process $($process.Id)."
}

$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    throw "Invalid window bounds ${width}x${height}."
}

$bitmap = New-Object System.Drawing.Bitmap $width, $height
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
try {
    $hdc = $graphics.GetHdc()
    try {
        if (-not [WindowCaptureNative]::PrintWindow(
                $process.MainWindowHandle,
                $hdc,
                2)) {
            throw "PrintWindow failed for process $($process.Id)."
        }
    } finally {
        $graphics.ReleaseHdc($hdc)
    }

    $outputDirectory = Split-Path -Parent $OutputPath
    if ($outputDirectory) {
        New-Item -ItemType Directory -Path $outputDirectory -Force |
            Out-Null
    }
    # GDI+ can reject an in-place save while a previous capture still has the
    # destination open. Encode to a unique sibling first, then replace the
    # requested artifact after the bitmap has been fully serialized.
    $temporaryPath = Join-Path $outputDirectory (
        '.' + [System.IO.Path]::GetFileName($OutputPath) + '.' +
            [Guid]::NewGuid().ToString('N') + '.tmp.png')
    try {
        $bitmap.Save(
            $temporaryPath,
            [System.Drawing.Imaging.ImageFormat]::Png)
        $copied = $false
        foreach ($attempt in 1..20) {
            try {
                [System.IO.File]::Copy($temporaryPath, $OutputPath, $true)
                $copied = $true
                break
            } catch [System.UnauthorizedAccessException] {
                Start-Sleep -Milliseconds 100
            } catch [System.IO.IOException] {
                Start-Sleep -Milliseconds 100
            }
        }
        if (-not $copied) {
            throw "Unable to publish capture after retrying: $OutputPath"
        }
    } finally {
        if (Test-Path -LiteralPath $temporaryPath) {
            Remove-Item -LiteralPath $temporaryPath -Force -ErrorAction SilentlyContinue
        }
    }
} finally {
    $graphics.Dispose()
    $bitmap.Dispose()
}

Get-Item -LiteralPath $OutputPath

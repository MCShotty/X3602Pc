param(
    [int]$TargetProcessId,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$target = if ($TargetProcessId) {
    Get-Process -Id $TargetProcessId -ErrorAction Stop
} else {
    Get-Process -Name Emu -ErrorAction Stop |
        Where-Object {
            $_.Path -eq 'D:\Games\AC6 shit\XeO3-AC6-lab\Emu.exe'
        } |
        Select-Object -First 1
}
if (-not $target -or $target.MainWindowHandle -eq [IntPtr]::Zero) {
    throw 'The AC6 emulator window is unavailable.'
}

if (-not $OutputPath) {
    $repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
    $OutputPath = Join-Path $repoRoot (
        'out\ac6\screen-{0}.png' -f (
            Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
}
$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6DesktopCaptureNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Rect
    {
        public int Left;
        public int Top;
        public int Right;
        public int Bottom;
    }

    [DllImport("user32.dll", SetLastError = true)]
    public static extern bool GetWindowRect(IntPtr window, out Rect rect);
}
'@

$rect = [Ac6DesktopCaptureNative+Rect]::new()
if (-not [Ac6DesktopCaptureNative]::GetWindowRect(
        $target.MainWindowHandle,
        [ref]$rect)) {
    throw "GetWindowRect failed with error $(
        [Runtime.InteropServices.Marshal]::GetLastWin32Error())."
}
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    throw "The AC6 window has invalid bounds ${width}x${height}."
}

$bitmap = [Drawing.Bitmap]::new($width, $height)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
try {
    $graphics.CopyFromScreen(
        $rect.Left,
        $rect.Top,
        0,
        0,
        [Drawing.Size]::new($width, $height))
} finally {
    $graphics.Dispose()
}

try {
    $bitmap.Save(
        [IO.Path]::GetFullPath($OutputPath),
        [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}

[IO.Path]::GetFullPath($OutputPath)

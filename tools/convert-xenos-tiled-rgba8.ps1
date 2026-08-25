[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$InputPath,

    [Parameter(Mandatory)]
    [string]$OutputPath,

    [Parameter(Mandatory)]
    [ValidateRange(1, 16384)]
    [int]$Width,

    [Parameter(Mandatory)]
    [ValidateRange(1, 16384)]
    [int]$Height,

    [Parameter(Mandatory)]
    [ValidateRange(32, 16384)]
    [int]$Pitch,

    [ValidateSet('Tiled', 'Linear')]
    [string]$Layout = 'Tiled',

    [ValidateSet(0, 1, 2, 3)]
    [int]$Endian = 0
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

if (($Pitch -band 31) -ne 0) {
    throw "Pitch must be aligned to 32 pixels."
}
if ($Pitch -lt $Width) {
    throw "Pitch must be at least the image width."
}

Add-Type @'
using System;

public static class XenosRgba8Converter
{
    private static long Tiled2D(int x, int y, int pitch)
    {
        long outerBlocks = (((y >> 5) * (pitch >> 5)) + (x >> 5)) << 6;
        long innerBlocks = (((y >> 1) & 7) << 3) | (x & 7);
        long outerInnerBytes = (outerBlocks | innerBlocks) << 2;
        long bank = (y >> 4) & 1;
        long pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
        long yLsb = y & 1;
        return ((yLsb << 4) | (pipe << 6) | (bank << 11)) |
               (outerInnerBytes & 15) |
               (((outerInnerBytes >> 4) & 1) << 5) |
               (((outerInnerBytes >> 5) & 7) << 8) |
               ((outerInnerBytes >> 8) << 12);
    }

    private static uint SwapEndian(uint value, int mode)
    {
        switch (mode)
        {
            case 0:
                return value;
            case 1:
                return ((value & 0x00FF00FFu) << 8) |
                       ((value & 0xFF00FF00u) >> 8);
            case 2:
                return ((value & 0x000000FFu) << 24) |
                       ((value & 0x0000FF00u) << 8) |
                       ((value & 0x00FF0000u) >> 8) |
                       ((value & 0xFF000000u) >> 24);
            case 3:
                return ((value & 0x0000FFFFu) << 16) |
                       ((value & 0xFFFF0000u) >> 16);
            default:
                throw new ArgumentOutOfRangeException("mode");
        }
    }

    public static byte[] Convert(
        byte[] input,
        int width,
        int height,
        int pitch,
        bool tiled,
        int endian,
        out long maximumSourceOffset)
    {
        byte[] bgra = new byte[checked(width * height * 4)];
        maximumSourceOffset = 0;
        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                long sourceOffset = tiled
                    ? Tiled2D(x, y, pitch)
                    : ((long)y * pitch + x) * 4;
                if (sourceOffset < 0 || sourceOffset + 4 > input.LongLength)
                {
                    throw new ArgumentOutOfRangeException(
                        "input",
                        String.Format(
                            "Source offset 0x{0:X} exceeds {1} bytes.",
                            sourceOffset,
                            input.LongLength));
                }
                if (sourceOffset > maximumSourceOffset)
                {
                    maximumSourceOffset = sourceOffset;
                }

                uint value = SwapEndian(
                    BitConverter.ToUInt32(input, checked((int)sourceOffset)),
                    endian);
                int destinationOffset = checked((y * width + x) * 4);
                bgra[destinationOffset] = (byte)(value >> 16);
                bgra[destinationOffset + 1] = (byte)(value >> 8);
                bgra[destinationOffset + 2] = (byte)value;
                bgra[destinationOffset + 3] = (byte)(value >> 24);
            }
        }
        return bgra;
    }
}
'@

$inputBytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $InputPath))
$maximumSourceOffset = 0L
$bgra = [XenosRgba8Converter]::Convert(
    $inputBytes,
    $Width,
    $Height,
    $Pitch,
    $Layout -eq 'Tiled',
    $Endian,
    [ref]$maximumSourceOffset)

Add-Type -AssemblyName System.Drawing
$bitmap = [Drawing.Bitmap]::new(
    $Width,
    $Height,
    [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$rectangle = [Drawing.Rectangle]::new(0, 0, $Width, $Height)
$bitmapData = $bitmap.LockBits(
    $rectangle,
    [Drawing.Imaging.ImageLockMode]::WriteOnly,
    [Drawing.Imaging.PixelFormat]::Format32bppArgb)
try {
    [Runtime.InteropServices.Marshal]::Copy(
        $bgra,
        0,
        $bitmapData.Scan0,
        $bgra.Length)
} finally {
    $bitmap.UnlockBits($bitmapData)
}

$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
$outputDirectory = Split-Path -Parent $resolvedOutput
if ($outputDirectory) {
    [IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
}
try {
    $bitmap.Save($resolvedOutput, [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
}

[pscustomobject]@{
    Input = (Resolve-Path -LiteralPath $InputPath).Path
    Output = $resolvedOutput
    Width = $Width
    Height = $Height
    Pitch = $Pitch
    Layout = $Layout
    Endian = $Endian
    MaximumSourceOffset = $maximumSourceOffset
    OutputSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedOutput).Hash
}

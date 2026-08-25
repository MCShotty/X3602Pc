[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$CommonHeaderPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$sourcePath = (Resolve-Path -LiteralPath $CommonHeaderPath).Path
$source = [IO.File]::ReadAllText($sourcePath)
$declaration =
    'ByteAddressBuffer IndexBuffer:         register(t34);'
if ($source.IndexOf($declaration, [StringComparison]::Ordinal) -lt 0) {
    throw "Expected XeO3 index-buffer declaration is missing."
}

$functionStart = $source.IndexOf(
    'uint FetchIndexBuffer(uint hostIndex)',
    [StringComparison]::Ordinal)
if ($functionStart -lt 0) {
    throw "Expected XeO3 FetchIndexBuffer function is missing."
}
$functionEnd = $source.IndexOf(
    "`n}",
    $functionStart,
    [StringComparison]::Ordinal)
if ($functionEnd -lt 0) {
    throw "XeO3 FetchIndexBuffer function is not terminated."
}
$functionEnd += 2

$typedFunction = @'
uint FetchIndexBuffer(uint hostIndex)
{
    const uint endian = IbDescEndian(PackedIbDesc);
    const bool bits32 = IbDescBits32(PackedIbDesc);
    const uint elementIndex = IbBase + hostIndex;
    if (bits32)
    {
        return GetEndianSwappedValue(
            IndexBuffer.Load(elementIndex), endian);
    }

    const uint pairIndex = elementIndex & ~1u;
    const uint low = IndexBuffer.Load(pairIndex) & 0xFFFFu;
    const uint high = IndexBuffer.Load(pairIndex + 1u) & 0xFFFFu;
    const uint rawDword = low | (high << 16);
    const uint swapped = GetEndianSwappedValue(rawDword, endian);
    return (elementIndex & 1u) != 0u
        ? (swapped >> 16)
        : (swapped & 0xFFFFu);
}
'@

$typedSource = $source.Replace(
    $declaration,
    'Buffer<uint> IndexBuffer:              register(t34);')
$typedSource =
    $typedSource.Substring(0, $functionStart) +
    $typedFunction +
    $typedSource.Substring($functionEnd)

$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$typedRoot = Join-Path $outputRoot 'typed-index-buffer'
[IO.Directory]::CreateDirectory($typedRoot) | Out-Null
$typedPath = Join-Path $typedRoot 'common_header.h'
[IO.File]::WriteAllText(
    $typedPath,
    $typedSource,
    [Text.UTF8Encoding]::new($false))

[pscustomobject]@{
    SourcePath = $sourcePath
    SourceSha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $sourcePath
    ).Hash
    TypedIndexHeader = $typedPath
    TypedIndexHeaderSha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $typedPath
    ).Hash
}

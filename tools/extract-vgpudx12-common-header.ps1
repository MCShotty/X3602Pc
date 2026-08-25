[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$VgpuDllPath,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$resolvedDll = (Resolve-Path -LiteralPath $VgpuDllPath).Path
$bytes = [IO.File]::ReadAllBytes($resolvedDll)
$imageText = [Text.Encoding]::ASCII.GetString($bytes)
$marker = '#include "patch_const.h"'
$start = $imageText.IndexOf($marker, [StringComparison]::Ordinal)
if ($start -lt 0) {
    throw "Embedded common_header.h marker was not found in '$resolvedDll'."
}

$end = $start
while ($end -lt $bytes.Length -and $bytes[$end] -ne 0) {
    $end++
}
if ($end -eq $bytes.Length) {
    throw "Embedded common_header.h in '$resolvedDll' is not null-terminated."
}

$source = [Text.Encoding]::ASCII.GetString($bytes, $start, $end - $start)
foreach ($requiredMarker in @(
    'T GetEndianSwappedValue(T value, uint endian)',
    'uint HostToGuestIndex(uint hostIndex)',
    'float4 FetchByID_FLOAT4('
)) {
    if ($source.IndexOf($requiredMarker, [StringComparison]::Ordinal) -lt 0) {
        throw "Embedded common_header.h is missing expected marker '$requiredMarker'."
    }
}

$parent = Split-Path -Parent $OutputPath
if ($parent) {
    New-Item -ItemType Directory -Path $parent -Force | Out-Null
}
$resolvedOutput = [IO.Path]::GetFullPath($OutputPath)
[IO.File]::WriteAllText(
    $resolvedOutput,
    $source,
    [Text.UTF8Encoding]::new($false)
)

$dllHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedDll).Hash
$sourceBytes = [Text.Encoding]::UTF8.GetBytes($source)
$sha = [Security.Cryptography.SHA256]::Create()
try {
    $sourceHash = (
        [BitConverter]::ToString($sha.ComputeHash($sourceBytes))
    ).Replace('-', '')
} finally {
    $sha.Dispose()
}

[pscustomobject]@{
    VgpuDllPath = $resolvedDll
    VgpuDllSha256 = $dllHash
    OutputPath = $resolvedOutput
    SourceOffset = $start
    SourceLength = $source.Length
    SourceSha256 = $sourceHash
}

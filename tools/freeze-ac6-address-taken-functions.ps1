param(
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [string]$CandidatePath,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$expectedXexHash =
    '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'
$actualXexHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualXexHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualXexHash"
}

if (-not $CandidatePath) {
    $CandidatePath = Join-Path $repoRoot 'out\ac6\hidden_entry_candidates.tsv'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot 'configs\ac6\address_taken_functions.tsv'
}

$candidates = @(Import-Csv -Delimiter "`t" -LiteralPath $CandidatePath)
if ($candidates.Count -eq 0 -or
    -not ($candidates[0].PSObject.Properties.Name -contains
        'table_pointer_count')) {
    throw "Hidden-entry scan lacks table-pointer evidence: $CandidatePath"
}

$imageBase = [Convert]::ToUInt32('82000000', 16)
$imageEnd = $imageBase + [Convert]::ToUInt32('00AA0000', 16)
$functions = @(
    $candidates |
        Where-Object { [uint32]$_.table_pointer_count -ne 0 } |
        Sort-Object {
            [Convert]::ToUInt32($_.candidate_address.Substring(2), 16)
        }
)

$seen = [Collections.Generic.HashSet[uint32]]::new()
$lines = [Collections.Generic.List[string]]::new()
$lines.Add(
    "address`tsize`tdata_pointer_count`ttable_pointer_count"
)
foreach ($function in $functions) {
    $address =
        [Convert]::ToUInt32($function.candidate_address.Substring(2), 16)
    $size =
        [Convert]::ToUInt32($function.candidate_size.Substring(2), 16)
    if ($address -lt $imageBase -or $address -ge $imageEnd) {
        throw "Address-taken function is outside the pinned image: $($function.candidate_address)"
    }
    if ($size -eq 0 -or ($size % 4) -ne 0 -or
        [uint64]$address + $size -gt $imageEnd) {
        throw "Invalid address-taken function size: $($function.candidate_address) $($function.candidate_size)"
    }
    if (-not $seen.Add($address)) {
        throw "Duplicate address-taken function: $($function.candidate_address)"
    }

    $lines.Add((
        "0x{0:X8}`t0x{1:X8}`t{2}`t{3}" -f @(
            $address,
            $size,
            [uint32]$function.data_pointer_count,
            [uint32]$function.table_pointer_count
        )
    ))
}

if ($functions.Count -ne 404) {
    throw "Pinned AC6 table-function count changed: expected 404, found $($functions.Count)"
}

[IO.File]::WriteAllLines(
    [IO.Path]::GetFullPath($OutputPath),
    $lines,
    (New-Object Text.UTF8Encoding($false))
)

[pscustomobject]@{
    XexSha256 = $actualXexHash
    FunctionCount = $functions.Count
    Output = [IO.Path]::GetFullPath($OutputPath)
    Sha256 = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $OutputPath
    ).Hash
} | Format-List

param(
    [string]$ImportsPath,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($ImportsPath)) {
    $ImportsPath = Join-Path $repoRoot 'out\ac6\imports.tsv'
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot 'out\ac6\generated\ac6_imports.inc'
}

$imports = @(
    Import-Csv -Delimiter "`t" -LiteralPath $ImportsPath |
        Where-Object kind -eq 'import'
)
if ($imports.Count -ne 229) {
    throw "Expected 229 AC6 function imports, found $($imports.Count)."
}

$seenNames = @{}
$seenAddresses = @{}
$builder = [Text.StringBuilder]::new()
for ($index = 0; $index -lt $imports.Count; ++$index) {
    $import = $imports[$index]
    if ($import.name -notmatch '^[A-Za-z_][A-Za-z0-9_]*$') {
        throw "Import name is not a C++ identifier: $($import.name)"
    }
    if ($import.address -notmatch '^0x[0-9A-Fa-f]{8}$') {
        throw "Import address is malformed: $($import.address)"
    }
    if ($seenNames.ContainsKey($import.name)) {
        throw "Duplicate import name: $($import.name)"
    }
    if ($seenAddresses.ContainsKey($import.address)) {
        throw "Duplicate import address: $($import.address)"
    }
    $seenNames[$import.name] = $true
    $seenAddresses[$import.address] = $true
    $line = 'XEO3_AC6_IMPORT({0}, {1}u, {2})' -f @(
        $index,
        $import.address.ToUpperInvariant(),
        $import.name
    )
    [void]$builder.AppendLine($line)
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$content = $builder.ToString()
if (-not (Test-Path -LiteralPath $OutputPath) -or
    [IO.File]::ReadAllText($OutputPath) -cne $content) {
    [IO.File]::WriteAllText(
        $OutputPath,
        $content,
        [Text.UTF8Encoding]::new($false))
}

[pscustomobject]@{
    ImportCount = $imports.Count
    Output = (Resolve-Path -LiteralPath $OutputPath).Path
    Sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $OutputPath).Hash
} | Format-List

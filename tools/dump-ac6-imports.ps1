param(
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex'
)

$ErrorActionPreference = 'Stop'

$expectedXexHash = '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$tool = Join-Path $repoRoot 'build\probe\ac6_xex_symbols.exe'
$outputRoot = Join-Path $repoRoot 'out\ac6'
$output = Join-Path $outputRoot 'imports.tsv'

if (-not (Test-Path -LiteralPath $tool)) {
    throw "XEX symbol tool is missing: $tool"
}

$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualHash"
}

New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
& $tool $XexPath $output
if ($LASTEXITCODE -ne 0) {
    throw "XEX symbol extraction failed with exit code $LASTEXITCODE"
}

$imports = Import-Csv -Delimiter "`t" -LiteralPath $output |
    Where-Object kind -eq 'import'

[pscustomobject]@{
    XexPath = $XexPath
    XexSha256 = $actualHash
    Output = $output
    FunctionImportCount = @($imports).Count
    ContainsFirstObservedImport = [bool](
        $imports | Where-Object address -ieq '0x823CFE1C'
    )
} | Format-List

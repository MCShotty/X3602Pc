param(
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [string]$AnalyzerPath = '',
    [string]$SwitchTablePath = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$expectedXexHash =
    '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'

if (-not (Test-Path -LiteralPath $XexPath -PathType Leaf)) {
    throw "AC6 XEX is missing: $XexPath"
}
$actualXexHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualXexHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualXexHash"
}

$prepared = & (Join-Path $PSScriptRoot 'prepare-xenonrecomp.ps1')
$analyzer = if ($AnalyzerPath) {
    [IO.Path]::GetFullPath($AnalyzerPath)
} else {
    Join-Path `
        $repoRoot `
        "build\xenonrecomp-patched-$($prepared.PatchSeries.Substring(0, 12))\XenonAnalyse\XenonAnalyse.exe"
}
if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf)) {
    throw "Patched XenonAnalyse build is missing: $analyzer"
}

$switchTable = if ($SwitchTablePath) {
    if ([IO.Path]::IsPathRooted($SwitchTablePath)) {
        [IO.Path]::GetFullPath($SwitchTablePath)
    } else {
        [IO.Path]::GetFullPath((Join-Path $repoRoot $SwitchTablePath))
    }
} else {
    Join-Path $repoRoot 'out\ac6\switch_tables.toml'
}
$outputRoot = Split-Path -Parent $switchTable
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

& $analyzer $XexPath $switchTable
if ($LASTEXITCODE -ne 0) {
    throw "XenonAnalyse failed with exit code $LASTEXITCODE"
}
if (-not (Test-Path -LiteralPath $switchTable -PathType Leaf)) {
    throw "XenonAnalyse did not create $switchTable"
}

[pscustomobject]@{
    Analyzer = $analyzer
    Input = (Resolve-Path -LiteralPath $XexPath).Path
    InputSha256 = $actualXexHash
    SwitchTables = (Resolve-Path -LiteralPath $switchTable).Path
} | Format-List

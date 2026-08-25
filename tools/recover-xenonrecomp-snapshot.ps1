param(
    [Parameter(Mandatory)]
    [string]$SnapshotPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'out\ac6'))
$generatedRoot = [IO.Path]::GetFullPath((Join-Path $outputRoot 'generated'))
$snapshotRoot = (Resolve-Path -LiteralPath $SnapshotPath).Path

if ([IO.Path]::GetDirectoryName($generatedRoot) -ne $outputRoot -or
    [IO.Path]::GetDirectoryName($snapshotRoot) -ne $outputRoot -or
    -not [IO.Path]::GetFileName($snapshotRoot).StartsWith(
        '.generated-previous-',
        [StringComparison]::Ordinal)) {
    throw "Refusing to recover unexpected generated paths: $generatedRoot, $snapshotRoot"
}
if (-not (Test-Path -LiteralPath $generatedRoot -PathType Container)) {
    throw "Generated output is missing: $generatedRoot"
}

$reusedFiles = 0
foreach ($newFile in Get-ChildItem -LiteralPath $generatedRoot -File) {
    $newPath = $newFile.FullName
    $previousFile = Join-Path $snapshotRoot $newFile.Name
    if (-not (Test-Path -LiteralPath $previousFile -PathType Leaf) -or
        (Get-Item -LiteralPath $previousFile).Length -ne $newFile.Length -or
        (Get-FileHash -Algorithm SHA256 -LiteralPath $previousFile).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $newPath).Hash) {
        continue
    }
    Copy-Item -LiteralPath $previousFile -Destination $newPath -Force
    ++$reusedFiles
}

Remove-Item -LiteralPath $snapshotRoot -Recurse -Force

[pscustomobject]@{
    GeneratedRoot = $generatedRoot
    ReusedFiles = $reusedFiles
    SnapshotRemoved = -not (Test-Path -LiteralPath $snapshotRoot)
} | Format-List

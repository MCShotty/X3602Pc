[CmdletBinding()]
param(
    [string]$CommandListsSource =
        'out\pix\ac6-vpos-exposure-gameplay-export\CommandLists_000.cpp'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sourcePath = (Resolve-Path (
    Join-Path $repoRoot $CommandListsSource)).Path
$source = Get-Content -LiteralPath $sourcePath
if ($source -match 'ShouldExecuteAc6MainSceneDraw\(526,\s*\d+\)') {
    throw 'Main-scene draw calls are already instrumented.'
}

$result = [Collections.Generic.List[string]]::new()
$commandListId = 0
$globalId = 0
$instrumented = 0
foreach ($line in $source) {
    if ($line -match '^void PopulateCommandList_(526|564)_') {
        $commandListId = [int]$Matches[1]
    } elseif ($line -match '^void PopulateCommandList_') {
        $commandListId = 0
    }

    if ($commandListId -ne 0 -and $line -match 'GlobalId\s*=\s*(\d+)') {
        $globalId = [int]$Matches[1]
    }

    if ($commandListId -ne 0 -and
        $line -match '^(\s*)GetCommandList\((526|564)\)->DrawInstanced\((.+)\);$') {
        if ($globalId -eq 0) {
            throw "DrawInstanced in command list $commandListId has no GlobalId."
        }
        $indent = $Matches[1]
        $result.Add(
            "${indent}if (ShouldExecuteAc6MainSceneDraw($commandListId, $globalId))")
        $result.Add("${indent}{")
        $result.Add("${indent}    $($line.TrimStart())")
        $result.Add("${indent}}")
        ++$instrumented
        continue
    }

    $result.Add($line)
}

if ($instrumented -ne 658) {
    throw "Expected to instrument 658 draws, found $instrumented."
}

Set-Content -LiteralPath $sourcePath -Value $result -Encoding utf8
[pscustomobject]@{
    Source = $sourcePath
    InstrumentedDraws = $instrumented
}

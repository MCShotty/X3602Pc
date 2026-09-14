$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot '../tools/capture-ac6-g2h-transfers.ps1'
$tokens = $null
$parseErrors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile(
    (Resolve-Path -LiteralPath $scriptPath), [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count -ne 0) { throw ($parseErrors | Out-String) }
$function = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'New-Ac6G2HTraceCommands'
}, $true)
if (-not $function) { throw 'Production command generator not found.' }
. ([scriptblock]::Create($function.Extent.Text))
$text = New-Ac6G2HTraceCommands 0x180000000 3 1280 720 $true
foreach ($required in @('bp 0x000000018000CC3F', '==0n1280', '==0n720', '>=0n3', 'k 0n24;', '.detach; q', 'sxi av', 'AC6_G2H|', '\\n')) {
    if (-not $text.Contains($required)) { throw "Missing command contract: $required" }
}
if ($text.Contains('&&') -or $text.Contains('__') -or $text.Contains('0x9280')) {
    throw 'Unsafe or unexpanded command expression.'
}
$unfiltered = New-Ac6G2HTraceCommands 0x180000000 16 0 0 $false
if ($unfiltered.Contains('==0n0') -or $unfiltered.Contains('k 0n24;')) {
    throw 'Optional filters were not removed.'
}
foreach ($arguments in @(
    @(0, 16, 0, 0, $false),
    @([uint64]::MaxValue, 16, 0, 0, $false),
    @(0x180000000, 0, 0, 0, $false),
    @(0x180000000, 1025, 0, 0, $false),
    @(0x180000000, 16, 1280, 0, $false),
    @(0x180000000, 16, -1, 720, $false)
)) {
    $rejected = $false
    try { New-Ac6G2HTraceCommands @arguments | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw "Invalid command parameters accepted: $arguments" }
}
'G2H command generation: bounded site, filters, stack, syntax and 6 rejection cases passed.'

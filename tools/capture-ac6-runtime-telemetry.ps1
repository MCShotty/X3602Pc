param(
    [int]$TargetProcessId = 0,
    [ValidateRange(1, 2400)]
    [int]$Samples = 240,
    [ValidateRange(10, 60000)]
    [int]$IntervalMilliseconds = 250,
    [Parameter(Mandatory)]
    [string]$OutputPath,
    [switch]$AllowMissingExports
)

$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outputRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'out\runtime'))
$resolvedOutput = if ([IO.Path]::IsPathRooted($OutputPath)) {
    [IO.Path]::GetFullPath($OutputPath)
} else {
    [IO.Path]::GetFullPath((Join-Path $repoRoot $OutputPath))
}
if (-not $resolvedOutput.StartsWith(
        $outputRoot + [IO.Path]::DirectorySeparatorChar,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw "Telemetry output must stay under $outputRoot."
}

$reader = Join-Path $PSScriptRoot 'read-ac6-runtime-telemetry.ps1'
New-Item -ItemType Directory -Path ([IO.Path]::GetDirectoryName($resolvedOutput)) -Force |
    Out-Null

$utf8 = [Text.UTF8Encoding]::new($false)
$writer = [IO.StreamWriter]::new($resolvedOutput, $false, $utf8)
$writer.AutoFlush = $true
try {
    & $reader `
        -TargetProcessId $TargetProcessId `
        -Samples $Samples `
        -IntervalMilliseconds $IntervalMilliseconds `
        -AllowMissingExports:$AllowMissingExports |
        ForEach-Object {
            $writer.WriteLine(($_ | ConvertTo-Json -Compress -Depth 3))
        }
} finally {
    $writer.Dispose()
}

[pscustomobject]@{
    OutputPath = $resolvedOutput
    Samples = $Samples
    IntervalMilliseconds = $IntervalMilliseconds
}

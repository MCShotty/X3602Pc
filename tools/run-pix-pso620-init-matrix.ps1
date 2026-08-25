[CmdletBinding()]
param(
    [string]$OutputDirectory = 'out\pix\pso620-init-matrix'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$genericMatrix = Join-Path $PSScriptRoot 'run-pix-shader-variant-matrix.ps1'
$workingRoot = Join-Path $repoRoot 'out\pix\ac6-vpos-exposure-gameplay-export-build'
$variantRoot = Join-Path $repoRoot 'out\pix\ac6-vpos-exposure-gameplay-export'
$replay = Join-Path $workingRoot 'Emu.exe'
$resolveShader = Join-Path $workingRoot 'pso523-resolve-4x-upscale.dxil'

$names = @(
    'AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE',
    'AC6_PSO397_ENDIAN_OVERRIDE',
    'AC6_PSO523_RESOLVE_PS',
    'AC6_PSO523_EVENT2051_DIRECT_RESOLVE',
    'AC6_LIST599_MAX_SECTION',
    'AC6_RESOURCE329_SNAPSHOT_STAGE',
    'AC6_FINAL_SOURCE_RESOURCE242',
    'AC6_FINAL_SOURCE_RESOURCE246',
    'AC6_FINAL_SOURCE_RESOURCE329',
    'AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT'
)
$saved = @{}
foreach ($name in $names) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}

try {
    $env:AC6_PSO397_EVENT1682_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO397_ENDIAN_OVERRIDE = '0'
    $env:AC6_PSO523_RESOLVE_PS = $resolveShader
    $env:AC6_PSO523_EVENT2051_DIRECT_RESOLVE = '1'
    $env:AC6_LIST599_MAX_SECTION = '-1'
    $env:AC6_RESOURCE329_SNAPSHOT_STAGE = '2'
    $env:AC6_FINAL_SOURCE_RESOURCE242 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE246 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329 = '0'
    $env:AC6_FINAL_SOURCE_RESOURCE329_SNAPSHOT = '1'

    & $genericMatrix `
        -ReplayPath $replay `
        -WorkingDirectory $workingRoot `
        -VariantDirectory $variantRoot `
        -OutputDirectory $OutputDirectory `
        -OverrideEnvironmentVariable AC6_PSO620_PS_OVERRIDE `
        -VariantName @(
            'pso620-fixed',
            'pso620-fixed-bgra',
            'pso620-fixed-signed',
            'pso620-fixed-bgra-signed',
            'pso620-geometry-only',
            'pso620-geometry-only-signed'
        )
} finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
    }
}

[CmdletBinding()]
param(
    [string]$BuildDirectory = 'out\pix\ac6-vpos-exposure-gameplay-export-build',
    [ValidateRange(1, 32)]
    [int]$Parallel = 4
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildPath = [IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDirectory))
$developerPrompt = 'C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat'

$command = (
    'call "{0}" -arch=x64 -host_arch=x64 && cmake --build "{1}" ' +
    '--config RelWithDebInfo --parallel {2}'
) -f
    $developerPrompt,
    $buildPath,
    $Parallel
& cmd.exe /d /c $command
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

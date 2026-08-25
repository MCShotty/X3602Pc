param(
    [string]$ExpectedLlvmVersion = '22.1.8',
    [string]$ExpectedNinjaVersion = '1.13.2'
)

$ErrorActionPreference = 'Stop'

$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vsWhere)) {
    throw "vswhere.exe was not found at $vsWhere"
}

$vsInstall = & $vsWhere `
    -latest `
    -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath

if (-not $vsInstall) {
    throw 'A Visual Studio installation with the x64 C++ toolchain was not found.'
}

$devShell = Join-Path $vsInstall 'Common7\Tools\Launch-VsDevShell.ps1'
. $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

$llvmBin = Join-Path $env:ProgramFiles 'LLVM\bin'
$wingetLinks = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Links'
$env:Path = "$llvmBin;$wingetLinks;$env:Path"

$clangCl = (Get-Command clang-cl.exe -ErrorAction Stop).Source
$ninja = (Get-Command ninja.exe -ErrorAction Stop).Source
$ml64 = (Get-Command ml64.exe -ErrorAction Stop).Source
$lldLink = (Get-Command lld-link.exe -ErrorAction Stop).Source
$llvmMt = (Get-Command llvm-mt.exe -ErrorAction Stop).Source

$clangVersion = (& $clangCl --version | Select-Object -First 1)
$ninjaVersion = (& $ninja --version).Trim()

if ($clangVersion -notmatch [regex]::Escape($ExpectedLlvmVersion)) {
    throw "Expected LLVM $ExpectedLlvmVersion, found: $clangVersion"
}

if ($ninjaVersion -ne $ExpectedNinjaVersion) {
    throw "Expected Ninja $ExpectedNinjaVersion, found: $ninjaVersion"
}

$env:XEO3_CLANG_CL = $clangCl
$env:XEO3_NINJA = $ninja
$env:XEO3_ML64 = $ml64
$env:XEO3_LLD_LINK = $lldLink
$env:XEO3_LLVM_MT = $llvmMt
$env:XEO3_VS_INSTALL = $vsInstall

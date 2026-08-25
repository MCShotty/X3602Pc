param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab'
)

$ErrorActionPreference = 'Stop'

$expectedEmuHash = 'D1578E07B533E391D8A81C330D5493BA2D45B252490A818EC148DABE1BA24D06'
$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildRoot = Join-Path $repoRoot 'build\probe'
$sourceDll = Join-Path $buildRoot $dllName
$sourcePdb = [System.IO.Path]::ChangeExtension($sourceDll, '.pdb')
$noDllName = [System.IO.Path]::GetFileNameWithoutExtension($dllName) + '_no.dll'
$labEmu = Join-Path $LabRoot 'Emu.exe'

if ($LabRoot -like '*\WindowsApps\*') {
    throw 'Refusing to deploy into WindowsApps.'
}

foreach ($requiredPath in @($sourceDll, $sourcePdb, $labEmu)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required deployment input is missing: $requiredPath"
    }
}

$actualEmuHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $labEmu).Hash
if ($actualEmuHash -ne $expectedEmuHash) {
    throw "Lab Emu.exe hash mismatch: expected $expectedEmuHash, found $actualEmuHash"
}

Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $LabRoot $dllName) -Force
Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $LabRoot $noDllName) -Force
Copy-Item -LiteralPath $sourcePdb -Destination (Join-Path $LabRoot ([System.IO.Path]::GetFileName($sourcePdb))) -Force

$deployedDll = Join-Path $LabRoot $dllName
$deployedNoDll = Join-Path $LabRoot $noDllName
[pscustomobject]@{
    LabRoot = (Resolve-Path -LiteralPath $LabRoot).Path
    EmuSha256 = $actualEmuHash
    Dll = $deployedDll
    DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedDll).Hash
    NoDll = $deployedNoDll
    NoDllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedNoDll).Hash
} | Format-List

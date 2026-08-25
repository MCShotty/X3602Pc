param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'

$expectedEmuHash =
    'D1578E07B533E391D8A81C330D5493BA2D45B252490A818EC148DABE1BA24D06'
$expectedXexHash =
    '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'
$expectedVgpuHash =
    '17BDCD5866B58DBC50C8BB8C8EC5F9D9BEDFBA81D31DE530760FDE38A8E0EB1D'
$expectedKernelHash =
    'DA5BE614FB51B5809D70DA073F406F071E5CCB1F8C0EBCD57DAFBAB31B519BDD'
$expectedKernelAotHash =
    '27CA5876B505361F00C3E1021FF06B68CD666987D4B517C934D69AADF44E8651'
$expectedPackageVersion = [version]'2607.2223.1.0'
$packageName = 'Xbox360BackwardCompatibil.PrimaryFuzionFrenzyFuzio'
$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$noDllName =
    'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6_no.dll'
$pdbName = [IO.Path]::ChangeExtension($dllName, '.pdb')
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $BuildRoot) {
    $BuildRoot = Join-Path $repoRoot 'build\ac6-aot'
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repoRoot $BuildRoot
}
$sourceRoot = Join-Path ([IO.Path]::GetFullPath($BuildRoot)) 'aot'
$sourceDll = Join-Path $sourceRoot $dllName
$sourcePdb = Join-Path $sourceRoot $pdbName
$labEmu = Join-Path $LabRoot 'Emu.exe'
$labKernel = Join-Path $LabRoot 'Flash\xboxkrnlcf.bin'
$labKernelAot = Join-Path $LabRoot 'xeo3_5fb3687c_001748c4.dll'

$resolvedLab = (Resolve-Path -LiteralPath $LabRoot).Path
if ($resolvedLab -like '*\WindowsApps\*') {
    throw 'Refusing to deploy into WindowsApps.'
}
if (@(Get-Process Emu,cdb,windbg -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Refusing to deploy while Emu or a debugger is running.'
}

$package = Get-AppxPackage -Name $packageName -ErrorAction Stop
if ($package.Version -ne $expectedPackageVersion) {
    throw "XeO3 package version mismatch: expected $expectedPackageVersion, found $($package.Version)"
}
$installedVgpu = Join-Path $package.InstallLocation 'VGPUDX12.dll'

foreach ($requiredPath in @(
    $sourceDll,
    $sourcePdb,
    $labEmu,
    $labKernel,
    $labKernelAot,
    $XexPath,
    $installedVgpu
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required deployment input is missing: $requiredPath"
    }
}

$pinnedInputs = @(
    [pscustomobject]@{
        Name = 'Lab Emu.exe'
        Path = $labEmu
        ExpectedHash = $expectedEmuHash
    },
    [pscustomobject]@{
        Name = 'AC6 default.xex'
        Path = $XexPath
        ExpectedHash = $expectedXexHash
    },
    [pscustomobject]@{
        Name = 'Installed VGPUDX12.dll'
        Path = $installedVgpu
        ExpectedHash = $expectedVgpuHash
    },
    [pscustomobject]@{
        Name = 'Flash kernel'
        Path = $labKernel
        ExpectedHash = $expectedKernelHash
    },
    [pscustomobject]@{
        Name = 'Kernel AOT DLL'
        Path = $labKernelAot
        ExpectedHash = $expectedKernelAotHash
    }
)
foreach ($input in $pinnedInputs) {
    $actualHash = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $input.Path
    ).Hash
    if ($actualHash -ne $input.ExpectedHash) {
        throw "$($input.Name) hash mismatch: expected $($input.ExpectedHash), found $actualHash"
    }
}
$actualEmuHash = $pinnedInputs[0].ExpectedHash

$backupRoot = Join-Path $LabRoot 'LocalAotBackups'
New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
foreach ($name in @($dllName, $noDllName, $pdbName)) {
    $current = Join-Path $LabRoot $name
    if (-not (Test-Path -LiteralPath $current)) {
        continue
    }
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $current).Hash
    $backupName = '{0}.{1}{2}' -f @(
        [IO.Path]::GetFileNameWithoutExtension($name),
        $hash.Substring(0, 16),
        [IO.Path]::GetExtension($name)
    )
    $backup = Join-Path $backupRoot $backupName
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $current -Destination $backup
    }
}

Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $LabRoot $dllName) -Force
Copy-Item -LiteralPath $sourceDll -Destination (Join-Path $LabRoot $noDllName) -Force
Copy-Item -LiteralPath $sourcePdb -Destination (Join-Path $LabRoot $pdbName) -Force

$deployedDll = Join-Path $LabRoot $dllName
$deployedNoDll = Join-Path $LabRoot $noDllName
[pscustomobject]@{
    LabRoot = $resolvedLab
    PackageVersion = $package.Version.ToString()
    EmuSha256 = $actualEmuHash
    XexSha256 = $expectedXexHash
    VgpuSha256 = $expectedVgpuHash
    KernelSha256 = $expectedKernelHash
    KernelAotSha256 = $expectedKernelAotHash
    Dll = $deployedDll
    DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedDll).Hash
    NoDll = $deployedNoDll
    NoDllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $deployedNoDll).Hash
    Pdb = Join-Path $LabRoot $pdbName
    BackupRoot = $backupRoot
} | Format-List

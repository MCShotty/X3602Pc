[CmdletBinding()]
param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [string]$DvdRoot = 'D:\XeO3AC6DVD',
    [string]$ProfilePath = 'profiles\xeo3\2608.3123.1.0-D1578E07.json'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not [IO.Path]::IsPathRooted($ProfilePath)) {
    $ProfilePath = Join-Path $repoRoot $ProfilePath
}
$profile = Get-Content -LiteralPath $ProfilePath -Raw | ConvertFrom-Json
$package = Get-AppxPackage -Name $profile.package.name -ErrorAction Stop
if (-not $package -or $package.Version -ne [version]$profile.package.version) {
    throw "Unsupported XeO3 package: expected $($profile.package.version), found $($package.Version). Revalidate the host profile before launching."
}
$inputs = @(
    @{ Path = Join-Path $LabRoot 'Emu.exe'; Hash = $profile.emu.sha256 },
    @{ Path = Join-Path $package.InstallLocation 'VGPUDX12.dll'; Hash = $profile.vgpuDx12.sha256 },
    @{ Path = Join-Path $LabRoot $profile.kernelAot.module; Hash = $profile.kernelAot.sha256 },
    @{ Path = Join-Path $LabRoot $profile.flashKernel.path; Hash = $profile.flashKernel.sha256 },
    @{ Path = Join-Path $DvdRoot 'default.xex'; Hash = '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC' }
)
foreach ($inputFile in $inputs) {
    $actualHash = (Get-FileHash -LiteralPath $inputFile.Path -Algorithm SHA256).Hash
    if ($actualHash -ne $inputFile.Hash) {
        throw "XeO3 input mismatch for $($inputFile.Path): expected $($inputFile.Hash), found $actualHash."
    }
}
[pscustomobject]@{
    Profile = $ProfilePath
    PackageVersion = $package.Version.ToString()
    VgpuPath = $inputs[1].Path
    KernelAotPath = $inputs[2].Path
    VerifiedInputCount = $inputs.Count
}

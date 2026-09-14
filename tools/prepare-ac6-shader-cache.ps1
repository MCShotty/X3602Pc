param(
    [Parameter(Mandatory)]
    [string]$LabRoot,
    [Parameter(Mandatory)]
    [string]$DvdRoot,
    [Parameter(Mandatory)]
    [string]$ConfigPath,
    [string]$ShaderCacheRoot,
    [string]$VgpuPath,
    [string]$KernelAotPath
)

$ErrorActionPreference = 'Stop'

$LabRoot = [System.IO.Path]::GetFullPath($LabRoot)
$DvdRoot = [System.IO.Path]::GetFullPath($DvdRoot)
$ConfigPath = [System.IO.Path]::GetFullPath($ConfigPath)
if (-not $ShaderCacheRoot) {
    $ShaderCacheRoot = Join-Path $LabRoot 'Storage\ShaderCache\XeO3_ShaderCache'
}
$ShaderCacheRoot = [System.IO.Path]::GetFullPath($ShaderCacheRoot)

$configText = [System.IO.File]::ReadAllText($ConfigPath)
$titleMatch = [System.Text.RegularExpressions.Regex]::Match(
    $configText,
    '(?im)^titleId=([0-9a-f]{8})\s*$'
)
if (-not $titleMatch.Success) {
    throw "No eight-digit titleId was found in $ConfigPath."
}
$titleId = $titleMatch.Groups[1].Value.ToUpperInvariant()

$aotName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
if (-not $VgpuPath -or -not $KernelAotPath) {
    $validatedHost = & (Join-Path $PSScriptRoot 'test-ac6-host-profile.ps1') `
        -LabRoot $LabRoot -DvdRoot $DvdRoot
    $VgpuPath = $validatedHost.VgpuPath
    $KernelAotPath = $validatedHost.KernelAotPath
}
$inputs = [ordered]@{
    AotDll = Join-Path $LabRoot $aotName
    Emu = Join-Path $LabRoot 'Emu.exe'
    Vgpu = $VgpuPath
    KernelAot = $KernelAotPath
    D3d12Core = Join-Path $LabRoot 'D3D12Core.dll'
    LaunchArguments = $ConfigPath
    DefaultXex = Join-Path $DvdRoot 'default.xex'
}

$inputHashes = [ordered]@{}
foreach ($entry in $inputs.GetEnumerator()) {
    if (-not (Test-Path -LiteralPath $entry.Value -PathType Leaf)) {
        throw "Shader-cache fingerprint input is missing: $($entry.Value)"
    }
    $inputHashes[$entry.Key] = (
        Get-FileHash -Algorithm SHA256 -LiteralPath $entry.Value
    ).Hash
}

$gpu = Get-CimInstance Win32_VideoController |
    Where-Object { $_.PNPDeviceID -like 'PCI\VEN_1002*' } |
    Select-Object -First 1
if (-not $gpu) {
    throw 'The pinned AC6 shader-cache workflow requires the AMD display adapter.'
}
$umdPath = @($gpu.InstalledDisplayDrivers -split ',') |
    Where-Object { [IO.Path]::GetFileName($_) -ieq 'amdxc64.dll' } |
    Select-Object -First 1
if (-not $umdPath -or -not (Test-Path -LiteralPath $umdPath -PathType Leaf)) {
    throw 'The active AMD D3D12 user-mode driver amdxc64.dll was not found.'
}
$gpuDriver = [ordered]@{
    Name = [string]$gpu.Name
    PnpDeviceId = [string]$gpu.PNPDeviceID
    DriverVersion = [string]$gpu.DriverVersion
    DriverDate = ([datetime]$gpu.DriverDate).ToUniversalTime().ToString('o')
    UmdPath = [IO.Path]::GetFullPath($umdPath)
    UmdSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $umdPath).Hash
}
$fingerprintMaterial = [ordered]@{
    Files = $inputHashes
    GpuDriver = $gpuDriver
}
$fingerprintJson = $fingerprintMaterial | ConvertTo-Json -Depth 4 -Compress
$sha256 = [System.Security.Cryptography.SHA256]::Create()
try {
    $fingerprintBytes = [System.Text.Encoding]::UTF8.GetBytes($fingerprintJson)
    $fingerprint = [System.BitConverter]::ToString(
        $sha256.ComputeHash($fingerprintBytes)
    ).Replace('-', '')
} finally {
    $sha256.Dispose()
}

$cacheRoot = $ShaderCacheRoot
$titleCache = [System.IO.Path]::GetFullPath(
    (Join-Path $cacheRoot $titleId)
)
$statePath = [System.IO.Path]::GetFullPath(
    (Join-Path $cacheRoot ".codex-$titleId-inputs.json")
)

foreach ($path in @($titleCache, $statePath)) {
    if (-not $path.StartsWith(
            $cacheRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Shader-cache path escaped the expected root: $path"
    }
}

New-Item -ItemType Directory -Path $cacheRoot -Force | Out-Null
$oldState = if (Test-Path -LiteralPath $statePath -PathType Leaf) {
    Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
}

$cacheEntries = @(
    Get-ChildItem -LiteralPath $titleCache -Force -ErrorAction SilentlyContinue
)
$fingerprintChanged = -not $oldState -or
    $oldState.fingerprint -ne $fingerprint
$backupPath = $null

if ($fingerprintChanged -and $cacheEntries.Count -gt 0) {
    $oldTag = if ($oldState -and $oldState.fingerprint) {
        $oldState.fingerprint.Substring(0, 12)
    } else {
        'legacy'
    }
    $backupName = '{0}.backup-{1}-{2}' -f (
        $titleId,
        (Get-Date -Format 'yyyyMMdd-HHmmss'),
        $oldTag
    )
    $backupPath = [System.IO.Path]::GetFullPath(
        (Join-Path $cacheRoot $backupName)
    )
    if (-not $backupPath.StartsWith(
            $cacheRoot + [System.IO.Path]::DirectorySeparatorChar,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Shader-cache backup escaped the expected root: $backupPath"
    }
    if (Test-Path -LiteralPath $backupPath) {
        throw "Shader-cache backup already exists: $backupPath"
    }
    Move-Item -LiteralPath $titleCache -Destination $backupPath
}

New-Item -ItemType Directory -Path $titleCache -Force | Out-Null

$state = [ordered]@{
    schemaVersion = 3
    titleId = $titleId
    fingerprint = $fingerprint
    recordedAt = (Get-Date).ToString('o')
    inputs = $inputHashes
    gpuDriver = $gpuDriver
}
$state | ConvertTo-Json -Depth 4 |
    Set-Content -LiteralPath $statePath -Encoding utf8

[pscustomobject]@{
    TitleId = $titleId
    Fingerprint = $fingerprint
    CachePath = $titleCache
    ArchivedCachePath = $backupPath
    Invalidated = [bool]$backupPath
}

param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [string]$DvdRoot = 'D:\XeO3AC6DVD',
    [string]$PixToolPath = ''
)

$ErrorActionPreference = 'Stop'
$packageName = 'Xbox360BackwardCompatibil.PrimaryFuzionFrenzyFuzio'
$configSource = Join-Path $PSScriptRoot '..\configs\ac6\LaunchArguments.txt'
$prepareShaderCache = Join-Path $PSScriptRoot 'prepare-ac6-shader-cache.ps1'
$requiredPaths = @(
    $prepareShaderCache,
    (Join-Path $LabRoot 'Emu.exe'),
    (Join-Path $LabRoot 'MicrosoftGame.config'),
    (Join-Path $LabRoot 'VGPUDX12.dll'),
    (Join-Path $LabRoot 'D3D12Core.dll'),
    (Join-Path $LabRoot 'BootAnim.mp4'),
    (Join-Path $DvdRoot 'default.xex')
)

foreach ($path in $requiredPaths) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required XeO3 input is missing: $path"
    }
}
if ($PixToolPath -and -not (Test-Path -LiteralPath $PixToolPath)) {
    throw "PIX command-line tool is missing: $PixToolPath"
}

$flashRoot = Join-Path $LabRoot 'Flash'
foreach ($extension in @('bin', 'hvdata')) {
    $cePath = Join-Path $flashRoot "xboxkrnlce.$extension"
    if (-not (Test-Path -LiteralPath $cePath)) {
        Copy-Item -LiteralPath (Join-Path $flashRoot "xboxkrnlcf.$extension") -Destination $cePath
    }
}

$configText = [System.IO.File]::ReadAllText($configSource)
$configText = [System.Text.RegularExpressions.Regex]::Replace($configText, '\r?\n', "`r`n")
[System.IO.File]::WriteAllText(
    (Join-Path $LabRoot 'LaunchArguments.txt'),
    $configText,
    (New-Object System.Text.UTF8Encoding($false))
)

New-Item -ItemType Directory -Path (Join-Path $LabRoot 'Storage') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $LabRoot 'XeO3_ShaderCache') -Force | Out-Null
$cacheState = & $prepareShaderCache `
    -LabRoot $LabRoot `
    -DvdRoot $DvdRoot `
    -ConfigPath $configSource
$package = Get-AppxPackage -Name $packageName -ErrorAction Stop
$storageRoot = Join-Path $LabRoot 'Storage'
$shaderCacheRoot = Join-Path $LabRoot 'XeO3_ShaderCache'
$arguments = "dvd `"$DvdRoot`" root `"$LabRoot`" systemfiles `"$LabRoot`" storage `"$storageRoot`" shadercache `"$shaderCacheRoot`""

Push-Location $LabRoot
try {
    $launchCommand = Join-Path $LabRoot 'Emu.exe'
    $launchArguments = $arguments
    if ($PixToolPath) {
        $launchCommand = $PixToolPath
        $quotedEmulator = '"' + (Join-Path $LabRoot 'Emu.exe') + '"'
        $quotedArguments = '"' + $arguments.Replace('"', '\"') + '"'
        $quotedWorkingDirectory = '"' + $LabRoot + '"'
        $launchArguments = @(
            'launch',
            $quotedEmulator,
            "--command-line=$quotedArguments",
            "--working-directory=$quotedWorkingDirectory",
            'set-gpu-capture-parameters',
            '--frames=1',
            'programmatic-capture',
            '--until-exit'
        ) -join ' '
    }
    Invoke-CommandInDesktopPackage `
        -PackageFamilyName $package.PackageFamilyName `
        -AppId 'App' `
        -Command $launchCommand `
        -Args $launchArguments `
        -PreventBreakaway
} finally {
    Pop-Location
}

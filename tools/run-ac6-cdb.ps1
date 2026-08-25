param(
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [string]$DvdRoot = 'D:\XeO3AC6DVD',
    [string]$TracePath,
    [string]$CommandFile,
    [switch]$ReuseCompatibleShaderCache
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$packageName = 'Xbox360BackwardCompatibil.PrimaryFuzionFrenzyFuzio'
$configSource = Join-Path $repoRoot 'configs\ac6\LaunchArguments.txt'
$prepareShaderCache = Join-Path $PSScriptRoot 'prepare-ac6-shader-cache.ps1'
if (-not $CommandFile) {
    $CommandFile = Join-Path $PSScriptRoot 'ac6-probe-trace.wds'
}
$commandFile = (Resolve-Path -LiteralPath $CommandFile).Path
$cdb = Get-ChildItem -LiteralPath (Join-Path $repoRoot '.tools') `
    -Directory `
    -Filter 'windbg-amd64-*' |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'cdb.exe' } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1

if (-not $cdb) {
    throw 'The project-local CDB runtime is missing.'
}

if (-not $TracePath) {
    $traceRoot = Join-Path $LabRoot 'ProbeLogs'
    New-Item -ItemType Directory -Path $traceRoot -Force | Out-Null
    $TracePath = Join-Path $traceRoot "$(Get-Date -Format 'yyyyMMdd-HHmmss')-cdb.log"
}

$requiredPaths = @(
    $cdb,
    $commandFile,
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
        throw "Required CDB launch input is missing: $path"
    }
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
$cacheState = if ($ReuseCompatibleShaderCache) {
    $compatibleCache = Join-Path (
        Join-Path $LabRoot 'Storage\ShaderCache\XeO3_ShaderCache'
    ) '4E4D07D1'
    $cacheFiles = @(
        Get-ChildItem -LiteralPath $compatibleCache -Recurse -File `
            -ErrorAction SilentlyContinue
    )
    if ($cacheFiles.Count -eq 0) {
        throw "The compatible AC6 shader cache is empty: $compatibleCache"
    }
    [pscustomobject]@{
        CachePath = $compatibleCache
        Files = $cacheFiles.Count
        Reused = $true
    }
} else {
    & $prepareShaderCache `
        -LabRoot $LabRoot `
        -DvdRoot $DvdRoot `
        -ConfigPath $configSource
}
$package = Get-AppxPackage -Name $packageName -ErrorAction Stop
$emu = Join-Path $LabRoot 'Emu.exe'
$storageRoot = Join-Path $LabRoot 'Storage'
$shaderCacheRoot = Join-Path $LabRoot 'XeO3_ShaderCache'
$emuArguments = "dvd `"$DvdRoot`" root `"$LabRoot`" systemfiles `"$LabRoot`" storage `"$storageRoot`" shadercache `"$shaderCacheRoot`""
$debuggerArguments = "-o -G -logo `"$TracePath`" -cf `"$commandFile`" `"$emu`" $emuArguments"

Push-Location $LabRoot
try {
    Invoke-CommandInDesktopPackage `
        -PackageFamilyName $package.PackageFamilyName `
        -AppId 'App' `
        -Command $cdb `
        -Args $debuggerArguments `
        -PreventBreakaway
} finally {
    Pop-Location
}

[pscustomobject]@{
    TracePath = $TracePath
    Cdb = $cdb
    PackageFamilyName = $package.PackageFamilyName
    ShaderCache = $cacheState
} | Format-List

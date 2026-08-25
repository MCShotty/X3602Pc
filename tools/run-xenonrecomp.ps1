param(
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [string]$RecompilerPath = '',
    [string]$PpcContextPath = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$expectedXexHash = '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'
$actualXexHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualXexHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualXexHash"
}

$configRoot = Join-Path $repoRoot 'configs\ac6'
$privateLink = Join-Path $configRoot 'private'
$xexRoot = Split-Path -Parent (Resolve-Path -LiteralPath $XexPath).Path
if (Test-Path -LiteralPath $privateLink) {
    $linkItem = Get-Item -LiteralPath $privateLink -Force
    if ($linkItem.LinkType -ne 'Junction' -or $linkItem.Target.Count -ne 1) {
        throw "Existing AC6 private input path is not the expected single-target junction: $privateLink"
    }
    $resolvedLink = [System.IO.Path]::GetFullPath($linkItem.Target[0])
    if ($resolvedLink -ne $xexRoot) {
        throw "Existing AC6 private input link targets '$resolvedLink', expected '$xexRoot'"
    }
}
else {
    New-Item -ItemType Junction -Path $privateLink -Target $xexRoot | Out-Null
}

$switchTable = Join-Path $repoRoot 'out\ac6\switch_tables.toml'
if (-not (Test-Path -LiteralPath $switchTable -PathType Leaf)) {
    throw "XenonAnalyse output is missing: $switchTable"
}
$manualSwitchTable = Join-Path $configRoot 'manual_switch_tables.toml'
if (-not (Test-Path -LiteralPath $manualSwitchTable -PathType Leaf)) {
    throw "Manual AC6 switch-table definitions are missing: $manualSwitchTable"
}

$ac6OutputRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'out\ac6'))
$generatedRoot = [System.IO.Path]::GetFullPath((Join-Path $ac6OutputRoot 'generated'))
$expectedGeneratedRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot 'out\ac6\generated'))
if ($generatedRoot -ne $expectedGeneratedRoot) {
    throw "Refusing to clean unexpected generated path: $generatedRoot"
}
$previousGeneratedRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $ac6OutputRoot ".generated-previous-$PID")
)
if ([System.IO.Path]::GetDirectoryName($previousGeneratedRoot) -ne $ac6OutputRoot) {
    throw "Refusing to use unexpected generated snapshot path: $previousGeneratedRoot"
}
if (Test-Path -LiteralPath $previousGeneratedRoot) {
    throw "Generated snapshot path already exists: $previousGeneratedRoot"
}

$hadPreviousGeneration = Test-Path -LiteralPath $generatedRoot
if ($hadPreviousGeneration) {
    Move-Item -LiteralPath $generatedRoot -Destination $previousGeneratedRoot
}
New-Item -ItemType Directory -Path $generatedRoot -Force | Out-Null

$prepared = & (Join-Path $PSScriptRoot 'prepare-xenonrecomp.ps1')
$recompiler = if ($RecompilerPath) {
    [System.IO.Path]::GetFullPath($RecompilerPath)
}
else {
    Join-Path `
        $repoRoot `
        "build\xenonrecomp-patched-$($prepared.PatchSeries.Substring(0, 12))\XenonRecomp\XenonRecomp.exe"
}
$baseConfig = Join-Path $configRoot 'ac6.toml'
$addressTakenPath = Join-Path $configRoot 'address_taken_functions.tsv'
$effectiveConfig = Join-Path $configRoot ".ac6-effective-$PID.toml"
$effectiveSwitchTable = Join-Path $configRoot ".switch-tables-effective-$PID.toml"
$ppcContext = if ($PpcContextPath) {
    [System.IO.Path]::GetFullPath($PpcContextPath)
}
else {
    Join-Path $prepared.Destination 'XenonUtils\ppc_context.h'
}
foreach ($requiredFile in @(
    $recompiler,
    $baseConfig,
    $addressTakenPath,
    $ppcContext
)) {
    if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
        throw "Required XenonRecomp input is missing: $requiredFile"
    }
}

$stdoutLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stdout.log'
$stderrLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stderr.log'
$addressTakenFunctions = @()
try {
    $addressTakenFunctions = @(
        Import-Csv -Delimiter "`t" -LiteralPath $addressTakenPath
    )
    if ($addressTakenFunctions.Count -ne 404) {
        throw "Pinned AC6 address-taken function count changed: expected 404, found $($addressTakenFunctions.Count)"
    }

    $seenAddresses = [Collections.Generic.HashSet[uint32]]::new()
    $effectiveEntries = [Collections.Generic.List[string]]::new()
    foreach ($function in $addressTakenFunctions) {
        if ($function.address -notmatch '^0x[0-9A-Fa-f]{8}$' -or
            $function.size -notmatch '^0x[0-9A-Fa-f]{8}$') {
            throw "Invalid address-taken function row: $($function.address) $($function.size)"
        }
        $address = [Convert]::ToUInt32($function.address.Substring(2), 16)
        $size = [Convert]::ToUInt32($function.size.Substring(2), 16)
        if (-not $seenAddresses.Add($address) -or
            $size -eq 0 -or ($size % 4) -ne 0) {
            throw "Invalid duplicate or size in address-taken function row: $($function.address) $($function.size)"
        }
        $effectiveEntries.Add(
            "    { address = $($function.address), size = $($function.size) },"
        )
    }

    $configText = [IO.File]::ReadAllText($baseConfig)
    $generatedSwitchText = [IO.File]::ReadAllText($switchTable)
    $manualSwitchText = [IO.File]::ReadAllText($manualSwitchTable)
    $manualSwitchBases = @(
        [Regex]::Matches(
            $manualSwitchText,
            '(?im)^\s*base\s*=\s*(0x[0-9a-f]+)\s*$'
        ) | ForEach-Object { $_.Groups[1].Value }
    )
    foreach ($manualSwitchBase in $manualSwitchBases) {
        if ($generatedSwitchText -match
            "(?im)^\s*base\s*=\s*$([Regex]::Escape($manualSwitchBase))\s*$") {
            throw "Manual switch base is now generated automatically: $manualSwitchBase"
        }
    }
    [IO.File]::WriteAllText(
        $effectiveSwitchTable,
        $generatedSwitchText.TrimEnd() + "`r`n`r`n" +
            $manualSwitchText.Trim() + "`r`n",
        (New-Object Text.UTF8Encoding($false))
    )
    $effectiveSwitchLeaf = Split-Path -Leaf $effectiveSwitchTable
    $effectiveSwitchPath = $effectiveSwitchLeaf.Replace('\', '/')
    $effectiveText = [Regex]::Replace(
        $configText,
        '(?m)^switch_table_file_path\s*=\s*"[^"]*"\s*$',
        "switch_table_file_path = `"$effectiveSwitchPath`""
    )
    if ($effectiveText -eq $configText) {
        throw "Unable to replace switch_table_file_path in $baseConfig"
    }
    $functionList = [Regex]::Match(
        $effectiveText,
        '(?ms)^functions\s*=\s*\[(?<body>.*?)^\]'
    )
    if (-not $functionList.Success) {
        throw "Unable to locate the AC6 functions array in $baseConfig"
    }
    foreach ($function in $addressTakenFunctions) {
        if ($effectiveText -match
            "(?im)address\s*=\s*$([Regex]::Escape($function.address))\b") {
            throw "Address-taken function is already in the base config: $($function.address)"
        }
    }

    $insertion = (
        "# Statically proven clustered code-pointer table entries.`r`n" +
        ($effectiveEntries -join "`r`n") +
        "`r`n"
    )
    $effectiveText = $effectiveText.Insert(
        $functionList.Groups['body'].Index +
            $functionList.Groups['body'].Length,
        $insertion
    )
    [IO.File]::WriteAllText(
        $effectiveConfig,
        $effectiveText,
        (New-Object Text.UTF8Encoding($false))
    )

    $process = Start-Process `
        -FilePath $recompiler `
        -ArgumentList @($effectiveConfig, $ppcContext) `
        -WorkingDirectory $repoRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput $stdoutLog `
        -RedirectStandardError $stderrLog `
        -PassThru `
        -Wait
    if ($process.ExitCode -ne 0) {
        throw "XenonRecomp failed with exit code $($process.ExitCode). See $stdoutLog and $stderrLog"
    }
}
catch {
    if (Test-Path -LiteralPath $generatedRoot) {
        Remove-Item -LiteralPath $generatedRoot -Recurse -Force
    }
    if ($hadPreviousGeneration -and
        (Test-Path -LiteralPath $previousGeneratedRoot)) {
        Move-Item -LiteralPath $previousGeneratedRoot -Destination $generatedRoot
    }
    throw
}
finally {
    if (Test-Path -LiteralPath $effectiveConfig) {
        Remove-Item -LiteralPath $effectiveConfig -Force
    }
    if (Test-Path -LiteralPath $effectiveSwitchTable) {
        Remove-Item -LiteralPath $effectiveSwitchTable -Force
    }
}

$reusedFiles = 0
if ($hadPreviousGeneration) {
    foreach ($newFile in Get-ChildItem -LiteralPath $generatedRoot -File) {
        $newPath = $newFile.FullName
        $previousName = $newFile.Name
        $previousFile = Join-Path $previousGeneratedRoot $previousName
        if (-not (Test-Path -LiteralPath $previousFile -PathType Leaf)) {
            continue
        }
        if ((Get-Item -LiteralPath $previousFile).Length -ne $newFile.Length) {
            continue
        }
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $previousFile).Hash -ne
            (Get-FileHash -Algorithm SHA256 -LiteralPath $newPath).Hash) {
            continue
        }
        Copy-Item -LiteralPath $previousFile -Destination $newPath -Force
        ++$reusedFiles
    }
    Remove-Item -LiteralPath $previousGeneratedRoot -Recurse -Force
}

$generatedFiles = Get-ChildItem -LiteralPath $generatedRoot -File
[pscustomobject]@{
    XexSha256 = $actualXexHash
    GeneratedFiles = $generatedFiles.Count
    GeneratedBytes = ($generatedFiles | Measure-Object -Property Length -Sum).Sum
    ReusedFiles = $reusedFiles
    AddressTakenFunctions = $addressTakenFunctions.Count
    StdoutLog = $stdoutLog
    StderrLog = $stderrLog
} | Format-List

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CaptureDirectory,

    [string]$MapPath,

    [string]$XeniaRoot = (Join-Path $PSScriptRoot '..\.tools\upstream\xenia'),

    [string]$OutputDirectory = (Join-Path $PSScriptRoot '..\out\shader-cache\ac6-xenia'),

    [ValidateRange(1, 64)]
    [int]$ThrottleLimit = 8,

    [ValidateRange(0, 100000)]
    [int]$MaxShaders = 0,

    [switch]$GenerateDxbcText
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$expectedXeniaCommit = '95a5c3ee250f80c3b9d139658649d9ffb6db3eec'
$xeniaRootResolved = (Resolve-Path -LiteralPath $XeniaRoot).Path
$captureDirectoryResolved = (Resolve-Path -LiteralPath $CaptureDirectory).Path
$compilerPath = Join-Path $xeniaRootResolved 'build\bin\Windows\Release\xenia-gpu-shader-compiler.exe'

if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
    throw "Pinned Xenia shader compiler not found: $compilerPath"
}

$actualXeniaCommit = (& git -C $xeniaRootResolved rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actualXeniaCommit -ne $expectedXeniaCommit) {
    throw "Xenia commit mismatch. Expected $expectedXeniaCommit, found $actualXeniaCommit"
}

if ([string]::IsNullOrWhiteSpace($MapPath)) {
    $mapFile = Get-ChildItem -LiteralPath $captureDirectoryResolved -Filter 'ac6-xenos-shader-map-*.jsonl' -File |
        Sort-Object LastWriteTimeUtc -Descending |
        Select-Object -First 1
    if ($null -eq $mapFile) {
        throw "No AC6 Xenos shader map found in $captureDirectoryResolved"
    }
    $MapPath = $mapFile.FullName
}

$mapPathResolved = (Resolve-Path -LiteralPath $MapPath).Path
$mapName = [System.IO.Path]::GetFileName($mapPathResolved)
if ($mapName -notmatch '^ac6-xenos-shader-map-(?<pid>\d+)\.jsonl$') {
    throw "Unexpected shader map name: $mapName"
}
$capturePid = [uint32]$Matches.pid

$allMapRows = @(
    Get-Content -LiteralPath $mapPathResolved |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { $_ | ConvertFrom-Json }
)
if ($allMapRows.Count -eq 0) {
    throw "Shader map is empty: $mapPathResolved"
}

# A vertex translation may also cause XeO3 to compile a derived geometry
# shader while the same ucode context is active. Keep those rows as capture
# evidence, but don't compare vertex ucode semantics against generated GS HLSL.
$mapRows = @(
    $allMapRows | Where-Object {
        ($_.stage_name -eq 'vertex' -and $_.target_profile -like 'vs_*') -or
        ($_.stage_name -eq 'pixel' -and $_.target_profile -like 'ps_*')
    }
)
if ($mapRows.Count -eq 0) {
    throw "Shader map has no stage-aligned VS/PS rows: $mapPathResolved"
}

$shaderItems = foreach ($row in $mapRows) {
    $stageName = [string]$row.stage_name
    if ($stageName -notin @('vertex', 'pixel')) {
        throw "Unsupported Xenos stage '$stageName' at sequence $($row.xenos_sequence)"
    }

    $ucodeSha256 = ([string]$row.ucode_sha256).ToUpperInvariant()
    $ucodeName = 'ac6-xenos-ucode-{0}-{1:D6}-{2}-{3}.bin' -f @(
        $capturePid,
        [uint64]$row.xenos_sequence,
        $stageName,
        $ucodeSha256
    )
    $ucodePath = Join-Path $captureDirectoryResolved $ucodeName
    if (-not (Test-Path -LiteralPath $ucodePath -PathType Leaf)) {
        throw "Missing captured ucode: $ucodePath"
    }

    $ucodeFile = Get-Item -LiteralPath $ucodePath
    if ($ucodeFile.Length -ne [uint64]$row.ucode_size) {
        throw "Ucode size mismatch for $ucodeName"
    }
    $actualUcodeSha256 = (Get-FileHash -LiteralPath $ucodePath -Algorithm SHA256).Hash
    if ($actualUcodeSha256 -ne $ucodeSha256) {
        throw "Ucode hash mismatch for $ucodeName"
    }

    [pscustomobject]@{
        XenosSequence  = [uint64]$row.xenos_sequence
        CompileSequence = [uint64]$row.compile_sequence
        Stage          = $stageName
        StageFlag      = if ($stageName -eq 'vertex') { 'vs' } else { 'ps' }
        UcodeSize      = [uint64]$row.ucode_size
        UcodeSha256    = $ucodeSha256
        UcodePath      = $ucodePath
        TargetProfile  = [string]$row.target_profile
        HlslSize       = [uint64]$row.hlsl_size
        HlslSha256     = ([string]$row.hlsl_sha256).ToUpperInvariant()
    }
}

$uniqueShaders = @(
    $shaderItems |
        Group-Object Stage, UcodeSha256 |
        ForEach-Object { $_.Group | Sort-Object XenosSequence | Select-Object -First 1 } |
        Sort-Object XenosSequence
)
if ($MaxShaders -gt 0) {
    $uniqueShaders = @($uniqueShaders | Select-Object -First $MaxShaders)
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$outputDirectoryResolved = (Resolve-Path -LiteralPath $OutputDirectory).Path
$generateDxbcTextValue = [bool]$GenerateDxbcText

$analysisResults = @(
    $uniqueShaders | ForEach-Object -Parallel {
        $shader = $_
        $baseName = '{0}-{1}' -f $shader.Stage, $shader.UcodeSha256
        $ucodeOutputPath = Join-Path $using:outputDirectoryResolved "$baseName.ucode.txt"
        $ucodeLogPath = Join-Path $using:outputDirectoryResolved "$baseName.ucode.log"

        $ucodeArguments = @(
            "--shader_input=$($shader.UcodePath)"
            "--shader_input_type=$($shader.StageFlag)"
            "--shader_output=$ucodeOutputPath"
            '--shader_output_type=ucode'
            '--log_file=stdout'
        )
        $ucodeOutput = & $using:compilerPath @ucodeArguments 2>&1
        $ucodeExitCode = $LASTEXITCODE
        $ucodeOutput | Set-Content -LiteralPath $ucodeLogPath -Encoding utf8

        $dxbcOutputPath = ''
        $dxbcLogPath = ''
        $dxbcExitCode = 0
        if ($using:generateDxbcTextValue) {
            $dxbcOutputPath = Join-Path $using:outputDirectoryResolved "$baseName.dxbc.txt"
            $dxbcLogPath = Join-Path $using:outputDirectoryResolved "$baseName.dxbc.log"
            $dxbcArguments = @(
                "--shader_input=$($shader.UcodePath)"
                "--shader_input_type=$($shader.StageFlag)"
                "--shader_output=$dxbcOutputPath"
                '--shader_output_type=dxbctext'
                '--log_file=stdout'
            )
            $dxbcOutput = & $using:compilerPath @dxbcArguments 2>&1
            $dxbcExitCode = $LASTEXITCODE
            $dxbcOutput | Set-Content -LiteralPath $dxbcLogPath -Encoding utf8
        }

        [pscustomobject]@{
            Stage           = $shader.Stage
            UcodeSha256     = $shader.UcodeSha256
            UcodeOutputPath = $ucodeOutputPath
            UcodeExitCode   = $ucodeExitCode
            DxbcOutputPath  = $dxbcOutputPath
            DxbcExitCode    = $dxbcExitCode
        }
    } -ThrottleLimit $ThrottleLimit
)

$failedUcode = @(
    $analysisResults | Where-Object {
        $_.UcodeExitCode -ne 0 -or
        -not (Test-Path -LiteralPath $_.UcodeOutputPath -PathType Leaf) -or
        (Get-Item -LiteralPath $_.UcodeOutputPath).Length -eq 0
    }
)
if ($failedUcode.Count -ne 0) {
    $failedNames = ($failedUcode | ForEach-Object { "$($_.Stage):$($_.UcodeSha256)" }) -join ', '
    throw "Xenia ucode analysis failed for $($failedUcode.Count) shader(s): $failedNames"
}

$resultByKey = @{}
foreach ($result in $analysisResults) {
    $resultByKey["$($result.Stage):$($result.UcodeSha256)"] = $result
}
$selectedKeys = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
foreach ($shader in $uniqueShaders) {
    [void]$selectedKeys.Add("$($shader.Stage):$($shader.UcodeSha256)")
}

$indexRows = foreach ($shader in $shaderItems) {
    $key = "$($shader.Stage):$($shader.UcodeSha256)"
    if (-not $selectedKeys.Contains($key)) {
        continue
    }

    $analysis = $resultByKey[$key]
    $hlslName = 'ac6-shader-{0}-{1:D4}-{2}-original.hlsl' -f @(
        $capturePid,
        $shader.CompileSequence,
        $shader.TargetProfile
    )
    $hlslPath = Join-Path $captureDirectoryResolved $hlslName
    $hlslHashValid = $false
    if (Test-Path -LiteralPath $hlslPath -PathType Leaf) {
        $hlslFile = Get-Item -LiteralPath $hlslPath
        $actualHlslSha256 = (Get-FileHash -LiteralPath $hlslPath -Algorithm SHA256).Hash
        $hlslHashValid = $hlslFile.Length -eq $shader.HlslSize -and $actualHlslSha256 -eq $shader.HlslSha256
    }

    [pscustomobject]@{
        XenosSequence   = $shader.XenosSequence
        CompileSequence = $shader.CompileSequence
        Stage           = $shader.Stage
        UcodeSize       = $shader.UcodeSize
        UcodeSha256     = $shader.UcodeSha256
        UcodePath       = $shader.UcodePath
        XeniaUcodePath  = $analysis.UcodeOutputPath
        XeniaUcodeExit  = $analysis.UcodeExitCode
        XeniaDxbcPath   = $analysis.DxbcOutputPath
        XeniaDxbcExit   = $analysis.DxbcExitCode
        HlslPath        = $hlslPath
        HlslHashValid   = $hlslHashValid
        HlslSha256      = $shader.HlslSha256
    }
}

$uniqueIndexPath = Join-Path $outputDirectoryResolved 'unique-shaders.tsv'
$captureIndexPath = Join-Path $outputDirectoryResolved 'capture-map.tsv'
$summaryPath = Join-Path $outputDirectoryResolved 'summary.json'

$analysisResults |
    Sort-Object Stage, UcodeSha256 |
    Export-Csv -LiteralPath $uniqueIndexPath -Delimiter "`t" -NoTypeInformation -Encoding utf8
$indexRows |
    Sort-Object XenosSequence |
    Export-Csv -LiteralPath $captureIndexPath -Delimiter "`t" -NoTypeInformation -Encoding utf8

$summary = [ordered]@{
    schema                  = 'ac6_xenia_shader_analysis_v1'
    xenia_commit            = $actualXeniaCommit
    compiler_sha256         = (Get-FileHash -LiteralPath $compilerPath -Algorithm SHA256).Hash
    capture_pid             = $capturePid
    shader_map              = $mapPathResolved
    map_rows                = $allMapRows.Count
    stage_aligned_map_rows  = $mapRows.Count
    derived_shader_rows     = $allMapRows.Count - $mapRows.Count
    selected_unique_shaders = $uniqueShaders.Count
    vertex_shaders          = @($uniqueShaders | Where-Object Stage -eq 'vertex').Count
    pixel_shaders           = @($uniqueShaders | Where-Object Stage -eq 'pixel').Count
    ucode_failures          = $failedUcode.Count
    dxbc_text_requested     = $generateDxbcTextValue
    dxbc_nonzero_exits      = @($analysisResults | Where-Object DxbcExitCode -ne 0).Count
    invalid_hlsl_captures   = @($indexRows | Where-Object { -not $_.HlslHashValid }).Count
    unique_index            = $uniqueIndexPath
    capture_index           = $captureIndexPath
}
$summary | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $summaryPath -Encoding utf8

$summary | Format-List

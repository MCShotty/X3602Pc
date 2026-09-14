param(
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [string]$EmuPath = 'D:\Games\AC6 shit\XeO3-AC6-lab\Emu.exe',
    [string]$VgpuPath,
    [string]$KernelPath =
        'D:\Games\AC6 shit\XeO3-AC6-lab\Flash\xboxkrnlcf.bin',
    [string]$KernelAotPath,
    [string]$AotDllPath,
    [string]$AotPdbPath,
    [string]$BuildRoot,
    [string]$GeneratedRoot,
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $AotDllPath) {
    $AotDllPath = Join-Path $repoRoot (
        'build\ac6-aot\aot\' +
        'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
    )
}
if (-not $AotPdbPath) {
    $AotPdbPath = [IO.Path]::ChangeExtension($AotDllPath, '.pdb')
}
if (-not $BuildRoot) {
    $BuildRoot = Join-Path $repoRoot 'build\ac6-aot'
}
if (-not $GeneratedRoot) {
    $GeneratedRoot = Join-Path $repoRoot 'out\ac6\generated'
}
if (-not $OutputPath) {
    $OutputPath = Join-Path $repoRoot 'out\manifests\ac6-build-manifest.json'
}

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Get-Artifact([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        return [ordered]@{
            path = $Path
            bytes = $null
            sha256 = $null
        }
    }
    $item = Get-Item -LiteralPath $Path
    return [ordered]@{
        path = $item.FullName
        bytes = $item.Length
        sha256 = Get-Sha256 $item.FullName
    }
}

function Get-FirstOutputLine([string]$Executable, [string[]]$Arguments) {
    if (-not (Test-Path -LiteralPath $Executable)) {
        return $null
    }
    return (& $Executable @Arguments 2>&1 | Select-Object -First 1).ToString().Trim()
}

function Get-NinjaObjectFlags(
    [string[]]$BuildLines,
    [string]$ObjectSourceName
) {
    for ($lineIndex = 0; $lineIndex -lt $BuildLines.Count; $lineIndex++) {
        if ($BuildLines[$lineIndex] -notlike "build *$ObjectSourceName.obj:*") {
            continue
        }
        $lastLine = [Math]::Min($lineIndex + 12, $BuildLines.Count - 1)
        for ($propertyIndex = $lineIndex + 1;
            $propertyIndex -le $lastLine;
            $propertyIndex++) {
            if ($BuildLines[$propertyIndex] -match '^\s*FLAGS = (?<flags>.+)$') {
                return $Matches.flags.Trim()
            }
        }
        break
    }
    return $null
}

$clang = 'C:\Program Files\LLVM\bin\clang-cl.exe'
$ninjaCommand = Get-Command ninja.exe -ErrorAction Stop
$cmakeCommand = Get-Command cmake.exe -ErrorAction Stop
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsInstall = $null
if (Test-Path -LiteralPath $vswhere) {
    $vsInstall = & $vswhere `
        -latest `
        -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
}

$msvcRoot = $null
$ml64 = $null
if ($vsInstall) {
    $msvcRoot = Get-ChildItem -LiteralPath (Join-Path $vsInstall 'VC\Tools\MSVC') `
        -Directory |
        Sort-Object { [version]$_.Name } -Descending |
        Select-Object -First 1
    if ($msvcRoot) {
        $ml64 = Join-Path $msvcRoot.FullName 'bin\Hostx64\x64\ml64.exe'
    }
}

$sdkRoot = 'C:\Program Files (x86)\Windows Kits\10\bin'
$sdkVersion = Get-ChildItem -LiteralPath $sdkRoot -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending |
    Select-Object -First 1

$package = Get-AppxPackage `
    -Name 'Xbox360BackwardCompatibil.PrimaryFuzionFrenzyFuzio' `
    -ErrorAction Stop
if (-not $VgpuPath) {
    $VgpuPath = Join-Path $package.InstallLocation 'VGPUDX12.dll'
}
if (-not $KernelAotPath) {
    $KernelAotPath = Join-Path (Split-Path -Parent $EmuPath) 'xeo3_5fb3687c_001748c4.dll'
}
if ($package.Version -ne [version]'2608.3123.1.0') {
    throw "Unsupported XeO3 package version: $($package.Version)"
}

$expectedHashes = [ordered]@{
    $XexPath = '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'
    $EmuPath = 'D1578E07B533E391D8A81C330D5493BA2D45B252490A818EC148DABE1BA24D06'
    $VgpuPath = '8306B4C06B100CAE18F91DCCD0468C2210CCA11A02D928025DE59BD827610247'
    $KernelAotPath = '27CA5876B505361F00C3E1021FF06B68CD666987D4B517C934D69AADF44E8651'
    $KernelPath = 'DA5BE614FB51B5809D70DA073F406F071E5CCB1F8C0EBCD57DAFBAB31B519BDD'
}
foreach ($entry in $expectedHashes.GetEnumerator()) {
    $actualHash = Get-Sha256 $entry.Key
    if ($actualHash -ne $entry.Value) {
        throw "Pinned input hash mismatch for $($entry.Key): expected $($entry.Value), found $actualHash"
    }
}

$branch = (& git -C $repoRoot symbolic-ref --short HEAD 2>$null).Trim()
$repositoryCommit = $null
$branchRefPath = Join-Path $repoRoot ".git\refs\heads\$branch"
if (Test-Path -LiteralPath $branchRefPath) {
    $repositoryCommit = [System.IO.File]::ReadAllText($branchRefPath).Trim()
} else {
    $packedRefsPath = Join-Path $repoRoot '.git\packed-refs'
    if (Test-Path -LiteralPath $packedRefsPath) {
        $packedRef = Get-Content -LiteralPath $packedRefsPath |
            Where-Object { $_ -match "^[0-9a-f]{40} refs/heads/$([Regex]::Escape($branch))$" } |
            Select-Object -First 1
        if ($packedRef) {
            $repositoryCommit = $packedRef.Substring(0, 40)
        }
    }
}
$xenonRecompCommit = (
    & git -C (Join-Path $repoRoot 'third_party\XenonRecomp') rev-parse HEAD
).Trim()
$xeniaReferencePath = Join-Path $repoRoot 'out\reference\xenia'
$xeniaReferenceCommit = if (Test-Path -LiteralPath $xeniaReferencePath) {
    (& git -C $xeniaReferencePath rev-parse HEAD).Trim()
} else {
    $null
}

$patches = @(
    Get-ChildItem (Join-Path $repoRoot 'patches\xenonrecomp') -Filter '*.patch' |
        Sort-Object Name
)
$patchManifest = @(
    foreach ($patch in $patches) {
        [ordered]@{
            name = $patch.Name
            sha256 = Get-Sha256 $patch.FullName
        }
    }
)
$patchSeriesMaterial = (
    $patchManifest |
        ForEach-Object { "$($_.name):$($_.sha256)" }
) -join "`n"
$sha256 = [Security.Cryptography.SHA256]::Create()
try {
    $patchSeriesBytes = [Text.Encoding]::UTF8.GetBytes($patchSeriesMaterial)
    $patchSeries = -join (
        $sha256.ComputeHash($patchSeriesBytes) |
            ForEach-Object { $_.ToString('x2') }
    )
} finally {
    $sha256.Dispose()
}

$generatedSources = @(
    Get-ChildItem -LiteralPath $GeneratedRoot -Filter 'ppc_recomp.*.cpp' -File
)
$gvnWorkaroundSource = $generatedSources |
    Select-String -Pattern 'PPC_FUNC_IMPL\(__imp____restvmx_65\)' |
    Select-Object -First 1
$publicationChecker = Join-Path $BuildRoot 'ac6_generated_state_check.exe'
if (-not (Test-Path -LiteralPath $publicationChecker -PathType Leaf)) {
    throw "Generated-state checker is missing: $publicationChecker"
}
$publicationOutput = (
    & $publicationChecker $GeneratedRoot 2>&1 |
        Out-String
).Trim()
if ($LASTEXITCODE -ne 0) {
    throw "Generated-state validation failed: $publicationOutput"
}
$publicationCounts = [ordered]@{}
foreach ($match in [Regex]::Matches($publicationOutput, '(\w+)=(\d+)')) {
    $publicationCounts[$match.Groups[1].Value] =
        [uint64]$match.Groups[2].Value
}
$mappingPath = Join-Path $GeneratedRoot 'ppc_func_mapping.cpp'
$mappingCount = if (Test-Path -LiteralPath $mappingPath) {
    @(
        Select-String -LiteralPath $mappingPath -Pattern '^\s*\{\s*0x'
    ).Count
} else {
    $null
}
$importsPath = Join-Path $GeneratedRoot 'ac6_imports.inc'
$importCount = if (Test-Path -LiteralPath $importsPath) {
    @(
        Select-String -LiteralPath $importsPath -Pattern '^XEO3_AC6_IMPORT\('
    ).Count
} else {
    $null
}
$addressTakenPath = Join-Path $repoRoot 'configs\ac6\address_taken_functions.tsv'
$addressTakenFunctions = @(
    Import-Csv -Delimiter "`t" -LiteralPath $addressTakenPath
)
if ($addressTakenFunctions.Count -ne 404 -or
    @(
        $addressTakenFunctions |
            Where-Object { [uint32]$_.table_pointer_count -eq 0 }
    ).Count -ne 0) {
    throw "Pinned AC6 address-taken function list is invalid: $addressTakenPath"
}
$cmakeCachePath = Join-Path $BuildRoot 'CMakeCache.txt'
$buildType = $null
$continuousStatePublicationEnabled = $false
if (Test-Path -LiteralPath $cmakeCachePath) {
    $continuousStatePublicationEnabled = [bool](
        Get-Content -LiteralPath $cmakeCachePath |
            Where-Object { $_ -match '^XEO3_CONTINUOUS_STATE_PUBLICATION:BOOL=(ON|TRUE|1)$' }
    )
    $buildTypeLine = Get-Content -LiteralPath $cmakeCachePath |
        Where-Object { $_ -match '^CMAKE_BUILD_TYPE:' } |
        Select-Object -First 1
    if ($buildTypeLine) {
        $buildType = ($buildTypeLine -split '=', 2)[1]
    }
}
$buildNinjaPath = Join-Path $BuildRoot 'build.ninja'
if (-not (Test-Path -LiteralPath $buildNinjaPath -PathType Leaf)) {
    throw "Ninja build description is missing: $buildNinjaPath"
}
$buildLines = Get-Content -LiteralPath $buildNinjaPath
$normalGeneratedSource = $generatedSources |
    Where-Object {
        -not $gvnWorkaroundSource -or
        $_.FullName -ne $gvnWorkaroundSource.Path
    } |
    Select-Object -First 1
$normalGeneratedFlags = Get-NinjaObjectFlags `
    $buildLines `
    $normalGeneratedSource.Name
$bridgeFlags = Get-NinjaObjectFlags $buildLines 'xeo3_bridge.cpp'
$gvnWorkaroundFlags = if ($gvnWorkaroundSource) {
    Get-NinjaObjectFlags `
        $buildLines `
        ([IO.Path]::GetFileName($gvnWorkaroundSource.Path))
} else {
    $null
}
if ($buildType -in @('Release', 'RelWithDebInfo')) {
    if ($normalGeneratedFlags -notmatch '(?:^|\s)/O2(?:\s|$)') {
        throw 'Generated PPC code was not compiled with /O2.'
    }
    if ($bridgeFlags -notmatch '(?:^|\s)/O2(?:\s|$)') {
        throw 'The XeO3 bridge was not compiled with /O2.'
    }
    if ($gvnWorkaroundSource -and
        $gvnWorkaroundFlags -notmatch '(?:^|\s)/O1(?:\s|$)') {
        throw 'The __restvmx_65 LLVM workaround shard was not compiled with /O1.'
    }
}
$deployedDllPath = Join-Path (
    Split-Path -Parent $EmuPath
) 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$edramScaleShaderArtifact = Get-Artifact (
    Join-Path $BuildRoot 'generated\ac6_edram_scale_fix.dxil'
)
$pso341WidthFixShaderArtifact = Get-Artifact (
    Join-Path $repoRoot 'out\pix\pso341-cs-width-fix-rooted.dxil'
)

$manifest = [ordered]@{
    schemaVersion = 30
    generatedAtUtc = [DateTime]::UtcNow.ToString('o')
    repository = [ordered]@{
        branch = $branch
        commit = $repositoryCommit
        dirty = @(& git -C $repoRoot status --porcelain).Count -ne 0
        xenonRecompCommit = $xenonRecompCommit
        xenonRecompPinnedCommit = 'ddd128bcca99fe8bfbb99bea583c972351fa6ace'
        xeniaSemanticsReference = [ordered]@{
            repository = 'https://github.com/xenia-project/xenia'
            commit = $xeniaReferenceCommit
            pinnedCommit = '95a5c3ee250f80c3b9d139658649d9ffb6db3eec'
        }
        xenonRecompPatchSeries = $patchSeries
        xenonRecompPatches = $patchManifest
    }
    target = [ordered]@{
        title = 'Ace Combat 6'
        titleId = '4E4D07D1'
        imageBase = '0x82000000'
        imageSize = '0x00AA0000'
        entryPoint = '0x821F5ED0'
        requiredDllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
        xex = [ordered]@{
            path = $XexPath
            sha256 = Get-Sha256 $XexPath
        }
    }
    host = [ordered]@{
        packageName = $package.Name
        packageFamilyName = $package.PackageFamilyName
        packageVersion = $package.Version.ToString()
        vdSwapTrace = [ordered]@{
            enabledByDefault = $false
            importThunk = '0x823D05BC'
            maximumSubmissions = 16
            behavior = 'read-only before/after snapshots of native VdSwap fetch, ringbuffer and argument memory; no guest writes'
            filePattern = 'ProbeLogs/ac6-vdswap-PID.jsonl'
            telemetry = @('BridgeVdSwapTraceEnabled', 'BridgeVdSwapTraceCount', 'BridgeVdSwapTraceFailure')
        }
        emu = [ordered]@{
            path = $EmuPath
            sha256 = Get-Sha256 $EmuPath
            unmappedIarObserver = [ordered]@{
                guardedRva = '0x0005FE78'
                fatalFormatRva = '0x0003B780'
                fatalMessageRva = '0x000FE468'
                fatalTrapRva = '0x0005FE88'
                expectedBytes =
                    '488B5778488D0DE5E50900E8F8B8FDFF'
                identityGate =
                    'PE timestamp, image size, and exact Emu SHA-256'
                logSuffix = '.unmapped-iar.log'
                behavior =
                    'record rejected guest IAR and dispatch slot, then preserve native fatal path'
            }
        }
        vgpuDx12 = [ordered]@{
            path = $VgpuPath
            sha256 = Get-Sha256 $VgpuPath
            g2hContextTrace = [ordered]@{
                enabledByDefault = $false
                maximumRecords = 1024
                requiresCallerRvas = @('0x92AA', '0xCC44')
                savedMetadataStackOffset = '0x28'
                behavior = 'read-only before/after endian fields and native texture metadata; both callers, fixed instructions, frame geometry and format must match'
                filePattern = 'ProbeLogs/ac6-g2h-context-PID.jsonl'
                telemetry = @('BridgeVgpuG2HTraceEnabled', 'BridgeVgpuG2HTraceCount', 'BridgeVgpuG2HTraceFailure')
            }
            pixEdramBoundFallback = [ordered]@{
                requiresModule = 'WinPixGpuCapturer.dll'
                opaqueCachedBlobSize = 848
                opaqueCachedBlobSha256 = '13C9E5C82BA93ED0C1BE12D4137ECD15F8A145E44E02DCE5205DF21A13B4E7B2'
                identity = 'per-draw exact EDRAM constants, draw shape, UINT RTV, sample count and bound root tables; opaque blob is never cached as shader identity'
                createRenderTargetViewSlot = 20
                omSetRenderTargetsSlot = 46
                descriptorCopySlots = @(23, 24)
                nonConstantViewInvalidationSlots = @(18, 19)
                descriptorShadow = 'copy GPU address values from tracked or pinned fixed-pool CBVs; invalidate non-CBV replacements; bounded allocation-free lookup'
                normalCachedBlobPathUnchanged = $true
                telemetry = @('BridgeVgpuPixEdramBoundMatchCount', 'BridgeVgpuPixEdramBoundRejectCount', 'BridgeVgpuPixEdramResolveFailure', 'BridgeVgpuRtvHookInstalled', 'BridgeVgpuRtvHookFailure', 'BridgeVgpuPixDescriptorCopyCount', 'BridgeVgpuPixConstantCopyCount', 'BridgeVgpuPixDescriptorHookFailure')
            }
            failedPipelineCapture = [ordered]@{
                maximumFailures = 32
                artifacts = @('VS', 'PS', 'GS', 'HS', 'DS', 'pointer-redacted x64 descriptor', 'D3D12 info queue')
                latchedTelemetry = @('BridgeVgpuPipelineStateLastFailureResult', 'BridgeVgpuPipelineStateLastFailureCreateSequence')
            }
            aircraftRestartCorrection = [ordered]@{
                sourceBytes = 4807
                sourceSha256 = '50680B1FA845879FD68EB39A34B8482036625E13B8D306BABF94CDC42D8E2A4A'
                behavior = 'restart-relative strip parity and rejection of reset-index triangles, using the same bounded resolver as the pinned sky and terrain shaders'
                telemetry = 'BridgeVgpuAircraftRestartShaderCount'
                evidence = 'PSO560 readback changes only the aircraft region; captured 7668-index draw has 1638 restart markers and maximum segment length 11'
            }
            shadowRestartCorrection = [ordered]@{
                sourceBytes = 4299
                restartScanLimit = 128
                maximumCapturedSegmentLength = 105
                sourceSha256 = '6C0EDF6CA947E47A849C87FBD64C52C194CB134D7D2BDBEF02A7904FAD56FE42'
                behavior = 'restart-relative winding for the pinned stencil shadow-volume shader; retain native stencil increment/decrement and shadow application'
                telemetry = 'BridgeVgpuShadowRestartShaderCount'
                evidence = 'PSO534 A/B changes the broken self-shadow revealed by PSO537 draw 1273; no culling, stencil, or pixel-shader state is changed'
            }
            drawLocalVertexId = [ordered]@{
                behavior = 'SV_VertexID already excludes StartVertexLocation; preserve it for primitive expansion and apply the guest vertexOffset only after index lookup'
                specification = 'https://microsoft.github.io/hlsl-specs/proposals/0015-extended-command-info/'
                evidence = 'Post-VS stream output for terrain bases 0, 160 and 2840 matches all 33774 predicted positions and clipping flags after removing the extra subtraction'
            }
            textureUnpackEndianCorrection = [ordered]@{
                enabledByDefault = $true
                exactSignatures = @(@(0, 2, 4, 1, 6, 14400), @(0, 2, 4, 1, 6, 468))
                replacementEndian = 0
                scopes = @('1280x720 full frame', '208x144 target preview')
                telemetry = @('BridgeVgpuTextureEndianFixCount', 'BridgeVgpuTexturePreviewEndianFixCount')
                behavior = 'change only the endian word for the two exact transfer signatures; retain all other texture transfers and native synchronization'
                evidence = '208x144 replay correction changes exactly 29952 pixels within [980,153,1188,297); the remainder of the final frame is byte-identical'
            }
            nullPipelineStateGuard = [ordered]@{
                guardedRva = '0x0000E926'
                normalResumeRva = '0x0000E938'
                skippedDrawResumeRva = '0x0000E90E'
                expectedBytes =
                    '488993181000008B0348C1E005488B4C1808'
                behavior =
                    'drop null-PSO draw, clear active-record flag, retain cached PSO'
                crashSignature =
                    'VGPUDX12+0xE941 SetPipelineState(nullptr), amdxc64 null read'
            }
            corruptEdramRestoreDrawGuard = [ordered]@{
                guardedRva = '0x0000E926'
                skippedDrawResumeRva = '0x0000E90E'
                cachedBlobBytes = 954
                cachedBlobSha256 =
                    '52737028BAFA144C68484A495F7572F1179EA7B9F1188FB99AD6A99061DD28C4'
                d3d12SdkVersion = 618
                structuralSignature =
                    '1280x720 viewport, 640x360 scissor, record kind 0, triangle vertex count 3'
                behavior =
                    'drop only the exact fingerprinted corrupt EDRAM restore draw'
            }
            hostEdramRestoreDrawGuard = [ordered]@{
                enabledByDefault = $false
                cachedBlobBytes = 954
                acceptedLoadPipelineSha256 = @(
                    '5EE6E2C421A6E2F131171E1B2E1C2B5E6F77F2A1E36B25DD998A5912AF179711',
                    'C7CAC6C6B475390000448ABE0683153966873EA73F3FC0CC498B54909735F674'
                )
                structuralSignature =
                    '1280x720 viewport, 640x360 scissor, depth 0..1, triangle vertex count 3'
                behavior =
                    'drop the fingerprinted host PSO523 load draw that overwrites only the top-left 640x360 quadrant'
                evidence =
                    'PIX event 2051 suppression preserves resource 329 offline; live A/B removes the exact teal top-left 640x360 quadrant while leaving the independent PSO341 corruption visible'
            }
            pso341TaskWidthCorrection = [ordered]@{
                enabledByDefault = $true
                constantUploadBytes = 768
                brokenPackedDimensions = '0x02D00000'
                correctedPackedDimensions = '0x02D00500'
                structuralSignature =
                    'task index 0; words 0..7 are 1280, 720, 0x02D00000, 1, 5120, 0, 0, 0; metadata word 128 is 14400 and words 129..143 are 0xFFFFFFFF'
                computeShader = [ordered]@{
                    bytes = 5032
                    sha256 =
                        'B15D25CB2A2052A52DCB65689B1ED54D2AA38E84F40E753684561C1039ED4A54'
                }
                cachedPipelineFingerprint = [ordered]@{
                    driver = 'AMD UMD 32.0.31041.1004'
                    bytes = 954
                    sha256 =
                        '75C7F44B784A9F7A2DE0216A0F460EF0407F4F5E835F6F8604F1B70BF0C050CA'
                    replayCount = 2
                    liveSha256 =
                        '2D34187A020B37A69B0B077BFC08005AD0A7A530337DBAF4CF9C0E6BE7B36AE4'
                    liveRunCount = 2
                    liveEvidence =
                        'candidate 2 is stable across two XeO3 processes, alternates with PSO333 at the captured 56:64 cadence, and binds the distinct PSO341 compute root signature corresponding to PIX root 343'
                }
                replacementComputeShader = [ordered]@{
                    enabledByDefault = $false
                    bytes = $pso341WidthFixShaderArtifact.bytes
                    sha256 = $pso341WidthFixShaderArtifact.sha256
                    rootSignature =
                        'pinned RTS0 restored from the original PSO341 DXIL and selected through a null D3D12 PSO root pointer; XeO3 command root 343 is compatible for dispatch but invalid for PSO creation'
                }
                behavior =
                    'keep the replacement PSO disabled by default; forcing width 1280 through either the broad or exact-signature DXIL variant hangs the pinned AMD device, so production retains XeO3 PSO341 while the task/output addressing is diagnosed'
                evidence =
                    'captured replay resource 321 at offset 167424 supplies the full gated signature before event 1661; the PSO creates successfully, but live runs 14276 and 4224 both end in DXGI_ERROR_DEVICE_HUNG and VGPUDX12+0x17DC8 after substitution'
            }
            screenSpaceVposCorrection = [ordered]@{
                enabledByDefault = $true
                behavior =
                    'replace generated pixel/geometry shader vpos_Scale uses with native-resolution float2(1,1); never alias to pixel-to-NDC vport_Scale'
                sceneShader = [ordered]@{
                    sourceBytes = 3351
                    sourceSha256 =
                        '100A6BAAAC3357530C7B6994E1FFD58379B077CD66E59DADF6D2063FC93511F9'
                    halfWidthUvEnabledByDefault = $true
                    behavior =
                        'halve only the fingerprinted PSO537 normalized scene-sample U coordinate after native-resolution VPOS correction'
                }
                validation =
                    'captured PSO537, particle pixel, and particle geometry shaders compile successfully with Windows SDK 10.0.26100.0 DXC'
            }
            edramScaleCorrection = [ordered]@{
                vertexShaderBytes = 2224
                vertexShaderSha256 =
                    '7F3F8E0EEC4028ECCFBF2E63621B28049528ECFAE2ECEC5045B5D65E72CBA81E'
                pixelShaderBytes = 2528
                pixelShaderSha256 =
                    '1E8874A8EE005E724FB5C455E2F03507B41659E6124678E9412C87305B496251'
                replacementShaderBytes = $edramScaleShaderArtifact.bytes
                replacementShaderSha256 = $edramScaleShaderArtifact.sha256
                behavior =
                    'read XeO3 pair-swizzled EDRAM at native destination coordinates and unpack RGBA8 channels'
                integration =
                    'pre-title PSO is observed at draw-record time; expand only an exact 1280x720 viewport and 640x720 fullscreen scissor record to 1280x720'
                sceneUvHalfWidth = $true
            }
        }
        flashKernel = [ordered]@{
            path = $KernelPath
            sha256 = Get-Sha256 $KernelPath
        }
        kernelAot = [ordered]@{
            path = $KernelAotPath
            sha256 = Get-Sha256 $KernelAotPath
            imageTimestamp = '0x6A588000'
            imageSize = '0x0013E000'
        }
        profile = 'profiles/xeo3/2608.3123.1.0-D1578E07.json'
        kernelContinuation = [ordered]@{
            guestIar = '0x8005F0B4'
            effectiveEntry = '0x8005F0B8'
            nativeBugCheckExTarget = '0x8005EEC8'
            semantics =
                'preserve r3 and LR, zero r4-r7, tail-branch to KeBugCheckEx'
            fingerprintSource =
                'flat Flash/xboxkrnlcf.bin plus live guest code pages'
            telemetryStages = [ordered]@{
                entered = 1
                nativeBugCheckExCall = 2
                unexpectedNativeReturn = 3
            }
            nativeHostFallback = [ordered]@{
                module = 'xeo3_5fb3687c_001748c4.dll'
                moduleSha256 =
                    '27CA5876B505361F00C3E1021FF06B68CD666987D4B517C934D69AADF44E8651'
                bugCheckExHostRva = '0x00021899'
                bugCheckExHostSha256 =
                    '7E106F74FB128E76C63F5C21CEC574BD040AE9A5F213D718AD871ED8B3D3B15C'
            }
            continuationSha256 =
                'E1321053B7D357FC2DA130D29AA1127A3641C2179594B980AC489E42159CB02B'
            bugCheckExSha256 =
                'ABCE00C593E34AF3A22EDD4DE0F6BDA0BB3042F817F6728091AE35BE991BB009'
        }
    }
    toolchain = [ordered]@{
        clangCl = [ordered]@{
            path = $clang
            version = Get-FirstOutputLine $clang @('--version')
        }
        ninja = [ordered]@{
            path = $ninjaCommand.Source
            version = Get-FirstOutputLine $ninjaCommand.Source @('--version')
        }
        cmake = [ordered]@{
            path = $cmakeCommand.Source
            version = Get-FirstOutputLine $cmakeCommand.Source @('--version')
        }
        visualStudio = [ordered]@{
            installationPath = $vsInstall
            msvcVersion = if ($msvcRoot) { $msvcRoot.Name } else { $null }
            ml64Path = $ml64
            ml64FileVersion = if ($ml64 -and (Test-Path -LiteralPath $ml64)) {
                (Get-Item -LiteralPath $ml64).VersionInfo.FileVersion
            } else {
                $null
            }
        }
        windowsSdkVersion = if ($sdkVersion) { $sdkVersion.Name } else { $null }
    }
    build = [ordered]@{
        configuration = $buildType
        buildRoot = [IO.Path]::GetFullPath($BuildRoot)
        cmakeCache = Get-Artifact $cmakeCachePath
        generated = [ordered]@{
            path = [IO.Path]::GetFullPath($GeneratedRoot)
            sourceFiles = $generatedSources.Count
            sourceBytes = (
                $generatedSources |
                    Measure-Object -Property Length -Sum
            ).Sum
            mappingCount = $mappingCount
            importCount = $importCount
            addressTakenFunctions = [ordered]@{
                count = $addressTakenFunctions.Count
                selection = 'clustered code-pointer table entries'
                artifact = Get-Artifact $addressTakenPath
            }
            decodedInstructions = $publicationCounts.iar
            generatedPublicationMarkers = [ordered]@{
                gpr = $publicationCounts.gpr
                fpr = $publicationCounts.fpr
                vector = $publicationCounts.vector
                condition = $publicationCounts.condition
                lr = $publicationCounts.lr
                ctr = $publicationCounts.ctr
                xer = $publicationCounts.xer
                fpscr = $publicationCounts.fpscr
                vscr = $publicationCounts.vscr
                msr = $publicationCounts.msr
            }
            publicationMarkerCoverageComplete = $true
            publicationChecker = Get-Artifact $publicationChecker
            recompilerStdout = Get-Artifact (
                Join-Path (Split-Path -Parent $GeneratedRoot) 'xenonrecomp.stdout.log'
            )
            recompilerStderr = Get-Artifact (
                Join-Path (Split-Path -Parent $GeneratedRoot) 'xenonrecomp.stderr.log'
            )
        }
        translatedCodeOptions = @(
            '/O2',
            '/Zi',
            '/MT',
            '/bigobj',
            '/W0',
            '-mmovbe',
            '/FI src/xeo3_bridge/xenonrecomp_overrides.h'
        )
        verifiedNativeCompileFlags = [ordered]@{
            generatedPpc = $normalGeneratedFlags
            bridge = $bridgeFlags
            llvmGvnWorkaroundShard = $gvnWorkaroundFlags
        }
        sourceOverrides = @(
            if ($gvnWorkaroundSource) {
                [ordered]@{
                    source = [IO.Path]::GetFileName(
                        $gvnWorkaroundSource.Path
                    )
                    options = @('/O1')
                    reason =
                        'LLVM 22.1.8 GVN access violation in __restvmx_65'
                }
            }
        )
        stateSynchronization = [ordered]@{
            mode = if ($continuousStatePublicationEnabled) {
                'fully-eager-asynchronous-coherence'
            } else {
                'boundary-synchronized'
            }
            continuousPublicationEnabled = $continuousStatePublicationEnabled
            localIarField = 'PPCContext::xeo3GuestIar'
            indirectCallCheckpointEnabledByDefault = $false
            indirectCallCheckpointInterval = 16
            availableEagerState = @(
                'IAR',
                'all GPRs',
                'CR fields',
                'LR',
                'CTR',
                'XER',
                'MSR',
                'FPRs',
                'VMX registers',
                'FPSCR',
                'VSCR'
            )
            fullFlushBoundaries = @(
                'native import',
                'native dispatch fallback',
                'MMIO callback',
                'translated dispatch exit'
            )
            cpuStateCompilerBarrier =
                'std::atomic_signal_fence(std::memory_order_seq_cst)'
            reason =
                'continuous publication follows the actual CMake option; optional indirect checkpoints require BridgeIndirectStateSyncEnabled; mission stability remains unverified'
        }
    }
    output = [ordered]@{
        dll = Get-Artifact $AotDllPath
        pdb = Get-Artifact $AotPdbPath
        deployedDll = Get-Artifact $deployedDllPath
    }
}

$outputDirectory = Split-Path -Parent $OutputPath
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$json = $manifest | ConvertTo-Json -Depth 8
[System.IO.File]::WriteAllText(
    $OutputPath,
    $json + [Environment]::NewLine,
    (New-Object System.Text.UTF8Encoding($false))
)

$manifest

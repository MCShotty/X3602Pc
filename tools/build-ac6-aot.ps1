param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'RelWithDebInfo',
    [ValidateRange(1, 8)]
    [int]$Parallel = 4,
    [string]$XexPath = 'D:\XeO3AC6DVD\default.xex',
    [string]$BuildRoot
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$generatedRoot = Join-Path $repoRoot 'out\ac6\generated'
$importsPath = Join-Path $repoRoot 'out\ac6\imports.tsv'
$stdoutLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stdout.log'
$stderrLog = Join-Path $repoRoot 'out\ac6\xenonrecomp.stderr.log'
$expectedXexHash =
    '6EEFBA42CDFE9121207E534D8D290009C98B1A8C60AE5334A33A4F15167CBBBC'

foreach ($requiredPath in @(
    $XexPath,
    $importsPath,
    $stdoutLog,
    $stderrLog,
    (Join-Path $generatedRoot 'ppc_func_mapping.cpp')
)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required AOT build input is missing: $requiredPath"
    }
}

$actualXexHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $XexPath).Hash
if ($actualXexHash -ne $expectedXexHash) {
    throw "AC6 XEX hash mismatch: expected $expectedXexHash, found $actualXexHash"
}
if ((Get-Item -LiteralPath $stderrLog).Length -ne 0) {
    throw "XenonRecomp stderr is not empty: $stderrLog"
}

$diagnosticPattern =
    'Unrecognized instruction|Unable to decode|RC bit enabled|switch jump table|Unexpected .* instruction|out.of.range'
if (@(Select-String -LiteralPath $stdoutLog -Pattern $diagnosticPattern).Count -ne 0) {
    throw "XenonRecomp diagnostics remain in $stdoutLog"
}
if (@(
    Get-ChildItem $generatedRoot -Filter 'ppc_recomp.*.cpp' |
        Select-String -Pattern '__builtin_debugtrap\(\)'
).Count -ne 0) {
    throw 'Generated AC6 code still contains debug traps.'
}

& (Join-Path $PSScriptRoot 'apply-ac6-generated-guards.ps1') `
    -GeneratedRoot $generatedRoot |
    Out-Host

$memsetSource = Get-ChildItem $generatedRoot -Filter 'ppc_recomp.*.cpp' |
    Select-String -SimpleMatch 'PPC_FUNC_IMPL(__imp__sub_823830F0)' |
    Select-Object -First 1
if (-not $memsetSource) {
    throw 'Generated AC6 code is missing sub_823830F0.'
}
$memsetText = [IO.File]::ReadAllText($memsetSource.Path)
$memsetStart = $memsetText.IndexOf(
    'PPC_FUNC_IMPL(__imp__sub_823830F0)',
    [StringComparison]::Ordinal
)
$memsetEnd = $memsetText.IndexOf(
    '__attribute__((alias(',
    $memsetStart + 1,
    [StringComparison]::Ordinal
)
if ($memsetEnd -lt 0) {
    $memsetEnd = $memsetText.Length
}
$memsetBody = $memsetText.Substring($memsetStart, $memsetEnd - $memsetStart)
foreach ($tailStore in @('// stb r4,1(r6)', '// stb r4,2(r6)')) {
    if ($memsetBody.IndexOf($tailStore, [StringComparison]::Ordinal) -lt 0) {
        throw "Generated AC6 memset lost its pdata-split fallthrough tail: $tailStore"
    }
}

& (Join-Path $PSScriptRoot 'generate-ac6-bridge.ps1') |
    Out-Host

. (Join-Path $PSScriptRoot 'enter-native-build-env.ps1')

if (-not $BuildRoot) {
    $BuildRoot = Join-Path $repoRoot 'build\ac6-aot'
} elseif (-not [IO.Path]::IsPathRooted($BuildRoot)) {
    $BuildRoot = Join-Path $repoRoot $BuildRoot
}
$buildRoot = [IO.Path]::GetFullPath($BuildRoot)
cmake `
    -S $repoRoot `
    -B $buildRoot `
    -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Configuration" `
    "-DCMAKE_CXX_COMPILER:FILEPATH=$env:XEO3_CLANG_CL" `
    "-DCMAKE_ASM_MASM_COMPILER:FILEPATH=$env:XEO3_ML64" `
    "-DCMAKE_LINKER:FILEPATH=$env:XEO3_LLD_LINK" `
    "-DCMAKE_MT:FILEPATH=$env:XEO3_LLVM_MT" `
    "-DXEO3_ENABLE_PROBE_TESTS:BOOL=OFF" `
    "-DAC6_GENERATED_DIR:PATH=$generatedRoot"
if ($LASTEXITCODE -ne 0) {
    throw "AOT CMake configuration failed with exit code $LASTEXITCODE"
}

cmake `
    --build $buildRoot `
    --target `
        debug_break_after `
        xeo3_cpu_state_tests `
        xeo3_fiber_frame_tests `
        xeo3_mmio_endian_tests `
        ac6_queue_wait_tests `
        xeo3_native_dispatch_tests `
        xeo3_kernel_continuation_tests `
        xeo3_state_sync_tests `
        xeo3_vgpu_patch_tests `
        xeo3_host_unmapped_observer_tests `
        ac6_sync_dvd_io_tests `
        ac6_generated_state_check `
    --parallel $Parallel
if ($LASTEXITCODE -ne 0) {
    throw "Bridge unit-test build failed with exit code $LASTEXITCODE"
}
ctest --test-dir $buildRoot --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Bridge tests failed with exit code $LASTEXITCODE"
}

cmake --build $buildRoot --target xeo3_ac6_aot --parallel $Parallel
if ($LASTEXITCODE -ne 0) {
    throw "AC6 AOT DLL build failed with exit code $LASTEXITCODE"
}

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dll = Join-Path $buildRoot "aot\$dllName"
$pdb = [IO.Path]::ChangeExtension($dll, '.pdb')
foreach ($artifact in @($dll, $pdb)) {
    if (-not (Test-Path -LiteralPath $artifact)) {
        throw "Expected AOT artifact is missing: $artifact"
    }
}

cmake --build $buildRoot --target xeo3_aot_contract_tests --parallel $Parallel
if ($LASTEXITCODE -ne 0) {
    throw "AOT contract-test build failed with exit code $LASTEXITCODE"
}
$contractTest = Join-Path $buildRoot 'xeo3_aot_contract_tests.exe'
$contractDll = Join-Path (
    Split-Path -Parent $dll
) (
    [IO.Path]::GetFileNameWithoutExtension($dll) + '_no.dll'
)
Copy-Item -LiteralPath $dll -Destination $contractDll -Force
& $contractTest $contractDll
if ($LASTEXITCODE -ne 0) {
    throw "AOT contract test failed with exit code $LASTEXITCODE"
}

$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'
$exports = (& $readObj --coff-exports $dll) -join "`n"
foreach ($requiredExport in @(
    'CleanupPrecompiledDll',
    'InitPrecompiledDll',
    'PrecompiledImportTable',
    'PrecompiledPointers',
    'BridgeLastOuterHostFence',
    'BridgeLastNestedHostFence',
    'BridgeFatalIndirectCount',
    'BridgeLastFatalIndirectTarget',
    'BridgeLastFatalIndirectCallerIar',
    'BridgeLastFatalIndirectObject',
    'BridgeLastFatalIndirectVtable',
    'BridgeLastFatalIndirectSlot0',
    'BridgeLastFatalIndirectSlot1',
    'BridgeLastFatalIndirectArgument4',
    'BridgeLastFatalIndirectThreadId',
    'BridgeKernelContinuationInstallCount',
    'BridgeKernelContinuationInstallFailureCount',
    'BridgeKernelContinuationRestoreCount',
    'BridgeKernelContinuationRestoreFailureCount',
    'BridgeKernelContinuationHitCount',
    'BridgeKernelContinuationNativeCallCount',
    'BridgeKernelContinuationNativeReturnCount',
    'BridgeKernelContinuationEpilogueCallCount',
    'BridgeKernelContinuationEpilogueReturnCount',
    'BridgeKernelContinuationEnsureCount',
    'BridgeKernelContinuationAttemptCount',
    'BridgeKernelContinuationThrottleCount',
    'BridgeKernelContinuationLastIar',
    'BridgeKernelContinuationLastFailure',
    'BridgeKernelContinuationFingerprintMask',
    'BridgeKernelContinuationHostFingerprintMask',
    'BridgeKernelContinuationNativeDispatchMask',
    'BridgeKernelContinuationNativeDirectMask',
    'BridgeKernelContinuationLastCr6',
    'BridgeKernelContinuationLastPath',
    'BridgeKernelContinuationLastResult',
    'BridgeKernelContinuationLastThreadId',
    'BridgeKernelContinuationLastStage',
    'BridgeKernelContinuationLastEntryR9',
    'BridgeKernelContinuationLastBugCheckCode',
    'BridgeKernelContinuationLastLr',
    'BridgeKernelContinuationLastStackPointer',
    'BridgeKernelContinuationLastDispatchBase',
    'BridgeKernelContinuationLastSlot',
    'BridgeKernelContinuationLastOriginalTarget',
    'BridgeKernelContinuationLastGuestMemory',
    'BridgeKernelContinuationKernelModuleBase',
    'BridgeKernelContinuationNativeBugCheckExHost',
    'BridgeHostUnmappedIarObserverInstalled',
    'BridgeHostUnmappedIarObserverFailure',
    'BridgeHostUnmappedIarCount',
    'BridgeHostUnmappedIarLastIar',
    'BridgeHostUnmappedIarLastThreadId',
    'BridgeHostUnmappedIarLastCpuObject',
    'BridgeHostUnmappedIarLastCpuStateAnchor',
    'BridgeHostUnmappedIarLastGuestMemory',
    'BridgeHostUnmappedIarLastDispatchBase',
    'BridgeHostUnmappedIarLastSlot',
    'BridgeHostUnmappedIarLastSlotTarget',
    'BridgeVgpuPipelineStateCreateFailureCount',
    'BridgeVgpuPipelineStateLastCreateResult',
    'BridgeVgpuPipelineStateLastCreateOutput',
    'BridgeVgpuComputePipelineStateHookInstalled',
    'BridgeVgpuComputePipelineStateHookFailure',
    'BridgeVgpuComputePipelineStateCreateCount',
    'BridgeVgpuComputePipelineStateFingerprintMatchCount',
    'BridgeVgpuComputePipelineStateLastShaderSize',
    'BridgeVgpuComputePipelineStateLastShaderHash0',
    'BridgeVgpuComputePipelineStateLastShaderHash1',
    'BridgeVgpuComputePipelineStateLastShaderHash2',
    'BridgeVgpuComputePipelineStateLastShaderHash3',
    'BridgeVgpuComputePipelineStateLastCreateResult',
    'BridgeVgpuComputePipelineStateLastCreateOutput',
    'BridgeVgpuPipelineStreamHookInstalled',
    'BridgeVgpuPipelineStreamHookFailure',
    'BridgeVgpuPipelineStreamCreateCount',
    'BridgeVgpuPipelineStreamParseCount',
    'BridgeVgpuPipelineStreamParseFailureCount',
    'BridgeVgpuPipelineStreamLastCreateResult',
    'BridgeVgpuPipelineStreamLastCreateOutput',
    'BridgeVgpuNullPipelineStateGuardInstalled',
    'BridgeVgpuNullPipelineStateGuardFailure',
    'BridgeVgpuNullPipelineStateSkipCount',
    'BridgeVgpuNullPipelineStateLastThreadId',
    'BridgeVgpuNullPipelineStateLastRecord',
    'BridgeVgpuNullPipelineStateLastCommandList',
    'BridgeVgpuNullPipelineStateLastCommandContext',
    'BridgeVgpuNullPipelineStateLastCachedPso',
    'BridgeVgpuNullPipelineStateLastRootSignature',
    'BridgeVgpuNullPipelineStateLastRecordKind',
    'BridgeVgpuNullPipelineStateLastVertexCount',
    'BridgeVgpuNullPipelineStateLastStartVertex',
    'BridgeVgpuEdramRestoreDrawCandidateCount',
    'BridgeVgpuEdramRestoreDrawSkipCount',
    'BridgeVgpuEdramRestoreDrawHashMismatchCount',
    'BridgeVgpuEdramRestoreDrawGuardFailure',
    'BridgeVgpuEdramRestoreDrawLastRecord',
    'BridgeVgpuEdramRestoreDrawLastPipelineState',
    'BridgeVgpuEdramRestoreDrawLastCachedBlobSize',
    'BridgeVgpuEdramRestoreDrawLastCachedBlobHash0',
    'BridgeVgpuEdramRestoreDrawLastCachedBlobHash1',
    'BridgeVgpuEdramRestoreDrawLastCachedBlobHash2',
    'BridgeVgpuEdramRestoreDrawLastCachedBlobHash3',
    'BridgeVgpuEdramRestoreExperimentSelector',
    'BridgeVgpuEdramRestoreExperimentCandidateCount',
    'BridgeVgpuEdramRestoreExperimentLastCandidate',
    'BridgeVgpuEdramRestoreExperimentHitCount',
    'BridgeVgpuEdramRestoreExperimentSkipCount',
    'BridgeVgpuEdramRestoreExperimentLastCreateCount',
    'BridgeVgpuEdramScaleFixEnabled',
    'BridgeVgpuEdramScaleCandidateCount',
    'BridgeVgpuEdramScaleFingerprintMatchCount',
    'BridgeVgpuEdramScaleFixPipelineCount',
    'BridgeVgpuEdramScaleFixFailure',
    'BridgeVgpuEdramScaleReplacementShaderSize',
    'BridgeVgpuEdramScaleCandidateVsHash0',
    'BridgeVgpuEdramScaleCandidateVsHash1',
    'BridgeVgpuEdramScaleCandidateVsHash2',
    'BridgeVgpuEdramScaleCandidateVsHash3',
    'BridgeVgpuEdramScaleCandidatePsHash0',
    'BridgeVgpuEdramScaleCandidatePsHash1',
    'BridgeVgpuEdramScaleCandidatePsHash2',
    'BridgeVgpuEdramScaleCandidatePsHash3',
    'BridgeVgpuEdramLoadFixEnabled',
    'BridgeVgpuEdramLoadCandidateCount',
    'BridgeVgpuEdramLoadFingerprintMatchCount',
    'BridgeVgpuEdramLoadFixPipelineCount',
    'BridgeVgpuEdramLoadFixFailure',
    'BridgeVgpuEdramLoadReplacementShaderSize',
    'BridgeVgpuEdramLoadCandidateVsHash0',
    'BridgeVgpuEdramLoadCandidateVsHash1',
    'BridgeVgpuEdramLoadCandidateVsHash2',
    'BridgeVgpuEdramLoadCandidateVsHash3',
    'BridgeVgpuEdramLoadCandidatePsHash0',
    'BridgeVgpuEdramLoadCandidatePsHash1',
    'BridgeVgpuEdramLoadCandidatePsHash2',
    'BridgeVgpuEdramLoadCandidatePsHash3',
    'BridgeVgpuEdramScaleDrawMatchCount',
    'BridgeVgpuEdramScaleDrawSubstitutionCount',
    'BridgeVgpuEdramScaleDrawOriginalPipelineState',
    'BridgeVgpuEdramScaleDrawReplacementPipelineState',
    'BridgeVgpuEdramLoadDrawMatchCount',
    'BridgeVgpuEdramLoadDrawSubstitutionCount',
    'BridgeVgpuEdramLoadDrawOriginalPipelineState',
    'BridgeVgpuEdramLoadDrawReplacementPipelineState',
    'BridgeVgpuEdramDrawRootSignature',
    'BridgeVgpuEdramDrawRootSignatureMismatchCount',
    'BridgeVgpuEdramDrawFingerprintCandidateCount',
    'BridgeVgpuEdramDrawFingerprintQueryCount',
    'BridgeVgpuEdramDrawFingerprintCacheHitCount',
    'BridgeVgpuEdramDrawFingerprintCacheOverflowCount',
    'BridgeVgpuEdramDrawLastFingerprintPipelineState',
    'BridgeVgpuEdramDrawLastFingerprintBlobSize',
    'BridgeVgpuEdramDrawLastFingerprintHash0',
    'BridgeVgpuEdramDrawLastFingerprintHash1',
    'BridgeVgpuEdramDrawLastFingerprintHash2',
    'BridgeVgpuEdramDrawLastFingerprintHash3',
    'BridgeVgpuEdramDrawLastFingerprintResult',
    'BridgeVgpuEdramDrawLastFingerprintClassification',
    'BridgeVgpuEdramDrawFingerprintDumpEnabled',
    'BridgeVgpuEdramDrawFingerprintDumpFailure',
    'BridgeVgpuHostCommandListHookCount',
    'BridgeVgpuHostCommandListHookFailure',
    'BridgeVgpuHostCommandListResetCount',
    'BridgeVgpuHostCommandListResetLastInitialPipelineState',
    'BridgeVgpuHostCommandListResetLastResult',
    'BridgeVgpuHostDrawCallCount',
    'BridgeVgpuHostTransferDrawCount',
    'BridgeVgpuHostTransferExperimentSelector',
    'BridgeVgpuHostTransferLastCandidate',
    'BridgeVgpuHostTransferLastClassification',
    'BridgeVgpuHostTransferLastPipelineState',
    'BridgeVgpuHostTransferLastRootSignature',
    'BridgeVgpuHostEdramRestoreDrawSkipEnabled',
    'BridgeVgpuHostEdramRestoreDrawCandidateCount',
    'BridgeVgpuHostEdramRestoreDrawSkipCount',
    'BridgeVgpuHostEdramRestoreDrawLastDrawCall',
    'BridgeVgpuHostEdramRestoreDrawLastPipelineState',
    'BridgeVgpuHostEdramRestoreDrawLastRootSignature',
    'BridgeVgpuVposSceneHalfWidthUvEnabled',
    'BridgeVgpuTransfer341WidthFixEnabled',
    'BridgeVgpuTransfer341WidthCandidateCount',
    'BridgeVgpuTransfer341WidthSignatureMatchCount',
    'BridgeVgpuTransfer341WidthPatchCount',
    'BridgeVgpuTransfer341WidthFailureCount',
    'BridgeVgpuTransfer341MappedBufferCount',
    'BridgeVgpuTransfer341ArenaCandidateCount',
    'BridgeVgpuTransfer341CommittedArenaCandidateCount',
    'BridgeVgpuTransfer341LastArenaHeapType',
    'BridgeVgpuTransfer341LastArenaMapResult',
    'BridgeVgpuTransfer341UploadContextCount',
    'BridgeVgpuTransfer341UploadContextHitCount',
    'BridgeVgpuTransfer341LastUploadContext',
    'BridgeVgpuTransfer341LastUploadCpuBase',
    'BridgeVgpuTransfer341LastUploadGpuBase',
    'BridgeVgpuTransfer341LastUploadStride',
    'BridgeVgpuTransfer341LastUploadSlotCount',
    'BridgeVgpuTransfer341LastUploadMappedSpan',
    'BridgeVgpuTransfer341LastResolvedCpuAddress',
    'BridgeVgpuTransfer341DescriptorCount',
    'BridgeVgpuTransfer341BindCount',
    'BridgeVgpuTransfer341DescriptorMissCount',
    'BridgeVgpuTransfer341GpuAddressMissCount',
    'BridgeVgpuTransfer341LastGpuAddress',
    'BridgeVgpuTransfer341LastMappedGpuBase',
    'BridgeVgpuTransfer341LastMappedBufferSize',
    'BridgeVgpuTransfer341WidthLastCall',
    'BridgeVgpuTransfer341WidthLastContext',
    'BridgeVgpuTransfer341WidthLastSourceSize',
    'BridgeVgpuTransfer341WidthLastOriginalPackedDimensions',
    'BridgeVgpuTransfer341WidthLastReplacementPackedDimensions',
    'BridgeVgpuTransfer341PipelineState',
    'BridgeVgpuTransfer341PipelineBindCount',
    'BridgeVgpuTransfer341CachedPsoQueryCount',
    'BridgeVgpuTransfer341CachedPsoCacheHitCount',
    'BridgeVgpuTransfer341CachedPsoMatchCount',
    'BridgeVgpuTransfer341LastCachedPso',
    'BridgeVgpuTransfer341LastCachedBlobSize',
    'BridgeVgpuTransfer341LastCachedBlobHash0',
    'BridgeVgpuTransfer341LastCachedBlobHash1',
    'BridgeVgpuTransfer341LastCachedBlobHash2',
    'BridgeVgpuTransfer341LastCachedBlobHash3',
    'BridgeVgpuTransfer341LastCachedBlobResult',
    'BridgeVgpuTransfer341FingerprintDumpFailure',
    'BridgeVgpuTransfer341PipelineReplacementEnabled',
    'BridgeVgpuTransfer341ReplacementPipelineState',
    'BridgeVgpuTransfer341ReplacementCreateCount',
    'BridgeVgpuTransfer341ReplacementSubstitutionCount',
    'BridgeVgpuTransfer341ReplacementFailure',
    'BridgeVgpuTransfer341LastComputeRootSignature',
    'BridgeVgpuTransfer341LastCpuDescriptor',
    'BridgeVgpuTransfer341LastDescriptor0',
    'BridgeVgpuTransfer341LastDescriptor1',
    'BridgeVgpuTransfer341LastDescriptor2',
    'BridgeVgpuTransfer341LastDescriptor3',
    'BridgeVgpuTransfer341LastTaskCpuDescriptor',
    'BridgeVgpuTransfer341LastTaskDescriptor0',
    'BridgeVgpuTransfer341LastTaskDescriptor1',
    'BridgeVgpuTransfer341LastTaskDescriptor2',
    'BridgeVgpuTransfer341LastTaskDescriptor3',
    'BridgeVgpuTransfer341LastTaskGpuAddress',
    'BridgeVgpuFullscreenScissorFixEnabled',
    'BridgeVgpuFullscreenScissorFixCount',
    'BridgeVgpuFullscreenScissorFixFailure',
    'BridgeVgpuDrawRecordCallCount',
    'BridgeVgpuDrawRecordInterestingCount',
    'BridgeVgpuDrawRecordUniqueCount',
    'BridgeVgpuDrawRecordLastRecord',
    'BridgeVgpuDrawRecordLastCommandList',
    'BridgeVgpuDrawRecordLastCommandContext',
    'BridgeVgpuDrawRecordLastRootSignature',
    'BridgeVgpuDrawRecordLastPipelineState',
    'BridgeVgpuDrawRecordLastViewportWidthBits',
    'BridgeVgpuDrawRecordLastViewportHeightBits',
    'BridgeVgpuDrawRecordLastViewportMinDepthBits',
    'BridgeVgpuDrawRecordLastViewportMaxDepthBits',
    'BridgeVgpuDrawRecordLastViewportTopLeftXBits',
    'BridgeVgpuDrawRecordLastViewportTopLeftYBits',
    'BridgeVgpuDrawRecordLastScissorRight',
    'BridgeVgpuDrawRecordLastScissorBottom',
    'BridgeVgpuDrawRecordLastRecordKind',
    'BridgeVgpuDrawRecordLastVertexCount',
    'BridgeVgpuDrawRecordLastStartVertex'
  )) {
    if ($exports -notmatch [regex]::Escape("Name: $requiredExport")) {
        throw "AOT DLL is missing export $requiredExport"
    }
}

$mappingCount = (
    Select-String `
        -LiteralPath (Join-Path $generatedRoot 'ppc_func_mapping.cpp') `
        -Pattern '^\s*\{\s*0x'
).Count

[pscustomobject]@{
    Dll = $dll
    DllBytes = (Get-Item -LiteralPath $dll).Length
    DllSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash
    Pdb = $pdb
    PdbBytes = (Get-Item -LiteralPath $pdb).Length
    MappingCount = $mappingCount
    ImportCount = 229
    XexSha256 = $actualXexHash
} | Format-List

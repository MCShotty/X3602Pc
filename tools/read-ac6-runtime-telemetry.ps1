param(
    [int]$TargetProcessId = 0,
    [ValidateRange(1, 2400)]
    [int]$Samples = 1,
    [ValidateRange(0, 60000)]
    [int]$IntervalMilliseconds = 1000,
    [string]$LabRoot = 'D:\Games\AC6 shit\XeO3-AC6-lab',
    [switch]$AllowMissingExports
)

$ErrorActionPreference = 'Stop'

$dllName = 'xeo3_58f9e24a_5e717488_0670684a_5afca9cb_3f697be6.dll'
$dllPath = Join-Path $LabRoot $dllName
$readObj = Join-Path $env:ProgramFiles 'LLVM\bin\llvm-readobj.exe'

foreach ($requiredPath in @($dllPath, $readObj)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required telemetry input is missing: $requiredPath"
    }
}

if ($TargetProcessId -eq 0) {
    $targetProcess = Get-Process -Name Emu -ErrorAction Stop |
        Where-Object MainWindowHandle -ne 0 |
        Select-Object -First 1
} else {
    $targetProcess = Get-Process -Id $TargetProcessId -ErrorAction Stop
}
if (-not $targetProcess) {
    throw 'No running Emu process was found.'
}

$module = $targetProcess.Modules |
    Where-Object ModuleName -eq $dllName |
    Select-Object -First 1
if (-not $module) {
    throw "The AC6 AOT DLL is not loaded in process $($targetProcess.Id)."
}

$exportText = (& $readObj --coff-exports $dllPath) -join "`n"
if ($LASTEXITCODE -ne 0) {
    throw "llvm-readobj failed with exit code $LASTEXITCODE."
}

$exportRvas = @{}
$exportPattern =
    'Export\s*\{\s*Ordinal:\s*\d+\s*Name:\s*(?<name>\S+)\s*RVA:\s*0x(?<rva>[0-9A-Fa-f]+)'
foreach ($match in [regex]::Matches($exportText, $exportPattern)) {
    $exportRvas[$match.Groups['name'].Value] =
        [Convert]::ToUInt64($match.Groups['rva'].Value, 16)
}

$fields = [ordered]@{
    BridgeDispatchCount = 8
    BridgeDispatchExitCount = 8
    BridgeImportCount = 8
    BridgeImportCountersEnabled = 4
    BridgeImportCounterCount = 4
    BridgeFunctionLookupEntryCount = 4
    BridgeIntegerImportSyncEnabled = 4
    BridgeIntegerImportSyncAttemptCount = 8
    BridgeIntegerImportSyncHitCount = 8
    BridgeIntegerImportSyncFallbackCount = 8
    BridgeIntegerImportSyncLastThunk = 4
    BridgeEventTraceEnabled = 4
    BridgeThreadImportTraceEnabled = 4
    BridgeFastSynchronizationTelemetryEnabled = 4
    BridgeFastCriticalSectionsEnabled = 4
    BridgeFastCriticalSectionAttemptCount = 8
    BridgeFastCriticalSectionHitCount = 8
    BridgeFastCriticalSectionFallbackCount = 8
    BridgeFastCriticalSectionSignalCount = 8
    BridgeFastCriticalSectionLastThunk = 4
    BridgeFastCriticalSectionLastAddress = 4
    BridgeFastCriticalSectionLastThread = 4
    BridgeFastCriticalSectionLastDisposition = 4
    BridgeFastSpinLocksEnabled = 4
    BridgeFastSpinLockAttemptCount = 8
    BridgeFastSpinLockHitCount = 8
    BridgeFastSpinLockFallbackCount = 8
    BridgeFastSpinLockContentionCount = 8
    BridgeFastSpinLockLastThunk = 4
    BridgeFastSpinLockLastAddress = 4
    BridgeFastIrqlEnabled = 4
    BridgeFastIrqlTelemetryEnabled = 4
    BridgeFastIrqlAttemptCount = 8
    BridgeFastIrqlHitCount = 8
    BridgeFastIrqlNativeFallbackCount = 8
    BridgeFastIrqlInvalidFallbackCount = 8
    BridgeFastIrqlLastThunk = 4
    BridgeFastIrqlLastOldIrql = 4
    BridgeFastIrqlLastNewIrql = 4
    BridgeFastIrqlLastPendingIrql = 4
    BridgeRaiseIrqlOldPassiveCount = 8
    BridgeRaiseIrqlOldApcCount = 8
    BridgeRaiseIrqlOldDispatchCount = 8
    BridgeRaiseIrqlUnexpectedCount = 8
    BridgeIndirectTelemetryEnabled = 4
    BridgeIndirectStateSyncEnabled = 4
    BridgeIndirectCallCount = 8
    BridgeLastIndirectTarget = 4
    BridgeLastIndirectCallerIar = 4
    BridgeFatalIndirectCount = 8
    BridgeLastFatalIndirectTarget = 4
    BridgeLastFatalIndirectCallerIar = 4
    BridgeLastFatalIndirectObject = 4
    BridgeLastFatalIndirectVtable = 4
    BridgeLastFatalIndirectSlot0 = 4
    BridgeLastFatalIndirectSlot1 = 4
    BridgeLastFatalIndirectArgument4 = 8
    BridgeLastFatalIndirectThreadId = 4
    BridgeWorkerEntryCount = 8
    BridgeWorkerLoopCount = 8
    BridgeWorkerCallbackDispatchCount = 8
    BridgeWorkerCallbackReturnCount = 8
    BridgeWorkerActiveDecrementCount = 8
    BridgeQueueWaitLoopCount = 8
    BridgeLastQueueWaitObject = 4
    BridgeLastQueueWaitCount = 4
    BridgeQueueWaitPositiveCount = 8
    BridgeQueueWaitPauseCount = 8
    BridgeQueueWaitSwitchCount = 8
    BridgeQueueWaitSleepCount = 8
    BridgeWorkerCallbackTargetCount = 8
    BridgeLastWorkerCallbackTarget = 4
    BridgeLastWorkerCallbackCallerIar = 4
    BridgeLastWorkerPrimaryTarget = 4
    BridgeLastWorkerPrimaryObject = 4
    BridgeLastWorkerPrimaryPayload = 8
    BridgeLastWorkerPrimaryThreadId = 4
    BridgeActiveWorkerPrimaryTarget = 4
    BridgeActiveWorkerPrimaryObject = 4
    BridgeActiveWorkerPrimaryPayload = 8
    BridgeActiveWorkerPrimaryThreadId = 4
    BridgeActiveWorkerPrimaryClaimCount = 8
    BridgeLastWorkerDestroyTarget = 4
    BridgeLastWorkerDestroyObject = 4
    BridgeAc6AudioPollClockFallbackEnabled = 4
    BridgeAc6AudioPollTraceEnabled = 4
    BridgeVdSwapTraceEnabled = 4
    BridgeVdSwapTraceCount = 4
    BridgeVdSwapTraceFailure = 4
    BridgeGuestIarTelemetryEnabled = 4
    BridgeContinuousStatePublicationMode = 4
    BridgeAc6AudioPollCallCount = 8
    BridgeAc6AudioPollReturnCount = 8
    BridgeAc6AudioPollSampleCount = 8
    BridgeAc6AudioPollContinueCount = 8
    BridgeAc6AudioPollTimeoutCount = 8
    BridgeAc6AudioPollExitCount = 8
    BridgeAc6AudioPollTimeBaseSampleCount = 8
    BridgeAc6AudioPollClockFallbackCount = 8
    BridgeAc6AudioPollDelayCount = 8
    BridgeAc6AudioPollLastGuestIar = 4
    BridgeAc6AudioPollLastThreadId = 4
    BridgeAc6AudioPollLastObject = 4
    BridgeAc6AudioPollLastFrame = 4
    BridgeAc6AudioPollLastGuestMemory = 8
    BridgeAc6AudioPollLastTimeBase = 8
    BridgeAc6AudioPollLastGlobalTick = 4
    BridgeAc6AudioPollLastGlobalClock = 4
    BridgeAc6AudioPollLastObjectClock = 4
    BridgeAc6AudioPollLastBaselineTick = 4
    BridgeAc6AudioPollLastDelta = 4
    BridgeAc6AudioPollLastConsumerPointer = 4
    BridgeAc6AudioPollLastConsumerValue = 4
    BridgeAc6AudioPollLastObservedConsumer = 4
    BridgeAc6AudioPollLastRequiredDistance = 4
    BridgeAc6AudioPollLastAvailableDistance = 4
    BridgeAc6AudioPollLastReturn = 4
    BridgeAc6AudioPollLastResolvedTick = 4
    BridgeSynchronousQueueEnabled = 4
    BridgeSynchronousQueueBypassCount = 8
    BridgeSynchronousQueueSubmitCount = 8
    BridgeSynchronousQueueTaskCount = 8
    BridgeSynchronousQueueFailureCount = 8
    BridgeSynchronousQueueFallbackCount = 8
    BridgeSynchronousQueueLastObject = 4
    BridgeSynchronousQueueLastTask = 4
    BridgeSynchronousQueueLastTarget = 4
    BridgeSynchronousQueueLastPhase = 4
    BridgeWorkerPrimarySlot0Target = 4
    BridgeWorkerPrimarySlot0Object = 4
    BridgeWorkerPrimarySlot0Payload = 8
    BridgeWorkerPrimarySlot0ThreadId = 4
    BridgeWorkerPrimarySlot1Target = 4
    BridgeWorkerPrimarySlot1Object = 4
    BridgeWorkerPrimarySlot1Payload = 8
    BridgeWorkerPrimarySlot1ThreadId = 4
    BridgeWorkerPrimarySlot2Target = 4
    BridgeWorkerPrimarySlot2Object = 4
    BridgeWorkerPrimarySlot2Payload = 8
    BridgeWorkerPrimarySlot2ThreadId = 4
    BridgeWorkerPrimarySlot3Target = 4
    BridgeWorkerPrimarySlot3Object = 4
    BridgeWorkerPrimarySlot3Payload = 8
    BridgeWorkerPrimarySlot3ThreadId = 4
    BridgeLastGuestIar = 4
    BridgeLastGuestMemory = 8
    BridgeLastOuterHostFence = 8
    BridgeLastNestedHostFence = 8
    BridgeLastImportThunk = 4
    BridgeLastImportTarget = 4
    BridgeVirtualPadPollCount = 8
    BridgeVirtualPadLastButtons = 4
    BridgeCriticalSectionEventCount = 8
    BridgeRtlTryLastFailureStreak = 8
    BridgeSynchronousArchiveReadCount = 8
    BridgeSynchronousArchiveReadAttemptCount = 8
    BridgeSynchronousArchiveReadFailureCount = 8
    BridgeSynchronousArchiveReadLastPhase = 4
    BridgeSynchronousArchiveReadLastCallerIar = 4
    BridgeSynchronousArchiveReadLastSignalThreadId = 4
    BridgeSynchronousArchiveReadLastOffset = 8
    BridgeSynchronousArchiveReadLastIndex = 4
    BridgeSynchronousArchiveReadLastHandle = 4
    BridgeSynchronousArchiveReadLastLength = 4
    BridgeSynchronousArchiveReadLastCompleted = 4
    BridgeSynchronousArchiveReadLastError = 4
    BridgeSynchronousArchiveSignalCount = 8
    BridgeSynchronousArchiveSignalFailureCount = 8
    BridgeSynchronousArchiveReadLastEvent = 4
    BridgeSynchronousArchiveReadLastApcRoutine = 4
    BridgeSynchronousArchiveReadLastApcContext = 4
    BridgeSynchronousArchiveReadLastIoStatusBlock = 4
    BridgeSynchronousArchiveReadLastBuffer = 4
    BridgeSynchronousArchiveReadLastOffsetPointer = 4
    BridgeSynchronousArchiveReadLastThreadId = 4
    BridgeSynchronousArchiveReadLastIoStatus = 4
    BridgeSynchronousArchiveReadLastIoInformation = 4
    BridgeSynchronousArchiveSignalLastStatus = 4
    BridgeSynchronousArchiveReadLastGuestMemory = 8
    BridgeSynchronousArchiveReadLastHostBuffer = 8
    BridgeSynchronousArchiveReadLastFailureGuestMemory = 8
    BridgeSynchronousArchiveReadLastFailureHostBuffer = 8
    BridgeSynchronousArchiveReadLastFailureLength = 4
    BridgeNtWaitCount = 8
    BridgeNtWaitLastHandle = 4
    BridgeNtWaitLastWaitMode = 4
    BridgeNtWaitLastTimeout = 4
    BridgeNtWaitLastStatus = 4
    BridgeNtWaitLastCallerLr = 4
    BridgeNtReleaseSemaphoreCount = 8
    BridgeNtReleaseSemaphoreLastHandle = 4
    BridgeNtReleaseSemaphoreLastReleaseCount = 4
    BridgeNtReleaseSemaphoreLastPreviousCount = 4
    BridgeNtReleaseSemaphoreLastStatus = 4
    BridgeNtReleaseSemaphoreLastCallerLr = 4
    BridgePostIntroCallbackCount = 8
    BridgePostIntroCallbackReturnCount = 8
    BridgePostIntroCallbackZeroCount = 8
    BridgePostIntroCallbackOneCount = 8
    BridgePostIntroCallbackOtherCount = 8
    BridgePostIntroCallbackLastKind = 4
    BridgePostIntroCallbackLastObject = 4
    BridgePostIntroCallbackLastState = 4
    BridgePostIntroCallbackLastFlags = 4
    BridgePostIntroCallbackLastTarget = 4
    BridgePostIntroCallbackLastTargetContext = 4
    BridgePostIntroCallbackLastR13 = 4
    BridgePostIntroCallbackLastEntryLr = 4
    BridgePostIntroCallbackLastExitLr = 4
    BridgePostIntroCallbackLastExitR3 = 4
    BridgePostIntroCallbackLastThreadId = 4
    BridgeKernelContinuationInstallCount = 8
    BridgeKernelContinuationInstallFailureCount = 8
    BridgeKernelContinuationRestoreCount = 8
    BridgeKernelContinuationRestoreFailureCount = 8
    BridgeKernelContinuationHitCount = 8
    BridgeKernelContinuationNativeCallCount = 8
    BridgeKernelContinuationNativeReturnCount = 8
    BridgeKernelContinuationEpilogueCallCount = 8
    BridgeKernelContinuationEpilogueReturnCount = 8
    BridgeKernelContinuationEnsureCount = 8
    BridgeKernelContinuationAttemptCount = 8
    BridgeKernelContinuationThrottleCount = 8
    BridgeKernelContinuationLastIar = 4
    BridgeKernelContinuationLastFailure = 4
    BridgeKernelContinuationFingerprintMask = 4
    BridgeKernelContinuationHostFingerprintMask = 4
    BridgeKernelContinuationNativeDispatchMask = 4
    BridgeKernelContinuationNativeDirectMask = 4
    BridgeKernelContinuationLastCr6 = 4
    BridgeKernelContinuationLastPath = 4
    BridgeKernelContinuationLastResult = 4
    BridgeKernelContinuationLastThreadId = 4
    BridgeKernelContinuationLastStage = 4
    BridgeKernelContinuationLastEntryR9 = 8
    BridgeKernelContinuationLastBugCheckCode = 8
    BridgeKernelContinuationLastLr = 8
    BridgeKernelContinuationLastStackPointer = 8
    BridgeKernelContinuationLastDispatchBase = 8
    BridgeKernelContinuationLastSlot = 8
    BridgeKernelContinuationLastOriginalTarget = 8
    BridgeKernelContinuationLastGuestMemory = 8
    BridgeKernelContinuationKernelModuleBase = 8
    BridgeKernelContinuationNativeBugCheckExHost = 8
    BridgeHostUnmappedIarObserverInstalled = 4
    BridgeHostUnmappedIarObserverFailure = 4
    BridgeHostUnmappedIarCount = 8
    BridgeHostUnmappedIarLastIar = 4
    BridgeHostUnmappedIarLastThreadId = 4
    BridgeHostUnmappedIarLastCpuObject = 8
    BridgeHostUnmappedIarLastCpuStateAnchor = 8
    BridgeHostUnmappedIarLastGuestMemory = 8
    BridgeHostUnmappedIarLastDispatchBase = 8
    BridgeHostUnmappedIarLastSlot = 8
    BridgeHostUnmappedIarLastSlotTarget = 8
    BridgeVgpuPatchStatus = 4
    BridgeVgpuExtendedFetchCount = 8
    BridgeVgpuLastFetchCount = 4
    BridgeVgpuZeroedHeapCount = 8
    BridgeVgpuDiscardedResourceCount = 8
    BridgeVgpuDiscardFailure = 4
    BridgeVgpuPlacedResourceCallCount = 8
    BridgeVgpuLastPlacedResourceSite = 4
    BridgeVgpuLastHeapFlags = 4
    BridgeVgpuLastResourceFlags = 4
    BridgeVgpuLastCommittedHeapFlags = 4
    BridgeVgpuLastHeapType = 4
    BridgeVgpuLastHeapCpuPageProperty = 4
    BridgeVgpuLastHeapMemoryPoolPreference = 4
    BridgeVgpuLegacyFallbackCount = 8
    BridgeVgpuLegacyFallbackFailure = 4
    BridgeVgpuCommittedOverflowFallbackCount = 8
    BridgeVgpuCommittedOverflowFallbackFailure = 4
    BridgeVgpuLastHeapOffset = 8
    BridgeVgpuLastHeapSize = 8
    BridgeVgpuLastAllocationSize = 8
    BridgeVgpuLastPlacedResourceFailure = 4
    BridgeVgpuDeviceRemovedReason = 4
    BridgeVgpuShaderCompileCount = 8
    BridgeVgpuLastCompiledShaderSize = 4
    BridgeVgpuShaderCaptureFailure = 4
    BridgeVgpuXenosTranslateHookInstalled = 4
    BridgeVgpuXenosTranslateHookFailure = 4
    BridgeVgpuXenosTranslateCount = 8
    BridgeVgpuXenosUcodeCaptureCount = 8
    BridgeVgpuXenosUcodeCaptureFailure = 4
    BridgeVgpuXenosShaderMapFailure = 4
    BridgeVgpuXenosLastStage = 4
    BridgeVgpuXenosLastUcodeSize = 4
    BridgeVgpuXenosLastUcodeHash0 = 8
    BridgeVgpuXenosLastUcodeHash1 = 8
    BridgeVgpuXenosLastUcodeHash2 = 8
    BridgeVgpuXenosLastUcodeHash3 = 8
    BridgeVgpuVertexShaderCompileCount = 8
    BridgeVgpuLastVertexShaderSize = 4
    BridgeVgpuVertexShaderCaptureFailure = 4
    BridgeVgpuGroundFixShaderCount = 8
    BridgeVgpuGroundFixFetchCount = 8
    BridgeVgpuGroundFixFailure = 4
    BridgeVgpuGroundFixEnabled = 4
    BridgeVgpuIndexFixShaderCount = 8
    BridgeVgpuIndexFixSiteCount = 8
    BridgeVgpuIndexFixFailure = 4
    BridgeVgpuAircraftRestartShaderCount = 8
    BridgeVgpuShadowRestartShaderCount = 8
    BridgeVgpuReciprocalFixShaderCount = 8
    BridgeVgpuReciprocalFixInstructionCount = 8
    BridgeVgpuReciprocalFixFailure = 4
    BridgeVgpuReciprocalFixEnabled = 4
    BridgeVgpuWaveBallotFingerprintMatchCount = 8
    BridgeVgpuWaveBallotFixShaderCount = 8
    BridgeVgpuWaveBallotFixSiteCount = 8
    BridgeVgpuWaveBallotFixFailure = 4
    BridgeVgpuWaveBallotFixEnabled = 4
    BridgeVgpuVposFixShaderCount = 8
    BridgeVgpuVposFixSiteCount = 8
    BridgeVgpuVposFixFailure = 4
    BridgeVgpuVposScaleFixEnabled = 4
    BridgeVgpuVposSceneHalfWidthUvEnabled = 4
    BridgeVgpuToneMapFixEnabled = 4
    BridgeVgpuExposureFixEnabled = 4
    BridgeVgpuExposureFingerprintMatchCount = 8
    BridgeVgpuExposureFixShaderCount = 8
    BridgeVgpuExposureFixSiteCount = 8
    BridgeVgpuExposureFixFailure = 4
    BridgeVgpuTextureEndianFixEnabled = 4
    BridgeVgpuTextureEndianFixBudget = 4
    BridgeVgpuTextureEndianReplacement = 4
    BridgeVgpuTextureEndianFixCount = 8
    BridgeVgpuTexturePreviewEndianFixCount = 8
    BridgeVgpuTextureEndianSignatureMatchCount = 8
    BridgeVgpuTextureEndianLastMatchCall = 8
    BridgeVgpuTextureEndianLastPatchedCall = 8
    BridgeVgpuTextureTransferCallCount = 8
    BridgeVgpuConstantUploadCallCount = 8
    BridgeVgpuConstantUpload128Count = 8
    BridgeVgpuConstantUploadContextCount = 8
    BridgeVgpuConstantUploadContextFailureCount = 8
    BridgeVgpuConstantUploadLastContext = 8
    BridgeVgpuConstantUploadLastCpuBase = 8
    BridgeVgpuConstantUploadLastGpuBase = 8
    BridgeVgpuConstantUploadLastStride = 4
    BridgeVgpuConstantUploadLastSlotCount = 4
    BridgeVgpuConstantUploadLastMappedSpan = 8
    BridgeVgpuTransfer341WidthFixEnabled = 4
    BridgeVgpuTransfer341WidthCandidateCount = 8
    BridgeVgpuTransfer341WidthSignatureMatchCount = 8
    BridgeVgpuTransfer341WidthPatchCount = 8
    BridgeVgpuTransfer341WidthFailureCount = 8
    BridgeVgpuTransfer341MappedBufferCount = 8
    BridgeVgpuTransfer341ArenaCandidateCount = 8
    BridgeVgpuTransfer341CommittedArenaCandidateCount = 8
    BridgeVgpuTransfer341LastArenaHeapType = 4
    BridgeVgpuTransfer341LastArenaMapResult = 4
    BridgeVgpuTransfer341UploadContextCount = 8
    BridgeVgpuTransfer341UploadContextHitCount = 8
    BridgeVgpuTransfer341LastUploadContext = 8
    BridgeVgpuTransfer341LastUploadCpuBase = 8
    BridgeVgpuTransfer341LastUploadGpuBase = 8
    BridgeVgpuTransfer341LastUploadStride = 4
    BridgeVgpuTransfer341LastUploadSlotCount = 4
    BridgeVgpuTransfer341LastUploadMappedSpan = 8
    BridgeVgpuTransfer341LastResolvedCpuAddress = 8
    BridgeVgpuTransfer341DescriptorCount = 8
    BridgeVgpuTransfer341BindCount = 8
    BridgeVgpuTransfer341DescriptorMissCount = 8
    BridgeVgpuTransfer341GpuAddressMissCount = 8
    BridgeVgpuTransfer341LastGpuAddress = 8
    BridgeVgpuTransfer341LastMappedGpuBase = 8
    BridgeVgpuTransfer341LastMappedBufferSize = 8
    BridgeVgpuTransfer341WidthLastCall = 8
    BridgeVgpuTransfer341WidthLastContext = 8
    BridgeVgpuTransfer341WidthLastSourceSize = 8
    BridgeVgpuTransfer341WidthLastOriginalPackedDimensions = 4
    BridgeVgpuTransfer341WidthLastReplacementPackedDimensions = 4
    BridgeVgpuTransfer341PipelineState = 8
    BridgeVgpuTransfer341PipelineBindCount = 8
    BridgeVgpuTransfer341CachedPsoQueryCount = 8
    BridgeVgpuTransfer341CachedPsoCacheHitCount = 8
    BridgeVgpuTransfer341CachedPsoMatchCount = 8
    BridgeVgpuTransfer341LastCachedPso = 8
    BridgeVgpuTransfer341LastCachedBlobSize = 8
    BridgeVgpuTransfer341LastCachedBlobHash0 = 8
    BridgeVgpuTransfer341LastCachedBlobHash1 = 8
    BridgeVgpuTransfer341LastCachedBlobHash2 = 8
    BridgeVgpuTransfer341LastCachedBlobHash3 = 8
    BridgeVgpuTransfer341LastCachedBlobResult = 4
    BridgeVgpuTransfer341FingerprintDumpFailure = 4
    BridgeVgpuTransfer341PipelineReplacementEnabled = 4
    BridgeVgpuTransfer341ReplacementPipelineState = 8
    BridgeVgpuTransfer341ReplacementCreateCount = 8
    BridgeVgpuTransfer341ReplacementSubstitutionCount = 8
    BridgeVgpuTransfer341ReplacementFailure = 4
    BridgeVgpuTransfer341LastComputeRootSignature = 8
    BridgeVgpuTransfer341LastCpuDescriptor = 8
    BridgeVgpuTransfer341LastDescriptor0 = 8
    BridgeVgpuTransfer341LastDescriptor1 = 8
    BridgeVgpuTransfer341LastDescriptor2 = 8
    BridgeVgpuTransfer341LastDescriptor3 = 8
    BridgeVgpuTransfer341LastTaskCpuDescriptor = 8
    BridgeVgpuTransfer341LastTaskDescriptor0 = 8
    BridgeVgpuTransfer341LastTaskDescriptor1 = 8
    BridgeVgpuTransfer341LastTaskDescriptor2 = 8
    BridgeVgpuTransfer341LastTaskDescriptor3 = 8
    BridgeVgpuTransfer341LastTaskGpuAddress = 8
    BridgeVgpuEdramConstantCandidateCount = 8
    BridgeVgpuEdramLoadConstantCount = 8
    BridgeVgpuEdramScaleConstantCount = 8
    BridgeVgpuEdramConstantSnapshotSequence = 8
    BridgeVgpuEdramConstantLastCall = 8
    BridgeVgpuEdramConstantLastContext = 8
    BridgeVgpuEdramConstantLastSize = 8
    BridgeVgpuEdramConstantLastKind = 4
    BridgeVgpuEdramConstantData0 = 8
    BridgeVgpuEdramConstantData1 = 8
    BridgeVgpuEdramConstantData2 = 8
    BridgeVgpuEdramConstantData3 = 8
    BridgeVgpuEdramConstantData4 = 8
    BridgeVgpuEdramConstantData5 = 8
    BridgeVgpuEdramConstantData6 = 8
    BridgeVgpuEdramConstantData7 = 8
    BridgeVgpuTextureEndian2CallCount = 8
    BridgeVgpuLastTextureEndianOriginal = 4
    BridgeVgpuLastTextureEndianReplacement = 4
    BridgeVgpuLastTextureEndian2Parameter9 = 4
    BridgeVgpuLastTextureEndian2Parameter13 = 4
    BridgeVgpuLastTextureEndian2Parameter14 = 4
    BridgeVgpuLastTextureEndian2Parameter15 = 4
    BridgeVgpuLastTextureEndian2Parameter16 = 4
    BridgeVgpuPipelineStateHookInstalled = 4
    BridgeVgpuPipelineStateHookFailure = 4
    BridgeVgpuPipelineStateCreateCount = 8
    BridgeVgpuComputePipelineStateHookInstalled = 4
    BridgeVgpuComputePipelineStateHookFailure = 4
    BridgeVgpuComputePipelineStateCreateCount = 8
    BridgeVgpuComputePipelineStateFingerprintMatchCount = 8
    BridgeVgpuComputePipelineStateLastShaderSize = 8
    BridgeVgpuComputePipelineStateLastShaderHash0 = 8
    BridgeVgpuComputePipelineStateLastShaderHash1 = 8
    BridgeVgpuComputePipelineStateLastShaderHash2 = 8
    BridgeVgpuComputePipelineStateLastShaderHash3 = 8
    BridgeVgpuComputePipelineStateLastCreateResult = 4
    BridgeVgpuComputePipelineStateLastCreateOutput = 8
    BridgeVgpuPipelineStreamHookInstalled = 4
    BridgeVgpuPipelineStreamHookFailure = 4
    BridgeVgpuPipelineStreamCreateCount = 8
    BridgeVgpuPipelineStreamParseCount = 8
    BridgeVgpuPipelineStreamParseFailureCount = 8
    BridgeVgpuPipelineStreamLastCreateResult = 4
    BridgeVgpuPipelineStreamLastCreateOutput = 8
    BridgeVgpuSuppressedEdramRestorePsoCount = 8
    BridgeVgpuEdramRestoreFingerprintCount = 8
    BridgeVgpuPso535CullFixEnabled = 4
    BridgeVgpuPso535CullCandidateCount = 8
    BridgeVgpuPso535CullFingerprintMatchCount = 8
    BridgeVgpuPso535CullFixPipelineCount = 8
    BridgeVgpuPso535CullFixFailure = 4
    BridgeVgpuPso535CullLastOriginalMode = 4
    BridgeVgpuPso535CullLastReplacementMode = 4
    BridgeVgpuEdramScaleFixEnabled = 4
    BridgeVgpuEdramScaleCandidateCount = 8
    BridgeVgpuEdramScaleFingerprintMatchCount = 8
    BridgeVgpuEdramScaleFixPipelineCount = 8
    BridgeVgpuEdramScaleFixFailure = 4
    BridgeVgpuEdramScaleReplacementShaderSize = 4
    BridgeVgpuEdramScaleCandidateVsHash0 = 8
    BridgeVgpuEdramScaleCandidateVsHash1 = 8
    BridgeVgpuEdramScaleCandidateVsHash2 = 8
    BridgeVgpuEdramScaleCandidateVsHash3 = 8
    BridgeVgpuEdramScaleCandidatePsHash0 = 8
    BridgeVgpuEdramScaleCandidatePsHash1 = 8
    BridgeVgpuEdramScaleCandidatePsHash2 = 8
    BridgeVgpuEdramScaleCandidatePsHash3 = 8
    BridgeVgpuEdramLoadFixEnabled = 4
    BridgeVgpuEdramLoadCandidateCount = 8
    BridgeVgpuEdramLoadFingerprintMatchCount = 8
    BridgeVgpuEdramLoadFixPipelineCount = 8
    BridgeVgpuEdramLoadFixFailure = 4
    BridgeVgpuEdramLoadReplacementShaderSize = 4
    BridgeVgpuEdramLoadCandidateVsHash0 = 8
    BridgeVgpuEdramLoadCandidateVsHash1 = 8
    BridgeVgpuEdramLoadCandidateVsHash2 = 8
    BridgeVgpuEdramLoadCandidateVsHash3 = 8
    BridgeVgpuEdramLoadCandidatePsHash0 = 8
    BridgeVgpuEdramLoadCandidatePsHash1 = 8
    BridgeVgpuEdramLoadCandidatePsHash2 = 8
    BridgeVgpuEdramLoadCandidatePsHash3 = 8
    BridgeVgpuEdramScaleDrawMatchCount = 8
    BridgeVgpuEdramScaleDrawSubstitutionCount = 8
    BridgeVgpuEdramScaleDrawOriginalPipelineState = 8
    BridgeVgpuEdramScaleDrawReplacementPipelineState = 8
    BridgeVgpuEdramLoadDrawMatchCount = 8
    BridgeVgpuEdramLoadDrawSubstitutionCount = 8
    BridgeVgpuEdramLoadScissorOverrideCount = 8
    BridgeVgpuEdramLoadDrawOriginalPipelineState = 8
    BridgeVgpuEdramLoadDrawReplacementPipelineState = 8
    BridgeVgpuEdramDrawRootSignature = 8
    BridgeVgpuPixEdramBoundMatchCount = 8
    BridgeVgpuPixEdramBoundRejectCount = 8
    BridgeVgpuPixEdramResolveFailure = 4
    BridgeVgpuRtvHookInstalled = 4
    BridgeVgpuRtvHookFailure = 4
    BridgeVgpuPixDescriptorCopyCount = 8
    BridgeVgpuPixConstantCopyCount = 8
    BridgeVgpuPixDescriptorHookFailure = 4
    BridgeVgpuG2HTraceEnabled = 4
    BridgeVgpuG2HTraceCount = 4
    BridgeVgpuG2HTraceFailure = 4
    BridgeVgpuEdramDrawRootSignatureMismatchCount = 8
    BridgeVgpuEdramDrawFingerprintCandidateCount = 8
    BridgeVgpuEdramDrawFingerprintQueryCount = 8
    BridgeVgpuEdramDrawFingerprintCacheHitCount = 8
    BridgeVgpuEdramDrawFingerprintCacheOverflowCount = 8
    BridgeVgpuEdramDrawLastFingerprintPipelineState = 8
    BridgeVgpuEdramDrawLastFingerprintBlobSize = 8
    BridgeVgpuEdramDrawLastFingerprintHash0 = 8
    BridgeVgpuEdramDrawLastFingerprintHash1 = 8
    BridgeVgpuEdramDrawLastFingerprintHash2 = 8
    BridgeVgpuEdramDrawLastFingerprintHash3 = 8
    BridgeVgpuEdramDrawLastFingerprintResult = 4
    BridgeVgpuEdramDrawLastFingerprintClassification = 4
    BridgeVgpuEdramDrawFingerprintDumpEnabled = 4
    BridgeVgpuEdramDrawFingerprintDumpFailure = 4
    BridgeVgpuHostCommandListHookCount = 4
    BridgeVgpuHostCommandListHookFailure = 4
    BridgeVgpuHostCommandListResetCount = 8
    BridgeVgpuHostCommandListResetLastInitialPipelineState = 8
    BridgeVgpuHostCommandListResetLastResult = 4
    BridgeVgpuHostDrawCallCount = 8
    BridgeVgpuRestartTerrainPipelineCount = 8
    BridgeVgpuRestartSkyPipelineCount = 8
    BridgeVgpuRestartPipelineOverflowCount = 8
    BridgeVgpuRestartTerrainDrawCount = 8
    BridgeVgpuRestartSkyDrawCount = 8
    BridgeVgpuRestartTerrainStartVertexZeroCount = 8
    BridgeVgpuRestartTerrainStartVertexNonZeroCount = 8
    BridgeVgpuRestartSkyStartVertexZeroCount = 8
    BridgeVgpuRestartSkyStartVertexNonZeroCount = 8
    BridgeVgpuRestartTerrainLastVertexCount = 4
    BridgeVgpuRestartTerrainLastStartVertex = 4
    BridgeVgpuRestartTerrainMaxStartVertex = 4
    BridgeVgpuRestartSkyLastVertexCount = 4
    BridgeVgpuRestartSkyLastStartVertex = 4
    BridgeVgpuRestartSkyMaxStartVertex = 4
    BridgeVgpuRestartLastClassification = 4
    BridgeVgpuRestartLastPipelineState = 8
    BridgeVgpuRestartLastInstanceCount = 4
    BridgeVgpuRestartLastStartInstance = 4
    BridgeVgpuRestartLastThreadId = 4
    BridgeVgpuRestartLastDrawCall = 8
    BridgeVgpuRestartConstantResolveCount = 8
    BridgeVgpuRestartConstantResolveFailureCount = 8
    BridgeVgpuRestartLastResolveFailure = 4
    BridgeVgpuRestartLastRootDescriptorTable = 8
    BridgeVgpuRestartLastDescriptorHeapGpuStart = 8
    BridgeVgpuRestartLastDescriptorHeapCpuStart = 8
    BridgeVgpuRestartLastDescriptorHeapByteSpan = 8
    BridgeVgpuRestartLastDescriptorHeapIncrement = 4
    BridgeVgpuRestartLastCpuDescriptor = 8
    BridgeVgpuRestartLastDescriptorWord0 = 8
    BridgeVgpuRestartLastDescriptorWord1 = 8
    BridgeVgpuRestartLastDecodedGpuAddress = 8
    BridgeVgpuRestartLastUploadContextKind = 4
    BridgeVgpuRestartStartMatchesVertexOffsetCount = 8
    BridgeVgpuRestartStartMismatchesVertexOffsetCount = 8
    BridgeVgpuRestartLastConstantGpuAddress = 8
    BridgeVgpuRestartLastConstantCpuAddress = 8
    BridgeVgpuRestartLastVertexOffsetBits = 4
    BridgeVgpuRestartLastUseIndexBuffer = 4
    BridgeVgpuRestartLastIndexCount = 4
    BridgeVgpuRestartLastVfetchEndianness = 4
    BridgeVgpuRestartLastPackedIbDesc = 4
    BridgeVgpuRestartLastResetIndex = 4
    BridgeVgpuRestartLastIbBase = 4
    BridgeVgpuRestartTerrainLastVertexOffsetBits = 4
    BridgeVgpuRestartTerrainLastIndexCount = 4
    BridgeVgpuRestartTerrainLastPackedIbDesc = 4
    BridgeVgpuRestartSkyLastVertexOffsetBits = 4
    BridgeVgpuRestartSkyLastIndexCount = 4
    BridgeVgpuRestartSkyLastPackedIbDesc = 4
    BridgeVgpuHostTransferDrawCount = 8
    BridgeVgpuHostTransferExperimentSelector = 4
    BridgeVgpuHostTransferLastCandidate = 4
    BridgeVgpuHostTransferLastClassification = 4
    BridgeVgpuHostTransferLastPipelineState = 8
    BridgeVgpuHostTransferLastRootSignature = 8
    BridgeVgpuHostSetDescriptorHeapsCount = 8
    BridgeVgpuHostSetGraphicsRootDescriptorTableCount = 8
    BridgeVgpuHostTransferLastDescriptorHeapCount = 4
    BridgeVgpuHostTransferLastDescriptorHeap0 = 8
    BridgeVgpuHostTransferLastDescriptorHeap1 = 8
    BridgeVgpuHostTransferLastDescriptorHeap0GpuStart = 8
    BridgeVgpuHostTransferLastDescriptorHeap1GpuStart = 8
    BridgeVgpuHostTransferLastRootDescriptorTableMask = 8
    BridgeVgpuHostTransferLastRootDescriptorTable0 = 8
    BridgeVgpuHostTransferLastRootDescriptorTable1 = 8
    BridgeVgpuHostEdramRestoreDrawSkipEnabled = 4
    BridgeVgpuHostEdramRestoreDrawCandidateCount = 8
    BridgeVgpuHostEdramRestoreDrawSkipCount = 8
    BridgeVgpuHostEdramRestoreDrawLastDrawCall = 8
    BridgeVgpuHostEdramRestoreDrawLastPipelineState = 8
    BridgeVgpuHostEdramRestoreDrawLastRootSignature = 8
    BridgeVgpuFullscreenScissorFixEnabled = 4
    BridgeVgpuFullscreenScissorFixCount = 8
    BridgeVgpuFullscreenScissorFixFailure = 4
    BridgeVgpuMsaaViewportFixEnabled = 4
    BridgeVgpuMsaaViewportCandidateCount = 8
    BridgeVgpuMsaaViewportFixCount = 8
    BridgeVgpuMsaaViewportFixFailure = 4
    BridgeVgpuMsaaViewportLastPipelineState = 8
    BridgeVgpuMsaaViewportLastOriginalWidthBits = 4
    BridgeVgpuMsaaViewportLastReplacementWidthBits = 4
    BridgeVgpuMsaaViewportLastVertexCount = 4
    BridgeVgpuEdramRestoreDrawCandidateCount = 8
    BridgeVgpuEdramRestoreDrawSkipCount = 8
    BridgeVgpuEdramRestoreDrawHashMismatchCount = 8
    BridgeVgpuEdramRestoreDrawGuardFailure = 4
    BridgeVgpuEdramRestoreDrawLastRecord = 8
    BridgeVgpuEdramRestoreDrawLastPipelineState = 8
    BridgeVgpuEdramRestoreDrawLastCachedBlobSize = 8
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash0 = 8
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash1 = 8
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash2 = 8
    BridgeVgpuEdramRestoreDrawLastCachedBlobHash3 = 8
    BridgeVgpuEdramRestoreExperimentSelector = 4
    BridgeVgpuEdramRestoreExperimentCandidateCount = 4
    BridgeVgpuEdramRestoreExperimentLastCandidate = 4
    BridgeVgpuEdramRestoreExperimentHitCount = 8
    BridgeVgpuEdramRestoreExperimentSkipCount = 8
    BridgeVgpuEdramRestoreExperimentLastCreateCount = 8
    BridgeVgpuDrawRecordCallCount = 8
    BridgeVgpuDrawRecordInterestingCount = 8
    BridgeVgpuDrawRecordUniqueCount = 8
    BridgeVgpuDrawRecordLastRecord = 8
    BridgeVgpuDrawRecordLastCommandList = 8
    BridgeVgpuDrawRecordLastCommandContext = 8
    BridgeVgpuDrawRecordLastRootSignature = 8
    BridgeVgpuDrawRecordLastPipelineState = 8
    BridgeVgpuDrawRecordLastViewportWidthBits = 4
    BridgeVgpuDrawRecordLastViewportHeightBits = 4
    BridgeVgpuDrawRecordLastViewportMinDepthBits = 4
    BridgeVgpuDrawRecordLastViewportMaxDepthBits = 4
    BridgeVgpuDrawRecordLastViewportTopLeftXBits = 4
    BridgeVgpuDrawRecordLastViewportTopLeftYBits = 4
    BridgeVgpuDrawRecordLastScissorRight = 4
    BridgeVgpuDrawRecordLastScissorBottom = 4
    BridgeVgpuDrawRecordLastRecordKind = 4
    BridgeVgpuDrawRecordLastVertexCount = 4
    BridgeVgpuDrawRecordLastStartVertex = 4
    BridgeVgpuPipelineLastVertexShaderSize = 8
    BridgeVgpuPipelineLastPixelShaderSize = 8
    BridgeVgpuPipelineLastSampleCount = 4
    BridgeVgpuPipelineLastRenderTargetFormat = 4
    BridgeVgpuPipelineLastInputElementCount = 4
    BridgeVgpuPipelineLastExpectedInputLayout = 4
    BridgeVgpuPipelineLastExpectedFixedState = 4
    BridgeVgpuPipelineStateCreateFailureCount = 8
    BridgeVgpuPipelineStateLastCreateResult = 4
    BridgeVgpuPipelineStateLastCreateOutput = 8
    BridgeVgpuPipelineStateLastFailureResult = 4
    BridgeVgpuPipelineStateLastFailureCreateSequence = 8
    BridgeVgpuNullPipelineStateGuardInstalled = 4
    BridgeVgpuNullPipelineStateGuardFailure = 4
    BridgeVgpuNullPipelineStateSkipCount = 8
    BridgeVgpuNullPipelineStateLastThreadId = 4
    BridgeVgpuNullPipelineStateLastRecord = 8
    BridgeVgpuNullPipelineStateLastCommandList = 8
    BridgeVgpuNullPipelineStateLastCommandContext = 8
    BridgeVgpuNullPipelineStateLastCachedPso = 8
    BridgeVgpuNullPipelineStateLastRootSignature = 8
    BridgeVgpuNullPipelineStateLastRecordKind = 4
    BridgeVgpuNullPipelineStateLastVertexCount = 4
    BridgeVgpuNullPipelineStateLastStartVertex = 4
}
$missingExports = @()
foreach ($name in @($fields.Keys)) {
    if (-not $exportRvas.ContainsKey($name)) {
        $missingExports += $name
        if ($AllowMissingExports) {
            $fields.Remove($name)
        }
    }
}
if ($missingExports.Count -ne 0 -and -not $AllowMissingExports) {
    throw "The deployed AOT DLL does not export $($missingExports -join ', ')."
}

Add-Type @'
using System;
using System.Runtime.InteropServices;

public static class Ac6RuntimeMemoryNative
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(
        uint desiredAccess,
        bool inheritHandle,
        int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(
        IntPtr process,
        IntPtr baseAddress,
        byte[] buffer,
        UIntPtr size,
        out UIntPtr bytesRead);

    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@

$processVmRead = 0x0010
$processQueryInformation = 0x0400
$processHandle = [Ac6RuntimeMemoryNative]::OpenProcess(
    $processVmRead -bor $processQueryInformation,
    $false,
    $targetProcess.Id)
if ($processHandle -eq [IntPtr]::Zero) {
    $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
    throw "OpenProcess failed for $($targetProcess.Id) with error $errorCode."
}

$moduleBase = $module.BaseAddress.ToInt64()
try {
    for ($sampleIndex = 0; $sampleIndex -lt $Samples; $sampleIndex++) {
        $sample = [ordered]@{
            Timestamp = (Get-Date).ToString('o')
            ProcessId = $targetProcess.Id
        }
        if ($missingExports.Count -ne 0) {
            $sample.MissingExports = $missingExports
        }

        foreach ($entry in $fields.GetEnumerator()) {
            $length = [int]$entry.Value
            $buffer = [byte[]]::new($length)
            $bytesRead = [UIntPtr]::Zero
            $address = [IntPtr](
                $moduleBase + [int64]$exportRvas[$entry.Key])
            $readResult = [Ac6RuntimeMemoryNative]::ReadProcessMemory(
                $processHandle,
                $address,
                $buffer,
                [UIntPtr]::new([uint64]$length),
                [ref]$bytesRead)
            if (-not $readResult -or $bytesRead.ToUInt64() -ne $length) {
                $errorCode =
                    [Runtime.InteropServices.Marshal]::GetLastWin32Error()
                throw "ReadProcessMemory failed for $($entry.Key) with error $errorCode."
            }

            if ($length -eq 8) {
                $sample[$entry.Key] = [BitConverter]::ToUInt64($buffer, 0)
            } else {
                $sample[$entry.Key] = [BitConverter]::ToUInt32($buffer, 0)
            }
        }

        [pscustomobject]$sample
        if ($sampleIndex + 1 -lt $Samples -and
            $IntervalMilliseconds -ne 0) {
            Start-Sleep -Milliseconds $IntervalMilliseconds
        }
    }
} finally {
    [void][Ac6RuntimeMemoryNative]::CloseHandle($processHandle)
}

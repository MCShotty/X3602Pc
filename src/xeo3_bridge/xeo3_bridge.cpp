#include "xeo3_bridge/xenonrecomp_overrides.h"
#include "xeo3_bridge/ac6_audio_poll.h"
#include "xeo3_bridge/ac6_vd_swap_trace.h"
#include "xeo3_bridge/ac6_fast_critical_section.h"
#include "xeo3_bridge/ac6_fast_irql.h"
#include "xeo3_bridge/ac6_fast_spin_lock.h"
#include "xeo3_bridge/ac6_sync_dvd_io.h"
#include "xeo3_bridge/fiber_frame_registry.h"
#include "xeo3_bridge/xeo3_host_unmapped_observer.h"
#include "xeo3_bridge/xeo3_kernel_continuations.h"
#include "xeo3_bridge/xeo3_state_sync.h"
#include "xeo3_bridge/xeo3_vgpu_patch.h"
#include "xeo3_contract/xeo3_contract.h"

#include "ppc_recomp_shared.h"

#include <Windows.h>
#include <Xinput.h>
#include <bcrypt.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "xinput.lib")

extern "C" IMAGE_DOS_HEADER __ImageBase;

struct BridgeCriticalSectionEvent
{
    std::uint64_t sequence;
    std::uint64_t tickCount;
    std::uint64_t cpuState;
    std::uint64_t failureStreak;
    std::uint32_t hostThreadId;
    std::uint32_t operation;
    std::uint32_t thunk;
    std::uint32_t lockAddress;
    std::uint32_t callerLr;
    std::uint32_t r13;
    std::uint32_t currentThread;
    std::int32_t lockCountBefore;
    std::uint32_t recursionBefore;
    std::uint32_t ownerBefore;
    std::uint32_t result;
    std::int32_t lockCountAfter;
    std::uint32_t recursionAfter;
    std::uint32_t ownerAfter;
};

struct BridgeThreadImportState
{
    std::uint64_t sequence;
    std::uint64_t importCount;
    std::uintptr_t cpuState;
    std::uintptr_t guestMemory;
    std::uint32_t hostThreadId;
    std::uint32_t lastGuestIar;
    std::uint32_t lastThunk;
    std::uint32_t lastTarget;
    std::uint32_t callerLr;
    std::uint32_t r1;
    std::uint32_t r3;
    std::uint32_t r4;
    std::uint32_t r5;
    std::uint32_t r6;
    std::uint32_t r7;
    std::uint32_t r8;
    std::uint32_t r9;
    std::uint32_t r10;
    std::uint32_t r13;
    std::uint32_t result;
};

static_assert(sizeof(BridgeThreadImportState) == 96);

constexpr std::size_t kAc6ImportCount = 0
#define XEO3_AC6_IMPORT(index, address, name) +1
#include "ac6_imports.inc"
#undef XEO3_AC6_IMPORT
    ;
static_assert(kAc6ImportCount == 229);

extern "C"
{
__declspec(dllexport) std::uintptr_t
    PrecompiledPointers[xeo3::kHostPointerCount]{};

__declspec(dllexport) std::uint32_t PrecompiledImportTable[] = {
#define XEO3_AC6_IMPORT(index, address, name) address, 0,
#include "ac6_imports.inc"
#undef XEO3_AC6_IMPORT
    0,
    0,
};

__declspec(dllexport) volatile std::uint64_t BridgeDispatchCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeImportCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeImportCountersEnabled = 0;
__declspec(dllexport) volatile std::uint32_t BridgeImportCounterCount =
    static_cast<std::uint32_t>(kAc6ImportCount);
alignas(64) __declspec(dllexport) volatile std::uint64_t
    BridgeImportCounts[kAc6ImportCount]{};
__declspec(dllexport) volatile std::uint32_t
    BridgeFunctionLookupEntryCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeIntegerImportSyncEnabled = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeIntegerImportSyncAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeIntegerImportSyncHitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeIntegerImportSyncFallbackCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeIntegerImportSyncLastThunk = 0;
__declspec(dllexport) volatile std::uint32_t BridgeEventTraceEnabled = 0;
__declspec(dllexport) volatile std::uint32_t BridgeThreadImportTraceEnabled = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastSynchronizationTelemetryEnabled = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastCriticalSectionsEnabled = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastCriticalSectionAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastCriticalSectionHitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastCriticalSectionFallbackCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastCriticalSectionSignalCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastCriticalSectionLastThunk = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastCriticalSectionLastAddress = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastCriticalSectionLastThread = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastCriticalSectionLastDisposition = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastSpinLocksEnabled = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastSpinLockAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastSpinLockHitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastSpinLockFallbackCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastSpinLockContentionCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastSpinLockLastThunk = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastSpinLockLastAddress = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastIrqlEnabled = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeFastIrqlTelemetryEnabled = 0;
__declspec(dllexport) volatile std::uint64_t BridgeFastIrqlAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeFastIrqlHitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastIrqlNativeFallbackCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeFastIrqlInvalidFallbackCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastIrqlLastThunk = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastIrqlLastOldIrql = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastIrqlLastNewIrql = 0;
__declspec(dllexport) volatile std::uint32_t BridgeFastIrqlLastPendingIrql = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeRaiseIrqlOldPassiveCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeRaiseIrqlOldApcCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeRaiseIrqlOldDispatchCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeRaiseIrqlUnexpectedCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeIndirectCallCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeIndirectTelemetryEnabled = 0;
__declspec(dllexport) volatile std::uint32_t BridgeIndirectStateSyncEnabled = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastIndirectTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastIndirectCallerIar = 0;
__declspec(dllexport) volatile std::uint64_t BridgeFatalIndirectCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectCallerIar = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectObject = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectVtable = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectSlot0 = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectSlot1 = 0;
__declspec(dllexport) volatile std::uint64_t BridgeLastFatalIndirectArgument4 = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastFatalIndirectThreadId = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerEntryCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerLoopCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackDispatchCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackReturnCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerActiveDecrementCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeQueueWaitLoopCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastQueueWaitObject = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastQueueWaitCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeQueueWaitPositiveCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeQueueWaitPauseCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeQueueWaitSwitchCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeQueueWaitSleepCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackTargetCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerCallbackTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerCallbackCallerIar = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryObject = 0;
__declspec(dllexport) volatile std::uint64_t BridgeLastWorkerPrimaryPayload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryObject = 0;
__declspec(dllexport) volatile std::uint64_t BridgeActiveWorkerPrimaryPayload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryThreadId = 0;
__declspec(dllexport) volatile std::uint64_t BridgeActiveWorkerPrimaryClaimCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerDestroyTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastWorkerDestroyObject = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollClockFallbackEnabled = 1;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollTraceEnabled = 0;
__declspec(dllexport) volatile std::uint32_t BridgeGuestIarTelemetryEnabled = 0;
#if defined(XEO3_CONTINUOUS_STATE_PUBLICATION)
__declspec(dllexport) volatile std::uint32_t
    BridgeContinuousStatePublicationMode = 1;
#else
__declspec(dllexport) volatile std::uint32_t
    BridgeContinuousStatePublicationMode = 0;
#endif
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollCallCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollReturnCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollSampleCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollContinueCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollTimeoutCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollExitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeAc6AudioPollTimeBaseSampleCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeAc6AudioPollClockFallbackCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollDelayCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGuestIar = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastObject = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastFrame = 0;
__declspec(dllexport) volatile std::uintptr_t BridgeAc6AudioPollLastGuestMemory = 0;
__declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollLastTimeBase = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGlobalTick = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGlobalClock = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastObjectClock = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastBaselineTick = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastDelta = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastConsumerPointer = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastConsumerValue = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastObservedConsumer = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastRequiredDistance = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastAvailableDistance = 0;
__declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastReturn = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastResolvedTick = 0;
__declspec(dllexport) volatile std::uint32_t BridgeSynchronousQueueEnabled = 1;
__declspec(dllexport) volatile std::uint64_t BridgeSynchronousQueueBypassCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeSynchronousQueueSubmitCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeSynchronousQueueTaskCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeSynchronousQueueFailureCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeSynchronousQueueFallbackCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeSynchronousQueueLastObject = 0;
__declspec(dllexport) volatile std::uint32_t BridgeSynchronousQueueLastTask = 0;
__declspec(dllexport) volatile std::uint32_t BridgeSynchronousQueueLastTarget = 0;
__declspec(dllexport) volatile std::uint32_t BridgeSynchronousQueueLastPhase = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot0Target = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot0Object = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerPrimarySlot0Payload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot0ThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot1Target = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot1Object = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerPrimarySlot1Payload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot1ThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot2Target = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot2Object = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerPrimarySlot2Payload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot2ThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot3Target = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot3Object = 0;
__declspec(dllexport) volatile std::uint64_t BridgeWorkerPrimarySlot3Payload = 0;
__declspec(dllexport) volatile std::uint32_t BridgeWorkerPrimarySlot3ThreadId = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastGuestIar = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastImportThunk = 0;
__declspec(dllexport) volatile std::uint32_t BridgeLastImportTarget = 0;
__declspec(dllexport) volatile std::uint64_t BridgeRecoveredContextCount = 0;
__declspec(dllexport) volatile std::uint64_t BridgeDispatchExitCount = 0;
__declspec(dllexport) volatile std::uintptr_t BridgeLastGuestMemory = 0;
__declspec(dllexport) volatile std::uintptr_t BridgeLastOuterHostFence = 0;
__declspec(dllexport) volatile std::uintptr_t BridgeLastNestedHostFence = 0;
__declspec(dllexport) volatile std::uint64_t BridgeVirtualPadPollCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeVirtualPadLastButtons = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeCriticalSectionEventCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeRtlTryLastFailureStreak = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveReadCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveReadAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveReadFailureCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastPhase = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastCallerIar = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastSignalThreadId = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveReadLastOffset = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastIndex = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastHandle = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastLength = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastCompleted = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastError = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveSignalCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeSynchronousArchiveSignalFailureCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastEvent = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastApcRoutine = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastApcContext = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastIoStatusBlock = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastBuffer = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastOffsetPointer = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastThreadId = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastIoStatus = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastIoInformation = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveSignalLastStatus = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeSynchronousArchiveReadLastGuestMemory = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeSynchronousArchiveReadLastHostBuffer = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeSynchronousArchiveReadLastFailureGuestMemory = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeSynchronousArchiveReadLastFailureHostBuffer = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeSynchronousArchiveReadLastFailureLength = 0;
__declspec(dllexport) volatile std::uint64_t BridgeNtWaitCount = 0;
__declspec(dllexport) volatile std::uint32_t BridgeNtWaitLastHandle = 0;
__declspec(dllexport) volatile std::uint32_t BridgeNtWaitLastWaitMode = 0;
__declspec(dllexport) volatile std::uint32_t BridgeNtWaitLastTimeout = 0;
__declspec(dllexport) volatile std::uint32_t BridgeNtWaitLastStatus = 0;
__declspec(dllexport) volatile std::uint32_t BridgeNtWaitLastCallerLr = 0;
__declspec(dllexport) volatile std::uint64_t BridgeNtReleaseSemaphoreCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeNtReleaseSemaphoreLastHandle = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeNtReleaseSemaphoreLastReleaseCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeNtReleaseSemaphoreLastPreviousCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeNtReleaseSemaphoreLastStatus = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeNtReleaseSemaphoreLastCallerLr = 0;
__declspec(dllexport) BridgeCriticalSectionEvent
    BridgeCriticalSectionEvents[256]{};
__declspec(dllexport) BridgeThreadImportState
    BridgeThreadImportStates[64]{};

void BridgeQueueWaitCooperativeYield(
    const std::uint32_t objectAddress,
    const std::uint32_t count) noexcept
{
    static thread_local std::uint32_t lastObjectAddress = 0;
    static thread_local std::uint32_t iteration = 0;

    if (!xeo3::IsQueueWaitActive(count))
    {
        lastObjectAddress = 0;
        iteration = 0;
        return;
    }

    if (lastObjectAddress != objectAddress)
    {
        lastObjectAddress = objectAddress;
        iteration = 0;
    }

    ++iteration;
    switch (xeo3::SelectQueueWaitAction(count, iteration))
    {
    case xeo3::QueueWaitAction::pause:
        BridgeQueueWaitPauseCount = BridgeQueueWaitPauseCount + 1;
        _mm_pause();
        break;
    case xeo3::QueueWaitAction::switchThread:
        BridgeQueueWaitSwitchCount = BridgeQueueWaitSwitchCount + 1;
        static_cast<void>(SwitchToThread());
        break;
    case xeo3::QueueWaitAction::sleepOneMillisecond:
        BridgeQueueWaitSleepCount = BridgeQueueWaitSleepCount + 1;
        Sleep(1);
        break;
    case xeo3::QueueWaitAction::reset:
        lastObjectAddress = 0;
        iteration = 0;
        break;
    }
}

void BridgeAc6AudioPollObserve(
    const std::uint32_t guestIar,
    const std::uint32_t r3,
    const std::uint32_t r9,
    const std::uint32_t r10,
    const std::uint64_t r11,
    const std::uint32_t,
    const std::uint32_t r29,
    const std::uint32_t r30,
    const std::uint32_t r31) noexcept
{
    BridgeAc6AudioPollLastGuestIar = guestIar;
    BridgeAc6AudioPollLastThreadId = GetCurrentThreadId();
    BridgeAc6AudioPollLastGuestMemory = BridgeLastGuestMemory;

    switch (guestIar)
    {
    case 0x821E6238U:
        BridgeAc6AudioPollTimeBaseSampleCount =
            BridgeAc6AudioPollTimeBaseSampleCount + 1;
        BridgeAc6AudioPollLastTimeBase = r11;
        break;
    case 0x821E6240U:
        BridgeAc6AudioPollCallCount = BridgeAc6AudioPollCallCount + 1;
        break;
    case 0x821E6248U:
        BridgeAc6AudioPollReturnCount = BridgeAc6AudioPollReturnCount + 1;
        BridgeAc6AudioPollLastReturn = r3;
        break;
    case 0x821E6264U:
        BridgeAc6AudioPollLastRequiredDistance = r9;
        BridgeAc6AudioPollLastAvailableDistance =
            static_cast<std::uint32_t>(r11);
        break;
    case 0x821E6B3CU:
        BridgeAc6AudioPollLastObject = r29;
        BridgeAc6AudioPollLastFrame = r31;
        BridgeAc6AudioPollLastConsumerPointer =
            static_cast<std::uint32_t>(r11);
        BridgeAc6AudioPollLastConsumerValue = r9;
        BridgeAc6AudioPollLastObservedConsumer = r10;
        break;
    case 0x821E6B70U:
        BridgeAc6AudioPollSampleCount = BridgeAc6AudioPollSampleCount + 1;
        BridgeAc6AudioPollLastObject = r29;
        BridgeAc6AudioPollLastFrame = r31;
        BridgeAc6AudioPollLastGlobalTick = r30;
        BridgeAc6AudioPollLastGlobalClock = r3;
        BridgeAc6AudioPollLastObjectClock =
            static_cast<std::uint32_t>(r11);
        break;
    case 0x821E6B74U:
        BridgeAc6AudioPollLastBaselineTick =
            static_cast<std::uint32_t>(r11);
        break;
    case 0x821E6B78U:
        BridgeAc6AudioPollLastDelta = static_cast<std::uint32_t>(r11);
        break;
    case 0x821E6B80U:
        BridgeAc6AudioPollContinueCount =
            BridgeAc6AudioPollContinueCount + 1;
        break;
    case 0x821E6B88U:
        BridgeAc6AudioPollTimeoutCount =
            BridgeAc6AudioPollTimeoutCount + 1;
        break;
    case 0x821E6B90U:
        BridgeAc6AudioPollExitCount = BridgeAc6AudioPollExitCount + 1;
        break;
    default:
        break;
    }
}

std::uint32_t BridgeAc6AudioPollResolveTick(
    const std::uint32_t guestTick,
    std::uint8_t* const guestMemory,
    const std::uint32_t frameAddress) noexcept
{
    constexpr auto baselineOffset = std::uint32_t{12};
    const auto frameAddressValid =
        guestMemory != nullptr &&
        frameAddress <= std::numeric_limits<std::uint32_t>::max() -
            baselineOffset;
    const auto fallbackEnabled =
        BridgeAc6AudioPollClockFallbackEnabled != 0 && frameAddressValid;
    const auto baselineAddress = frameAddress + baselineOffset;
    auto encodedBaselineTick = std::uint32_t{0};
    if (fallbackEnabled)
    {
        std::memcpy(
            &encodedBaselineTick,
            xeo3::GuestMemoryPointer(guestMemory, baselineAddress),
            sizeof(encodedBaselineTick));
    }
    const auto baselineTick = _byteswap_ulong(encodedBaselineTick);
    const auto hostTick = guestTick == 0 && fallbackEnabled
        ? GetTickCount()
        : 0;
    const auto resolution = xeo3::ac6_audio::ResolvePollTick(
        guestTick,
        baselineTick,
        hostTick,
        fallbackEnabled);
    if (resolution.initializeBaseline)
    {
        const auto encodedResolvedTick = _byteswap_ulong(resolution.tick);
        std::memcpy(
            xeo3::GuestMemoryPointer(guestMemory, baselineAddress),
            &encodedResolvedTick,
            sizeof(encodedResolvedTick));
    }
    if (resolution.usedFallback && BridgeAc6AudioPollTraceEnabled != 0)
    {
        BridgeAc6AudioPollClockFallbackCount =
            BridgeAc6AudioPollClockFallbackCount + 1;
        BridgeAc6AudioPollLastResolvedTick = resolution.tick;
    }
    return resolution.tick;
}

void XeO3AotThunk();
void XeO3CallMappedGuest(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostFence);
void XeO3CallHostGuest(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostTarget,
    void* hostFence);
}

namespace
{
struct WorkerPrimarySlot
{
    volatile std::uint32_t* target;
    volatile std::uint32_t* object;
    volatile std::uint64_t* payload;
    volatile std::uint32_t* threadId;
};

const std::array<WorkerPrimarySlot, 4> kWorkerPrimarySlots{{
    {
        &BridgeWorkerPrimarySlot0Target,
        &BridgeWorkerPrimarySlot0Object,
        &BridgeWorkerPrimarySlot0Payload,
        &BridgeWorkerPrimarySlot0ThreadId,
    },
    {
        &BridgeWorkerPrimarySlot1Target,
        &BridgeWorkerPrimarySlot1Object,
        &BridgeWorkerPrimarySlot1Payload,
        &BridgeWorkerPrimarySlot1ThreadId,
    },
    {
        &BridgeWorkerPrimarySlot2Target,
        &BridgeWorkerPrimarySlot2Object,
        &BridgeWorkerPrimarySlot2Payload,
        &BridgeWorkerPrimarySlot2ThreadId,
    },
    {
        &BridgeWorkerPrimarySlot3Target,
        &BridgeWorkerPrimarySlot3Object,
        &BridgeWorkerPrimarySlot3Payload,
        &BridgeWorkerPrimarySlot3ThreadId,
    },
}};

int ClaimWorkerPrimarySlot(
    const std::uint32_t threadId,
    const std::uint32_t target,
    const std::uint32_t object,
    const std::uint64_t payload) noexcept
{
    for (std::size_t index = 0; index < kWorkerPrimarySlots.size(); ++index)
    {
        const auto& slot = kWorkerPrimarySlots[index];
        if (InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(slot.threadId),
                static_cast<LONG>(threadId),
                0) != 0)
        {
            continue;
        }
        *slot.target = target;
        *slot.object = object;
        *slot.payload = payload;
        return static_cast<int>(index);
    }
    return -1;
}

void ReleaseWorkerPrimarySlot(
    const int index,
    const std::uint32_t threadId) noexcept
{
    if (index < 0 ||
        static_cast<std::size_t>(index) >= kWorkerPrimarySlots.size())
    {
        return;
    }
    const auto& slot = kWorkerPrimarySlots[static_cast<std::size_t>(index)];
    if (*slot.threadId != threadId)
    {
        return;
    }
    *slot.target = 0;
    *slot.object = 0;
    *slot.payload = 0;
    InterlockedExchange(
        reinterpret_cast<volatile LONG*>(slot.threadId),
        0);
}

constexpr std::uint32_t kImageBase = 0x82000000;
constexpr std::uint32_t kImageSize = 0x00AA0000;

enum class NestedStateSync
{
    full,
    integerOnly,
};
constexpr DWORD kBridgeException = 0xE0423603;
constexpr DWORD kExpectedEmuTimestamp = 0x6A585A77;
constexpr DWORD kExpectedEmuImageSize = 0x00D97000;
constexpr std::uintptr_t kFlsIndexRva = 0x0011F2A0;
constexpr std::uintptr_t kUnmappedDispatchRva = 0x0009C490;
constexpr std::ptrdiff_t kFlsCpuStateOffset = 0x10;
constexpr std::ptrdiff_t kCpuStateAnchorOffset = 0x80;
constexpr std::array<std::uint8_t, 32> kExpectedEmuSha256{
    0xD1, 0x57, 0x8E, 0x07, 0xB5, 0x33, 0xE3, 0x91,
    0xD8, 0xA8, 0x1C, 0x33, 0x0D, 0x54, 0x93, 0xBA,
    0x2D, 0x45, 0xB2, 0x52, 0x49, 0x0A, 0x81, 0x8E,
    0xC1, 0x48, 0xDA, 0xBE, 0x1B, 0xA2, 0x4D, 0x06,
};

enum class EventKind : std::uint32_t
{
    Dispatch,
    Import,
    IndirectFallback,
    MmioRead,
    MmioWrite,
    Failure,
    RecoveredContext,
    DispatchExit,
    ArchiveRead,
    KernelContinuation,
    Count,
};

struct ActiveFrame
{
    void* cpuState;
    std::uint8_t* guestMemory;
    xeo3::TranslatedState* translated;
    void* hostFence;
    ActiveFrame* next;
};

struct DistanceCopyTrace
{
    bool armed = false;
    const char* kind = nullptr;
    std::uint32_t destination = 0;
    std::uint32_t source = 0;
    std::uint32_t byteCount = 0;
    std::array<std::uint8_t, 640> sourceBefore{};
    std::array<std::uint8_t, 640> destinationBefore{};
};

struct CodeLengthTokenTrace
{
    bool armed = false;
    bool sequenceArmed = false;
    bool expectedArmed = false;
    std::uint32_t stackPointer = 0;
    std::uint32_t startIndex = 0;
    std::uint64_t startIndexRaw = 0;
    std::uint32_t owner = 0;
    std::uint32_t inputIndexPointer = 0;
    std::uint32_t inputBase = 0;
    std::uint64_t inputEnd = 0;
    std::uint64_t inputIndex = 0;
    std::uint64_t bitBuffer = 0;
    std::uint32_t bitCount = 0;
    std::uint32_t sequenceOwner = 0;
    std::uint32_t sequenceInputIndexPointer = 0;
    std::uint32_t sequenceInputBase = 0;
    std::uint64_t sequenceInputEnd = 0;
    std::uint64_t sequenceInputIndex = 0;
    std::uint64_t sequenceBitBuffer = 0;
    std::uint32_t sequenceBitCount = 0;
    std::uint32_t expectedStackPointer = 0;
    std::uint32_t expectedCount = 0;
    std::array<std::uint16_t, 320> expectedValues{};
};

enum class CodeLengthEventKind : std::uint8_t
{
    Refill,
    TokenEnd,
};

struct CodeLengthEvent
{
    std::uint64_t sequence = 0;
    CodeLengthEventKind kind = CodeLengthEventKind::Refill;
    std::uint32_t stackPointer = 0;
    std::uint32_t index = 0;
    std::uint32_t symbol = 0;
    std::uint32_t totalCount = 0;
    std::uint64_t valueA = 0;
    std::uint64_t valueB = 0;
    std::uint64_t valueC = 0;
};

struct CodeLengthEventRing
{
    std::array<CodeLengthEvent, 1024> events{};
    std::uint64_t nextSequence = 0;
};

std::vector<xeo3::MappingEntry> g_mappings;
std::vector<PPCFunc*> g_functionLookup;
void* g_hostInterface = nullptr;
std::size_t g_functionCount = 0;
std::atomic<std::uint64_t> g_eventSequence{0};
std::atomic<int> g_hostProfileStatus{0};
std::atomic<bool> g_huffmanInputCaptured{false};
std::atomic<bool> g_distanceCopyCaptured{false};
std::atomic<bool> g_codeLengthTokenCaptured{false};
std::atomic<bool> g_codeLengthRefillCaptured{false};
bool g_kernelContinuationsEnabled = false;
std::atomic<bool> g_codeLengthPrefixCaptured{false};
std::atomic<bool> g_indirectTypeIndexCaptured{false};
std::atomic<std::uint64_t> g_criticalSectionEventSequence{0};
std::atomic<std::uint64_t> g_threadImportStateSequence{0};
thread_local DistanceCopyTrace g_distanceCopyTrace;
thread_local CodeLengthTokenTrace g_codeLengthTokenTrace;
thread_local CodeLengthEventRing g_codeLengthEvents;
thread_local std::uint64_t g_targetLockTryFailureStreak = 0;
xeo3::FiberFrameRegistry<ActiveFrame> g_activeFrames;
std::array<
    std::atomic<std::uint64_t>,
    static_cast<std::size_t>(EventKind::Count)> g_eventCounts{};

using ReadGuestTimeBase = std::uint64_t (*)();
using MmioRead32 = std::uint32_t (*)(std::uint32_t guestAddress);
using MmioWrite32 = void (*)(
    std::uint32_t guestAddress,
    std::uint32_t value);

bool ShouldEmitEvent(const EventKind kind) noexcept
{
    if (!IsDebuggerPresent())
    {
        return false;
    }

    if (kind == EventKind::Failure)
    {
        return true;
    }
    if (BridgeEventTraceEnabled == 0)
    {
        return false;
    }
    if (kind == EventKind::RecoveredContext ||
        kind == EventKind::IndirectFallback)
    {
        return true;
    }

    auto& count = g_eventCounts[static_cast<std::size_t>(kind)];
    const auto current =
        count.fetch_add(1, std::memory_order_relaxed) + 1;
    constexpr std::uint64_t kInitialEventLimit = 512;
    return current <= kInitialEventLimit ||
        (current & (current - 1)) == 0;
}

void EmitEvent(
    const EventKind kind,
    const std::uint32_t guestIar,
    const std::uint32_t target,
    const std::uint32_t detail) noexcept
{
    if (!ShouldEmitEvent(kind))
    {
        return;
    }

    const auto sequence =
        g_eventSequence.fetch_add(1, std::memory_order_relaxed) + 1;
    char buffer[256]{};
    const auto length = std::snprintf(
        buffer,
        std::size(buffer),
        "{\"xeo3_ac6\":\"bridge\",\"seq\":%" PRIu64
        ",\"event\":%u,\"tid\":%lu,\"iar\":\"0x%08X\","
        "\"target\":\"0x%08X\",\"detail\":%u}\n",
        sequence,
        static_cast<unsigned>(kind),
        GetCurrentThreadId(),
        guestIar,
        target,
        detail);
    if (length > 0)
    {
        OutputDebugStringA(buffer);
    }
}

[[noreturn]] void Fail(
    const std::uint32_t guestIar,
    const std::uint32_t target,
    const std::uint32_t reason)
{
    EmitEvent(EventKind::Failure, guestIar, target, reason);
    const ULONG_PTR arguments[]{guestIar, target, reason};
    RaiseException(
        kBridgeException,
        EXCEPTION_NONCONTINUABLE,
        static_cast<DWORD>(std::size(arguments)),
        arguments);
    __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

bool HashFileSha256(
    const wchar_t* path,
    std::array<std::uint8_t, 32>& digest) noexcept
{
    const auto file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::vector<std::uint8_t> hashObject;
    bool succeeded = false;

    DWORD objectSize = 0;
    DWORD bytesWritten = 0;
    if (BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0) < 0 ||
        BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectSize),
            sizeof(objectSize),
            &bytesWritten,
            0) < 0)
    {
        goto cleanup;
    }

    try
    {
        hashObject.resize(objectSize);
    }
    catch (...)
    {
        goto cleanup;
    }

    if (BCryptCreateHash(
            algorithm,
            &hash,
            hashObject.data(),
            static_cast<ULONG>(hashObject.size()),
            nullptr,
            0,
            0) < 0)
    {
        goto cleanup;
    }

    for (;;)
    {
        std::array<std::uint8_t, 64 * 1024> buffer{};
        DWORD bytesRead = 0;
        if (!ReadFile(
                file,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytesRead,
                nullptr))
        {
            goto cleanup;
        }
        if (bytesRead == 0)
        {
            break;
        }
        if (BCryptHashData(hash, buffer.data(), bytesRead, 0) < 0)
        {
            goto cleanup;
        }
    }

    succeeded =
        BCryptFinishHash(
            hash,
            digest.data(),
            static_cast<ULONG>(digest.size()),
            0) >= 0;

cleanup:
    if (hash != nullptr)
    {
        BCryptDestroyHash(hash);
    }
    if (algorithm != nullptr)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    CloseHandle(file);
    return succeeded;
}

bool ValidatePinnedHost() noexcept
{
    const auto cached =
        g_hostProfileStatus.load(std::memory_order_acquire);
    if (cached != 0)
    {
        return cached > 0;
    }

    bool valid = false;
    const auto module = GetModuleHandleW(nullptr);
    if (module != nullptr)
    {
        const auto* const base =
            reinterpret_cast<const std::uint8_t*>(module);
        const auto* const dos =
            reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE)
        {
            const auto* const nt =
                reinterpret_cast<const IMAGE_NT_HEADERS64*>(
                    base + dos->e_lfanew);
            valid =
                nt->Signature == IMAGE_NT_SIGNATURE &&
                nt->FileHeader.TimeDateStamp == kExpectedEmuTimestamp &&
                nt->OptionalHeader.SizeOfImage == kExpectedEmuImageSize;
        }
    }

    std::array<wchar_t, 32768> path{};
    std::array<std::uint8_t, 32> digest{};
    if (valid)
    {
        const auto length = GetModuleFileNameW(
            module,
            path.data(),
            static_cast<DWORD>(path.size()));
        valid =
            length != 0 &&
            length < path.size() &&
            HashFileSha256(path.data(), digest) &&
            digest == kExpectedEmuSha256;
    }

    int expected = 0;
    const auto status = valid ? 1 : -1;
    g_hostProfileStatus.compare_exchange_strong(
        expected,
        status,
        std::memory_order_release,
        std::memory_order_relaxed);
    return valid;
}

bool IsNoCallStackAotModule() noexcept
{
    std::array<wchar_t, 32768> path{};
    const auto length = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    constexpr wchar_t suffix[] = L"_no.dll";
    constexpr auto suffixLength = std::size(suffix) - 1;
    return length >= suffixLength &&
        length < path.size() &&
        _wcsicmp(
            path.data() + length - suffixLength,
            suffix) == 0;
}

void* CurrentCpuStateAnchor() noexcept
{
    if (!ValidatePinnedHost())
    {
        return nullptr;
    }

    const auto* const base = reinterpret_cast<const std::uint8_t*>(
        GetModuleHandleW(nullptr));
    const auto flsIndex =
        *reinterpret_cast<const DWORD*>(base + kFlsIndexRva);
    if (flsIndex == FLS_OUT_OF_INDEXES)
    {
        return nullptr;
    }

    const auto* const flsValue =
        static_cast<const std::uint8_t*>(FlsGetValue(flsIndex));
    if (flsValue == nullptr)
    {
        return nullptr;
    }

    void* cpuStateObject = nullptr;
    std::memcpy(
        &cpuStateObject,
        flsValue + kFlsCpuStateOffset,
        sizeof(cpuStateObject));
    if (cpuStateObject == nullptr)
    {
        return nullptr;
    }
    return static_cast<std::uint8_t*>(cpuStateObject) +
           kCpuStateAnchorOffset;
}

bool RegisterActiveFrame(ActiveFrame& frame) noexcept
{
    return g_activeFrames.push(frame);
}

bool UnregisterActiveFrame(ActiveFrame& frame) noexcept
{
    return g_activeFrames.remove(frame);
}

ActiveFrame* FindActiveFrame(
    PPCContext& context,
    std::uint8_t* guestMemory) noexcept
{
    auto* const directFrame =
        static_cast<ActiveFrame*>(context.xeo3ActiveFrame);
    if (directFrame != nullptr &&
        directFrame->translated != nullptr &&
        &directFrame->translated->ppc == &context &&
        directFrame->guestMemory == guestMemory)
    {
        return directFrame;
    }

    return g_activeFrames.find(
        [&](const ActiveFrame& frame)
        {
            return frame.translated != nullptr &&
                   &frame.translated->ppc == &context &&
                   frame.guestMemory == guestMemory;
        });
}

PPCFunc* FindFunction(const std::uint32_t guestAddress) noexcept
{
    if (auto* const continuation =
            xeo3::kernel::FindContinuation(guestAddress);
        continuation != nullptr)
    {
        return continuation;
    }

    if (guestAddress < kImageBase)
    {
        return nullptr;
    }
    const auto relativeAddress = guestAddress - kImageBase;
    if (relativeAddress >= kImageSize || (relativeAddress & 3U) != 0)
    {
        return nullptr;
    }

    const auto index = static_cast<std::size_t>(relativeAddress >> 2U);
    return index < g_functionLookup.size()
        ? g_functionLookup[index]
        : nullptr;
}

void EnsureKernelContinuationReady(
    void* const cpuState,
    std::uint8_t* const guestMemory,
    const std::uint32_t sourceIar)
{
    if (!g_kernelContinuationsEnabled)
    {
        return;
    }

    const auto result =
        xeo3::kernel::EnsureInstalled(cpuState, guestMemory);
    if (result == xeo3::kernel::InstallResult::fatal)
    {
        Fail(
            sourceIar,
            xeo3::kernel::kContinuation8005F0B4,
            0x100U + BridgeKernelContinuationLastFailure);
    }
}

bool ExecuteViaXeO3(
    PPCContext& context,
    std::uint8_t* guestMemory,
    const std::uint32_t sourceIar,
    const std::uint32_t targetIar,
    const EventKind eventKind,
    const std::uint32_t detail,
    const std::uint32_t missingExecuteReason,
    void* const directHostTarget = nullptr,
    const NestedStateSync requestedStateSync = NestedStateSync::full)
{
    auto* const frame = FindActiveFrame(context, guestMemory);
    xeo3::TranslatedState recovered{};
    auto* translated =
        frame == nullptr ? &recovered : frame->translated;
    auto* cpuState =
        frame == nullptr ? CurrentCpuStateAnchor() : frame->cpuState;

    if (cpuState == nullptr)
    {
        Fail(sourceIar, targetIar, 9);
    }
    auto* const fenceFrame = frame == nullptr
        ? g_activeFrames.find(
              [&](const ActiveFrame& candidate)
              {
                  return candidate.cpuState == cpuState &&
                         candidate.guestMemory == guestMemory;
              })
        : frame;
    auto* const hostFence =
        fenceFrame == nullptr ? nullptr : fenceFrame->hostFence;
    if (hostFence == nullptr)
    {
        Fail(sourceIar, targetIar, 0x0EU);
    }
    if (frame == nullptr)
    {
        xeo3::CopyFromXeO3(
            xeo3::CpuStateView(cpuState),
            recovered);
        recovered.ppc = context;
        recovered.ppc.xeo3ActiveFrame = nullptr;
        BridgeRecoveredContextCount =
            BridgeRecoveredContextCount + 1;
        EmitEvent(
            EventKind::RecoveredContext,
            sourceIar,
            targetIar,
            0);
    }
    const auto useIntegerStateSync =
        frame != nullptr &&
        requestedStateSync == NestedStateSync::integerOnly;
    translated->ppc.xeo3CpuState = cpuState;
    translated->ppc.xeo3GuestMemory = guestMemory;
    // ExecuteViaXeO3 requires an active outer dispatch frame, and that frame
    // installs the continuation before it is registered. Rechecking the same
    // dispatch slot on every nested import made this path run hundreds of
    // millions of redundant atomic lookups during gameplay.

    const auto dispatchBase = *reinterpret_cast<const std::uintptr_t*>(
        static_cast<const std::uint8_t*>(cpuState) - 0x10);
    if (dispatchBase == 0)
    {
        Fail(sourceIar, targetIar, missingExecuteReason);
    }

    xeo3::SetTranslatedIar(*translated, sourceIar);
    if (useIntegerStateSync)
    {
        xeo3::CopyIntegerToXeO3(
            *translated,
            xeo3::CpuStateView(cpuState));
    }
    else
    {
        xeo3::CopyToXeO3(
            *translated,
            xeo3::CpuStateView(cpuState));
    }
    EmitEvent(eventKind, sourceIar, targetIar, detail);
    BridgeLastNestedHostFence = reinterpret_cast<std::uintptr_t>(hostFence);
    if (directHostTarget == nullptr)
    {
        XeO3CallMappedGuest(
            cpuState,
            guestMemory,
            targetIar,
            hostFence);
    }
    else
    {
        XeO3CallHostGuest(
            cpuState,
            guestMemory,
            targetIar,
            directHostTarget,
            hostFence);
    }
    if (useIntegerStateSync)
    {
        xeo3::CopyIntegerFromXeO3(
            xeo3::CpuStateView(cpuState),
            *translated);
    }
    else
    {
        xeo3::CopyFromXeO3(
            xeo3::CpuStateView(cpuState),
            *translated);
    }

    if (frame == nullptr)
    {
        context = recovered.ppc;
    }
    return useIntegerStateSync;
}

void ExecuteKernelMappedGuest(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t sourceIar,
    const std::uint32_t targetIar) noexcept
{
    ExecuteViaXeO3(
        context,
        guestMemory,
        sourceIar,
        targetIar,
        EventKind::KernelContinuation,
        targetIar,
        14,
        xeo3::kernel::NativeHostTarget(targetIar));
}

constexpr std::uint32_t kXamInputGetCapabilitiesThunk = 0x823D096C;
constexpr std::uint32_t kXamInputGetStateThunk = 0x823D097C;
constexpr std::uint32_t kXamInputSetStateThunk = 0x823D098C;
constexpr std::uint32_t kXamInputGetKeystrokeExThunk = 0x823D099C;
constexpr std::uint32_t kXInputAnyUserFlag = 1U << 30;
std::atomic<std::uint16_t> g_virtualPadPreviousButtons{0};

struct VirtualPadKeyMapping
{
    std::uint16_t button;
    std::uint16_t virtualKey;
};

constexpr std::array<VirtualPadKeyMapping, 14>
    kVirtualPadKeyMappings{{
        {XINPUT_GAMEPAD_START, VK_PAD_START},
        {XINPUT_GAMEPAD_A, VK_PAD_A},
        {XINPUT_GAMEPAD_B, VK_PAD_B},
        {XINPUT_GAMEPAD_X, VK_PAD_X},
        {XINPUT_GAMEPAD_Y, VK_PAD_Y},
        {XINPUT_GAMEPAD_DPAD_UP, VK_PAD_DPAD_UP},
        {XINPUT_GAMEPAD_DPAD_DOWN, VK_PAD_DPAD_DOWN},
        {XINPUT_GAMEPAD_DPAD_LEFT, VK_PAD_DPAD_LEFT},
        {XINPUT_GAMEPAD_DPAD_RIGHT, VK_PAD_DPAD_RIGHT},
        {XINPUT_GAMEPAD_BACK, VK_PAD_BACK},
        {XINPUT_GAMEPAD_LEFT_SHOULDER, VK_PAD_LSHOULDER},
        {XINPUT_GAMEPAD_RIGHT_SHOULDER, VK_PAD_RSHOULDER},
        {XINPUT_GAMEPAD_LEFT_THUMB, VK_PAD_LTHUMB_PRESS},
        {XINPUT_GAMEPAD_RIGHT_THUMB, VK_PAD_RTHUMB_PRESS},
    }};

bool GuestRangeIsValid(
    const std::uint32_t address,
    const std::size_t size) noexcept
{
    return address != 0 &&
           static_cast<std::uint64_t>(address) + size <=
               0x1'0000'0000ULL;
}

std::uint16_t ReadGuestU16(
    const std::uint8_t* const guestMemory,
    const std::uint32_t address) noexcept
{
    std::uint16_t raw = 0;
    std::memcpy(&raw, guestMemory + address, sizeof(raw));
    return _byteswap_ushort(raw);
}

std::uint32_t ReadGuestU32(
    const std::uint8_t* const guestMemory,
    const std::uint32_t address) noexcept
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, guestMemory + address, sizeof(raw));
    return _byteswap_ulong(raw);
}

constexpr std::uint32_t kTargetCriticalSection = 0x826A1918;
constexpr std::uint32_t kRtlEnterCriticalSectionThunk = 0x823D007C;
constexpr std::uint32_t kRtlLeaveCriticalSectionThunk = 0x823D008C;
constexpr std::uint32_t kRtlTryEnterCriticalSectionThunk = 0x823D00AC;
constexpr std::uint32_t kKeSetEventThunk = 0x823D056C;
constexpr std::size_t kKeSetEventImportIndex = 117;
constexpr std::uint32_t kKeReleaseSpinLockFromRaisedIrqlThunk = 0x823D04AC;
constexpr std::uint32_t kKeAcquireSpinLockAtRaisedIrqlThunk = 0x823D04DC;
constexpr std::uint32_t kKeTryToAcquireSpinLockAtRaisedIrqlThunk = 0x823D0BDC;
constexpr std::uint32_t kKeRaiseIrqlToDpcLevelThunk = 0x823D0BBC;
constexpr std::uint32_t kKfLowerIrqlThunk = 0x823D0BCC;
constexpr std::size_t kGuestAddressSpaceSize = 0x1'0000'0000ULL;

constexpr bool SupportsIntegerImportSync(const std::uint32_t thunk) noexcept
{
    switch (thunk)
    {
    case 0x823D049CU: // MmGetPhysicalAddress
    case 0x823D09FCU: // KeTlsGetValue
        return true;
    default:
        return false;
    }
}

// Synchronization and IRQL imports are deliberately excluded. A live AC6 run
// deadlocked on a permanently-owned guest spin lock after these imports used
// partial state synchronization. Their XeO3 implementations may reschedule or
// deliver work that mutates state outside the integer subset.
static_assert(!SupportsIntegerImportSync(kKeReleaseSpinLockFromRaisedIrqlThunk));
static_assert(!SupportsIntegerImportSync(kKeAcquireSpinLockAtRaisedIrqlThunk));
static_assert(!SupportsIntegerImportSync(kKeTryToAcquireSpinLockAtRaisedIrqlThunk));
static_assert(!SupportsIntegerImportSync(kKeRaiseIrqlToDpcLevelThunk));
static_assert(!SupportsIntegerImportSync(kKfLowerIrqlThunk));

struct CriticalSectionSnapshot
{
    std::uint64_t cpuState = 0;
    std::uint32_t r13 = 0;
    std::uint32_t currentThread = 0;
    std::int32_t lockCount = 0;
    std::uint32_t recursion = 0;
    std::uint32_t owner = 0;
};

bool IsTracedCriticalSectionImport(
    const std::uint32_t thunk,
    const std::uint32_t lockAddress) noexcept
{
    return BridgeEventTraceEnabled != 0 &&
        lockAddress == kTargetCriticalSection &&
        (thunk == kRtlEnterCriticalSectionThunk ||
         thunk == kRtlLeaveCriticalSectionThunk ||
         thunk == kRtlTryEnterCriticalSectionThunk);
}

CriticalSectionSnapshot CaptureCriticalSectionSnapshot(
    PPCContext& context,
    std::uint8_t* const guestMemory) noexcept
{
    CriticalSectionSnapshot snapshot{};
    const auto* const frame = FindActiveFrame(context, guestMemory);
    snapshot.cpuState = reinterpret_cast<std::uintptr_t>(
        frame == nullptr ? CurrentCpuStateAnchor() : frame->cpuState);
    snapshot.r13 = context.r13.u32;
    if (GuestRangeIsValid(snapshot.r13, 0x104))
    {
        snapshot.currentThread =
            ReadGuestU32(guestMemory, snapshot.r13 + 0x100);
    }
    snapshot.lockCount = static_cast<std::int32_t>(
        ReadGuestU32(guestMemory, kTargetCriticalSection + 0x10));
    snapshot.recursion =
        ReadGuestU32(guestMemory, kTargetCriticalSection + 0x14);
    snapshot.owner =
        ReadGuestU32(guestMemory, kTargetCriticalSection + 0x18);
    return snapshot;
}

std::uint32_t CriticalSectionOperation(
    const std::uint32_t thunk) noexcept
{
    if (thunk == kRtlEnterCriticalSectionThunk)
    {
        return 1;
    }
    if (thunk == kRtlLeaveCriticalSectionThunk)
    {
        return 2;
    }
    return 3;
}

void RecordCriticalSectionEvent(
    const std::uint32_t thunk,
    const std::uint32_t lockAddress,
    const std::uint32_t callerLr,
    const CriticalSectionSnapshot& before,
    const CriticalSectionSnapshot& after,
    const std::uint32_t result,
    const std::uint64_t failureStreak) noexcept
{
    const auto sequence =
        g_criticalSectionEventSequence.fetch_add(
            1,
            std::memory_order_relaxed) + 1;
    auto& event = BridgeCriticalSectionEvents[
        (sequence - 1) % std::size(BridgeCriticalSectionEvents)];
    event.sequence = 0;
    event.tickCount = GetTickCount64();
    event.cpuState = before.cpuState;
    event.failureStreak = failureStreak;
    event.hostThreadId = GetCurrentThreadId();
    event.operation = CriticalSectionOperation(thunk);
    event.thunk = thunk;
    event.lockAddress = lockAddress;
    event.callerLr = callerLr;
    event.r13 = before.r13;
    event.currentThread = before.currentThread;
    event.lockCountBefore = before.lockCount;
    event.recursionBefore = before.recursion;
    event.ownerBefore = before.owner;
    event.result = result;
    event.lockCountAfter = after.lockCount;
    event.recursionAfter = after.recursion;
    event.ownerAfter = after.owner;
    std::atomic_thread_fence(std::memory_order_release);
    event.sequence = sequence;
    BridgeCriticalSectionEventCount = sequence;
}

BridgeThreadImportState* FindThreadImportState(
    const std::uint32_t hostThreadId) noexcept
{
    constexpr auto stateCount = std::size(BridgeThreadImportStates);
    auto index = static_cast<std::size_t>(hostThreadId) % stateCount;
    for (std::size_t probe = 0; probe < stateCount; ++probe)
    {
        auto& state =
            BridgeThreadImportStates[(index + probe) % stateCount];
        if (state.hostThreadId == hostThreadId)
        {
            return &state;
        }
        if (state.hostThreadId == 0 &&
            InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(&state.hostThreadId),
                static_cast<LONG>(hostThreadId),
                0) == 0)
        {
            return &state;
        }
    }
    return nullptr;
}

BridgeThreadImportState* RecordThreadImportBefore(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk,
    const std::uint32_t target,
    const std::uint32_t callerLr) noexcept
{
    // This diagnostic snapshot writes 96 bytes and performs two globally
    // contended atomics per import. AC6 can issue hundreds of thousands of
    // imports per second, so keep it opt-in outside targeted captures.
    if (BridgeThreadImportTraceEnabled == 0)
    {
        return nullptr;
    }

    auto* const state = FindThreadImportState(GetCurrentThreadId());
    if (state == nullptr)
    {
        return nullptr;
    }

    const auto* const frame = FindActiveFrame(context, guestMemory);
    state->sequence = 0;
    state->importCount += 1;
    state->cpuState = reinterpret_cast<std::uintptr_t>(
        frame == nullptr ? CurrentCpuStateAnchor() : frame->cpuState);
    state->guestMemory = reinterpret_cast<std::uintptr_t>(guestMemory);
    state->lastGuestIar = frame == nullptr
        ? context.xeo3GuestIar
        : frame->translated->ppc.xeo3GuestIar;
    state->lastThunk = thunk;
    state->lastTarget = target;
    state->callerLr = callerLr;
    state->r1 = context.r1.u32;
    state->r3 = context.r3.u32;
    state->r4 = context.r4.u32;
    state->r5 = context.r5.u32;
    state->r6 = context.r6.u32;
    state->r7 = context.r7.u32;
    state->r8 = context.r8.u32;
    state->r9 = context.r9.u32;
    state->r10 = context.r10.u32;
    state->r13 = context.r13.u32;
    state->result = 0xFFFFFFFFU;
    std::atomic_thread_fence(std::memory_order_release);
    state->sequence =
        g_threadImportStateSequence.fetch_add(
            1,
            std::memory_order_relaxed) + 1;
    return state;
}

void RecordThreadImportAfter(
    BridgeThreadImportState* const state,
    const PPCContext& context) noexcept
{
    if (state == nullptr)
    {
        return;
    }
    state->result = context.r3.u32;
    std::atomic_thread_fence(std::memory_order_release);
    state->sequence =
        g_threadImportStateSequence.fetch_add(
            1,
            std::memory_order_relaxed) + 1;
}

void WriteGuestU8(
    std::uint8_t* const guestMemory,
    const std::uint32_t address,
    const std::uint8_t value) noexcept
{
    std::memcpy(guestMemory + address, &value, sizeof(value));
}

void WriteGuestU16(
    std::uint8_t* const guestMemory,
    const std::uint32_t address,
    const std::uint16_t value) noexcept
{
    const auto raw = _byteswap_ushort(value);
    std::memcpy(guestMemory + address, &raw, sizeof(raw));
}

void WriteGuestU32(
    std::uint8_t* const guestMemory,
    const std::uint32_t address,
    const std::uint32_t value) noexcept
{
    const auto raw = _byteswap_ulong(value);
    std::memcpy(guestMemory + address, &raw, sizeof(raw));
}

bool ResolveXInputUserIndex(
    const std::uint32_t requestedIndex,
    const std::uint32_t flags,
    std::uint32_t& resolvedIndex) noexcept
{
    if ((flags & 0xFF) != 0 &&
        (flags & XINPUT_FLAG_GAMEPAD) == 0)
    {
        return false;
    }

    if ((requestedIndex & 0xFF) == 0xFF ||
        (flags & kXInputAnyUserFlag) != 0)
    {
        resolvedIndex = 0;
        return true;
    }
    if (requestedIndex >= XUSER_MAX_COUNT)
    {
        return false;
    }
    resolvedIndex = requestedIndex;
    return true;
}

void WriteGuestGamepad(
    std::uint8_t* const guestMemory,
    const std::uint32_t address,
    const XINPUT_GAMEPAD& gamepad) noexcept
{
    WriteGuestU16(guestMemory, address, gamepad.wButtons);
    WriteGuestU8(guestMemory, address + 2, gamepad.bLeftTrigger);
    WriteGuestU8(guestMemory, address + 3, gamepad.bRightTrigger);
    WriteGuestU16(
        guestMemory,
        address + 4,
        static_cast<std::uint16_t>(gamepad.sThumbLX));
    WriteGuestU16(
        guestMemory,
        address + 6,
        static_cast<std::uint16_t>(gamepad.sThumbLY));
    WriteGuestU16(
        guestMemory,
        address + 8,
        static_cast<std::uint16_t>(gamepad.sThumbRX));
    WriteGuestU16(
        guestMemory,
        address + 10,
        static_cast<std::uint16_t>(gamepad.sThumbRY));
}

bool TryExecuteXInputImport(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk,
    const std::uint32_t argument3,
    const std::uint32_t argument4,
    const std::uint32_t argument5) noexcept
{
    if (guestMemory == nullptr)
    {
        return false;
    }
    if (thunk != kXamInputGetCapabilitiesThunk &&
        thunk != kXamInputGetStateThunk &&
        thunk != kXamInputSetStateThunk &&
        thunk != kXamInputGetKeystrokeExThunk)
    {
        return false;
    }

    if (thunk == kXamInputGetCapabilitiesThunk)
    {
        std::uint32_t userIndex = 0;
        if (!ResolveXInputUserIndex(
                argument3,
                argument4,
                userIndex))
        {
            return false;
        }

        if (userIndex != 0)
        {
            context.r3.u64 = ERROR_DEVICE_NOT_CONNECTED;
            BridgeVirtualPadPollCount =
                BridgeVirtualPadPollCount + 1;
            return true;
        }

        XINPUT_CAPABILITIES capabilities{};
        capabilities.Type = XINPUT_DEVTYPE_GAMEPAD;
        capabilities.SubType = XINPUT_DEVSUBTYPE_GAMEPAD;
        capabilities.Flags = XINPUT_CAPS_FFB_SUPPORTED;
        capabilities.Gamepad.wButtons = 0xF3FF;
        capabilities.Gamepad.bLeftTrigger = 0xFF;
        capabilities.Gamepad.bRightTrigger = 0xFF;
        capabilities.Gamepad.sThumbLX = INT16_MAX;
        capabilities.Gamepad.sThumbLY = INT16_MAX;
        capabilities.Gamepad.sThumbRX = INT16_MAX;
        capabilities.Gamepad.sThumbRY = INT16_MAX;
        capabilities.Vibration.wLeftMotorSpeed = UINT16_MAX;
        capabilities.Vibration.wRightMotorSpeed = UINT16_MAX;

        const auto output = argument5;
        if (!GuestRangeIsValid(output, 20))
        {
            return false;
        }
        WriteGuestU8(guestMemory, output, capabilities.Type);
        WriteGuestU8(guestMemory, output + 1, capabilities.SubType);
        WriteGuestU16(guestMemory, output + 2, capabilities.Flags);
        WriteGuestGamepad(guestMemory, output + 4, capabilities.Gamepad);
        WriteGuestU16(
            guestMemory,
            output + 16,
            capabilities.Vibration.wLeftMotorSpeed);
        WriteGuestU16(
            guestMemory,
            output + 18,
            capabilities.Vibration.wRightMotorSpeed);
        context.r3.u64 = ERROR_SUCCESS;
        BridgeVirtualPadPollCount = BridgeVirtualPadPollCount + 1;
        return true;
    }

    if (thunk == kXamInputGetStateThunk)
    {
        std::uint32_t userIndex = 0;
        if (!ResolveXInputUserIndex(
                argument3,
                argument4,
                userIndex))
        {
            return false;
        }

        if (userIndex != 0)
        {
            context.r3.u64 = ERROR_DEVICE_NOT_CONNECTED;
            BridgeVirtualPadPollCount =
                BridgeVirtualPadPollCount + 1;
            return true;
        }

        XINPUT_STATE state{};
        const auto result = XInputGetState(userIndex, &state);
        if (result != ERROR_SUCCESS)
        {
            return false;
        }

        const auto output = argument5;
        if (output != 0)
        {
            if (!GuestRangeIsValid(output, 16))
            {
                return false;
            }
            WriteGuestU32(guestMemory, output, state.dwPacketNumber);
            WriteGuestGamepad(guestMemory, output + 4, state.Gamepad);
        }
        context.r3.u64 = ERROR_SUCCESS;
        BridgeVirtualPadPollCount = BridgeVirtualPadPollCount + 1;
        BridgeVirtualPadLastButtons = state.Gamepad.wButtons;
        return true;
    }

    if (thunk == kXamInputSetStateThunk)
    {
        std::uint32_t userIndex = 0;
        if (!ResolveXInputUserIndex(argument3, 0, userIndex))
        {
            return false;
        }

        if (userIndex != 0)
        {
            context.r3.u64 = ERROR_DEVICE_NOT_CONNECTED;
            return true;
        }

        const auto input = argument5;
        if (!GuestRangeIsValid(input, 4))
        {
            return false;
        }
        XINPUT_VIBRATION vibration{};
        vibration.wLeftMotorSpeed =
            ReadGuestU16(guestMemory, input);
        vibration.wRightMotorSpeed =
            ReadGuestU16(guestMemory, input + 2);
        const auto result = XInputSetState(userIndex, &vibration);
        if (result != ERROR_SUCCESS)
        {
            return false;
        }
        context.r3.u64 = result;
        return true;
    }

    if (thunk == kXamInputGetKeystrokeExThunk)
    {
        const auto userIndexPointer = argument3;
        const auto output = argument5;
        if (!GuestRangeIsValid(userIndexPointer, 4) ||
            !GuestRangeIsValid(output, 8))
        {
            return false;
        }

        std::uint32_t userIndex = 0;
        if (!ResolveXInputUserIndex(
                ReadGuestU32(guestMemory, userIndexPointer),
                argument4,
                userIndex))
        {
            return false;
        }

        if (userIndex != 0)
        {
            context.r3.u64 = ERROR_DEVICE_NOT_CONNECTED;
            BridgeVirtualPadPollCount =
                BridgeVirtualPadPollCount + 1;
            return true;
        }

        XINPUT_STATE state{};
        if (XInputGetState(userIndex, &state) != ERROR_SUCCESS)
        {
            return false;
        }

        XINPUT_KEYSTROKE keystroke{};
        auto result = ERROR_EMPTY;
        const auto previousButtons =
            g_virtualPadPreviousButtons.exchange(
                state.Gamepad.wButtons,
                std::memory_order_acq_rel);
        const auto changedButtons =
            static_cast<std::uint16_t>(
                previousButtons ^ state.Gamepad.wButtons);
        for (const auto& mapping : kVirtualPadKeyMappings)
        {
            if ((changedButtons & mapping.button) == 0)
            {
                continue;
            }

            keystroke.VirtualKey = mapping.virtualKey;
            keystroke.Flags =
                (state.Gamepad.wButtons & mapping.button) != 0
                ? XINPUT_KEYSTROKE_KEYDOWN
                : XINPUT_KEYSTROKE_KEYUP;
            keystroke.UserIndex =
                static_cast<std::uint8_t>(userIndex);
            result = ERROR_SUCCESS;
            break;
        }
        if (result == ERROR_SUCCESS)
        {
            WriteGuestU32(guestMemory, userIndexPointer, userIndex);
            WriteGuestU16(
                guestMemory,
                output,
                keystroke.VirtualKey);
            WriteGuestU16(
                guestMemory,
                output + 2,
                keystroke.Unicode);
            WriteGuestU16(
                guestMemory,
                output + 4,
                keystroke.Flags);
            WriteGuestU8(
                guestMemory,
                output + 6,
                keystroke.UserIndex);
            WriteGuestU8(
                guestMemory,
                output + 7,
                keystroke.HidCode);
        }
        context.r3.u64 = result;
        BridgeVirtualPadPollCount = BridgeVirtualPadPollCount + 1;
        BridgeVirtualPadLastButtons = state.Gamepad.wButtons;
        return true;
    }

    return false;
}

constexpr std::uint32_t kNtReadFileThunk = 0x823D035C;
constexpr std::uint32_t kNtSetEventThunk = 0x823D015C;
constexpr std::uint32_t kNtWaitForSingleObjectExThunk = 0x823D020C;
constexpr std::uint32_t kNtReleaseSemaphoreThunk = 0x823D028C;
constexpr wchar_t kAc6DvdRoot[] = L"D:\\XeO3AC6DVD";

void RecordSynchronizationImport(
    const PPCContext& context,
    const std::uint32_t thunk,
    const std::uint32_t callerLr,
    const std::uint32_t argument3,
    const std::uint32_t argument4,
    const std::uint32_t argument5) noexcept
{
    if (thunk == kNtWaitForSingleObjectExThunk)
    {
        BridgeNtWaitCount = BridgeNtWaitCount + 1;
        BridgeNtWaitLastHandle = argument3;
        BridgeNtWaitLastWaitMode = argument4;
        BridgeNtWaitLastTimeout = argument5;
        BridgeNtWaitLastStatus = context.r3.u32;
        BridgeNtWaitLastCallerLr = callerLr;
    }
    else if (thunk == kNtReleaseSemaphoreThunk)
    {
        BridgeNtReleaseSemaphoreCount = BridgeNtReleaseSemaphoreCount + 1;
        BridgeNtReleaseSemaphoreLastHandle = argument3;
        BridgeNtReleaseSemaphoreLastReleaseCount = argument4;
        BridgeNtReleaseSemaphoreLastPreviousCount = argument5;
        BridgeNtReleaseSemaphoreLastStatus = context.r3.u32;
        BridgeNtReleaseSemaphoreLastCallerLr = callerLr;
    }
}

bool SignalSynchronousArchiveReadEvent(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t eventHandle) noexcept
{
    BridgeSynchronousArchiveReadLastPhase = 3;
    BridgeSynchronousArchiveReadLastSignalThreadId = GetCurrentThreadId();
    std::size_t importIndex = 0;
    while (PrecompiledImportTable[importIndex * 2] != 0 &&
           PrecompiledImportTable[importIndex * 2] != kNtSetEventThunk)
    {
        ++importIndex;
    }

    const auto target = PrecompiledImportTable[importIndex * 2 + 1];
    if (PrecompiledImportTable[importIndex * 2] != kNtSetEventThunk ||
        target == 0)
    {
        BridgeSynchronousArchiveSignalLastStatus = 0xC0000225U;
        BridgeSynchronousArchiveSignalFailureCount =
            BridgeSynchronousArchiveSignalFailureCount + 1;
        return false;
    }

    auto* const frame = FindActiveFrame(context, guestMemory);
    if (frame == nullptr)
    {
        BridgeSynchronousArchiveSignalLastStatus = 0xC0000001U;
        BridgeSynchronousArchiveSignalFailureCount =
            BridgeSynchronousArchiveSignalFailureCount + 1;
        return false;
    }

    const auto savedContext = context;
    const auto savedIar =
        frame->translated->ppc.xeo3GuestIar;
    context.r3.u64 = eventHandle;
    context.r4.u64 = 0;
    ExecuteViaXeO3(
        context,
        guestMemory,
        kNtSetEventThunk,
        target,
        EventKind::Import,
        static_cast<std::uint32_t>(importIndex),
        13);
    const auto status = context.r3.u32;

    context = savedContext;
    xeo3::SetTranslatedIar(*frame->translated, savedIar);
    xeo3::CopyToXeO3(
        *frame->translated,
        xeo3::CpuStateView(frame->cpuState));

    BridgeSynchronousArchiveSignalLastStatus = status;
    BridgeSynchronousArchiveReadLastPhase = 4;
    if (static_cast<std::int32_t>(status) < 0)
    {
        BridgeSynchronousArchiveSignalFailureCount =
            BridgeSynchronousArchiveSignalFailureCount + 1;
        return false;
    }

    BridgeSynchronousArchiveSignalCount =
        BridgeSynchronousArchiveSignalCount + 1;
    return true;
}

bool TryExecuteSynchronousArchiveRead(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk) noexcept
{
    if (thunk != kNtReadFileThunk ||
        context.r4.u32 == 0 ||
        context.r7.u32 == 0 ||
        context.r10.u32 == 0)
    {
        return false;
    }

    BridgeSynchronousArchiveReadAttemptCount =
        BridgeSynchronousArchiveReadAttemptCount + 1;
    BridgeSynchronousArchiveReadLastPhase = 1;
    BridgeSynchronousArchiveReadLastCallerIar =
        static_cast<std::uint32_t>(context.lr);
    BridgeSynchronousArchiveReadLastApcRoutine = context.r5.u32;
    BridgeSynchronousArchiveReadLastApcContext = context.r6.u32;
    BridgeSynchronousArchiveReadLastIoStatusBlock = context.r7.u32;
    BridgeSynchronousArchiveReadLastBuffer = context.r8.u32;
    BridgeSynchronousArchiveReadLastOffsetPointer = context.r10.u32;
    BridgeSynchronousArchiveReadLastThreadId = GetCurrentThreadId();

    const auto result =
        xeo3::ac6_io::TryReadDataArchiveSynchronously(
            guestMemory,
            context.r24.u32,
            context.r3.u32,
            context.r7.u32,
            context.r8.u32,
            context.r9.u32,
            context.r10.u32,
            kAc6DvdRoot);
    BridgeSynchronousArchiveReadLastGuestMemory =
        reinterpret_cast<std::uintptr_t>(guestMemory);
    BridgeSynchronousArchiveReadLastHostBuffer =
        reinterpret_cast<std::uintptr_t>(guestMemory) + context.r8.u32;
    if (result.disposition ==
        xeo3::ac6_io::ArchiveReadDisposition::NotApplicable)
    {
        BridgeSynchronousArchiveReadLastPhase = 0;
        return false;
    }

    BridgeSynchronousArchiveReadLastPhase = 2;

    BridgeSynchronousArchiveReadLastIndex = result.archiveIndex;
    BridgeSynchronousArchiveReadLastHandle = context.r3.u32;
    BridgeSynchronousArchiveReadLastOffset = result.byteOffset;
    BridgeSynchronousArchiveReadLastLength = result.requestedBytes;
    BridgeSynchronousArchiveReadLastCompleted =
        result.completedBytes;
    BridgeSynchronousArchiveReadLastError = result.win32Error;
    BridgeSynchronousArchiveReadLastEvent = context.r4.u32;
    if (GuestRangeIsValid(context.r7.u32, 8))
    {
        BridgeSynchronousArchiveReadLastIoStatus =
            ReadGuestU32(guestMemory, context.r7.u32);
        BridgeSynchronousArchiveReadLastIoInformation =
            ReadGuestU32(guestMemory, context.r7.u32 + 4);
    }

    if (result.disposition ==
        xeo3::ac6_io::ArchiveReadDisposition::Failed)
    {
        BridgeSynchronousArchiveReadLastPhase = 5;
        BridgeSynchronousArchiveReadLastFailureGuestMemory =
            reinterpret_cast<std::uintptr_t>(guestMemory);
        BridgeSynchronousArchiveReadLastFailureHostBuffer =
            reinterpret_cast<std::uintptr_t>(guestMemory) + context.r8.u32;
        BridgeSynchronousArchiveReadLastFailureLength = context.r9.u32;
        BridgeSynchronousArchiveReadFailureCount =
            BridgeSynchronousArchiveReadFailureCount + 1;
        EmitEvent(
            EventKind::ArchiveRead,
            thunk,
            context.r8.u32,
            0x80000000U | result.win32Error);
        return false;
    }

    if (!SignalSynchronousArchiveReadEvent(
            context,
            guestMemory,
            context.r4.u32))
    {
        BridgeSynchronousArchiveReadLastPhase = 6;
        return false;
    }

    BridgeSynchronousArchiveReadCount =
        BridgeSynchronousArchiveReadCount + 1;
    EmitEvent(
        EventKind::ArchiveRead,
        thunk,
        context.r8.u32,
        result.archiveIndex);
    context.r3.u64 = 0;
    BridgeSynchronousArchiveReadLastPhase = 7;
    return true;
}

bool TryExecuteFastCriticalSection(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk) noexcept
{
    if (BridgeFastCriticalSectionsEnabled == 0 ||
        (thunk != kRtlEnterCriticalSectionThunk &&
         thunk != kRtlLeaveCriticalSectionThunk &&
         thunk != kRtlTryEnterCriticalSectionThunk))
    {
        return false;
    }

    const auto trace = BridgeFastSynchronizationTelemetryEnabled != 0 ||
        BridgeThreadImportTraceEnabled != 0;
    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastCriticalSectionAttemptCount));
    }
    const auto criticalSectionAddress = context.r3.u32;
    std::uint32_t currentThread = 0;
    if (!xeo3::fast_sync::ReadCurrentThread(
            guestMemory,
            kGuestAddressSpaceSize,
            context.r13.u32,
            currentThread))
    {
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastCriticalSectionFallbackCount));
        }
        return false;
    }

    ActiveFrame* signalFrame = nullptr;
    std::uint32_t signalTarget = 0;
    if (thunk == kRtlLeaveCriticalSectionThunk)
    {
        if (PrecompiledImportTable[kKeSetEventImportIndex * 2] !=
                kKeSetEventThunk ||
            (signalTarget = PrecompiledImportTable[
                kKeSetEventImportIndex * 2 + 1]) == 0 ||
            (signalFrame = FindActiveFrame(context, guestMemory)) == nullptr)
        {
            if (trace)
            {
                InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                    &BridgeFastCriticalSectionFallbackCount));
            }
            return false;
        }
    }

    auto result = xeo3::fast_sync::CriticalSectionResult{};
    if (thunk == kRtlLeaveCriticalSectionThunk)
    {
        result = xeo3::fast_sync::LeaveCriticalSection(
            guestMemory,
            kGuestAddressSpaceSize,
            criticalSectionAddress,
            currentThread);
    }
    else
    {
        result = xeo3::fast_sync::EnterCriticalSection(
            guestMemory,
            kGuestAddressSpaceSize,
            criticalSectionAddress,
            currentThread,
            thunk == kRtlTryEnterCriticalSectionThunk
                ? xeo3::fast_sync::EnterMode::tryOnly
                : xeo3::fast_sync::EnterMode::wait);
    }

    if (result.disposition == xeo3::fast_sync::Disposition::fallback)
    {
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastCriticalSectionFallbackCount));
        }
        return false;
    }

    if (thunk == kRtlTryEnterCriticalSectionThunk)
    {
        context.r3.u64 = result.returnValue;
    }
    if (result.disposition ==
        xeo3::fast_sync::Disposition::completedAndSignal)
    {
        const auto savedContext = context;
        const auto savedIar =
            signalFrame->translated->ppc.xeo3GuestIar;
        context.r3.u64 = criticalSectionAddress;
        context.r4.u64 = 1;
        context.r5.u64 = 0;
        ExecuteViaXeO3(
            context,
            guestMemory,
            kKeSetEventThunk,
            signalTarget,
            EventKind::Import,
            static_cast<std::uint32_t>(kKeSetEventImportIndex),
            5);
        context = savedContext;
        xeo3::SetTranslatedIar(*signalFrame->translated, savedIar);
        xeo3::CopyToXeO3(
            *signalFrame->translated,
            xeo3::CpuStateView(signalFrame->cpuState));
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastCriticalSectionSignalCount));
        }
    }

    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastCriticalSectionHitCount));
        BridgeFastCriticalSectionLastThunk = thunk;
        BridgeFastCriticalSectionLastAddress = criticalSectionAddress;
        BridgeFastCriticalSectionLastThread = currentThread;
        BridgeFastCriticalSectionLastDisposition =
            static_cast<std::uint32_t>(result.disposition);
    }
    return true;
}

bool TryExecuteFastSpinLock(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk) noexcept
{
    if (BridgeFastSpinLocksEnabled == 0 ||
        (thunk != kKeAcquireSpinLockAtRaisedIrqlThunk &&
         thunk != kKeTryToAcquireSpinLockAtRaisedIrqlThunk &&
         thunk != kKeReleaseSpinLockFromRaisedIrqlThunk))
    {
        return false;
    }

    const auto trace = BridgeFastSynchronizationTelemetryEnabled != 0 ||
        BridgeThreadImportTraceEnabled != 0;
    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastSpinLockAttemptCount));
    }
    const auto spinLockAddress = context.r3.u32;
    auto result = xeo3::fast_spin::SpinLockResult{};
    if (thunk == kKeReleaseSpinLockFromRaisedIrqlThunk)
    {
        result = xeo3::fast_spin::ReleaseSpinLock(
            guestMemory,
            kGuestAddressSpaceSize,
            spinLockAddress);
    }
    else
    {
        result = xeo3::fast_spin::AcquireSpinLock(
            guestMemory,
            kGuestAddressSpaceSize,
            spinLockAddress,
            thunk == kKeTryToAcquireSpinLockAtRaisedIrqlThunk
                ? xeo3::fast_spin::AcquireMode::tryOnly
                : xeo3::fast_spin::AcquireMode::wait);
    }

    if (trace && result.contended)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastSpinLockContentionCount));
    }
    if (result.disposition == xeo3::fast_spin::Disposition::fallback)
    {
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastSpinLockFallbackCount));
        }
        return false;
    }

    if (thunk == kKeTryToAcquireSpinLockAtRaisedIrqlThunk)
    {
        context.r3.u64 = result.returnValue;
    }
    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastSpinLockHitCount));
        BridgeFastSpinLockLastThunk = thunk;
        BridgeFastSpinLockLastAddress = spinLockAddress;
    }
    return true;
}

bool TryExecuteFastIrql(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t thunk) noexcept
{
    if (BridgeFastIrqlEnabled == 0 ||
        (thunk != kKeRaiseIrqlToDpcLevelThunk &&
         thunk != kKfLowerIrqlThunk))
    {
        return false;
    }

    const auto trace = BridgeFastIrqlTelemetryEnabled != 0;
    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastIrqlAttemptCount));
        BridgeFastIrqlLastThunk = thunk;
    }

    if (thunk == kKeRaiseIrqlToDpcLevelThunk)
    {
        const auto result = xeo3::fast_irql::RaiseToDpc(
            guestMemory,
            kGuestAddressSpaceSize,
            context.r13.u32);
        if (result.disposition !=
            xeo3::fast_irql::Disposition::completed)
        {
            if (trace)
            {
                InterlockedIncrement64(
                    reinterpret_cast<volatile LONG64*>(
                        &BridgeFastIrqlInvalidFallbackCount));
            }
            return false;
        }

        context.r3.u64 = result.oldIrql;
        if (trace)
        {
            BridgeFastIrqlLastOldIrql = result.oldIrql;
            BridgeFastIrqlLastNewIrql =
                xeo3::fast_irql::kDispatchLevel;
            BridgeFastIrqlLastPendingIrql = 0;
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastIrqlHitCount));
        }
        return true;
    }

    const auto newIrql = context.r3.u32;
    const auto result = xeo3::fast_irql::Lower(
        guestMemory,
        kGuestAddressSpaceSize,
        context.r13.u32,
        newIrql);
    if (trace)
    {
        BridgeFastIrqlLastNewIrql = newIrql;
        BridgeFastIrqlLastPendingIrql = result.pendingIrql;
    }
    if (result.disposition ==
        xeo3::fast_irql::Disposition::nativeCheckRequired)
    {
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastIrqlNativeFallbackCount));
        }
        return false;
    }
    if (result.disposition != xeo3::fast_irql::Disposition::completed)
    {
        if (trace)
        {
            InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
                &BridgeFastIrqlInvalidFallbackCount));
        }
        return false;
    }

    if (trace)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeFastIrqlHitCount));
    }
    return true;
}

void ExecuteImport(
    PPCContext& context,
    std::uint8_t* guestMemory,
    const std::size_t index,
    const std::uint32_t thunk)
{
    const auto target = PrecompiledImportTable[index * 2 + 1];
    const auto argument3 = context.r3.u32;
    const auto argument4 = context.r4.u32;
    const auto argument5 = context.r5.u32;
    const auto callerLr = context.lr;
    const auto traceCriticalSection =
        IsTracedCriticalSectionImport(thunk, argument3);
    CriticalSectionSnapshot criticalSectionBefore{};
    if (traceCriticalSection)
    {
        criticalSectionBefore =
            CaptureCriticalSectionSnapshot(context, guestMemory);
    }
    BridgeImportCount = BridgeImportCount + 1;
    if (BridgeImportCountersEnabled != 0 && index < kAc6ImportCount)
    {
        InterlockedIncrement64(
            reinterpret_cast<volatile LONG64*>(&BridgeImportCounts[index]));
    }
    BridgeLastImportThunk = thunk;
    BridgeLastImportTarget = target;
    auto* const threadImportState = RecordThreadImportBefore(
        context,
        guestMemory,
        thunk,
        target,
        callerLr);
    if (target == 0)
    {
        Fail(thunk, target, 3);
    }

    if (TryExecuteSynchronousArchiveRead(
            context,
            guestMemory,
            thunk))
    {
        RecordThreadImportAfter(threadImportState, context);
        return;
    }

    if (TryExecuteFastCriticalSection(context, guestMemory, thunk))
    {
        RecordThreadImportAfter(threadImportState, context);
        return;
    }

    if (TryExecuteFastSpinLock(context, guestMemory, thunk))
    {
        RecordThreadImportAfter(threadImportState, context);
        return;
    }

    if (TryExecuteFastIrql(context, guestMemory, thunk))
    {
        RecordThreadImportAfter(threadImportState, context);
        return;
    }

    const auto requestIntegerSync =
        BridgeIntegerImportSyncEnabled != 0 &&
        SupportsIntegerImportSync(thunk);
    if (requestIntegerSync)
    {
        BridgeIntegerImportSyncAttemptCount =
            BridgeIntegerImportSyncAttemptCount + 1;
    }
    const auto usedIntegerSync = ExecuteViaXeO3(
        context,
        guestMemory,
        thunk,
        target,
        EventKind::Import,
        static_cast<std::uint32_t>(index),
        5,
        nullptr,
        requestIntegerSync
            ? NestedStateSync::integerOnly
            : NestedStateSync::full);
    if (requestIntegerSync)
    {
        if (usedIntegerSync)
        {
            BridgeIntegerImportSyncHitCount =
                BridgeIntegerImportSyncHitCount + 1;
            BridgeIntegerImportSyncLastThunk = thunk;
        }
        else
        {
            BridgeIntegerImportSyncFallbackCount =
                BridgeIntegerImportSyncFallbackCount + 1;
        }
    }
    if (thunk == kKeRaiseIrqlToDpcLevelThunk)
    {
        auto* counter = &BridgeRaiseIrqlUnexpectedCount;
        switch (context.r3.u32)
        {
        case 0:
            counter = &BridgeRaiseIrqlOldPassiveCount;
            break;
        case 1:
            counter = &BridgeRaiseIrqlOldApcCount;
            break;
        case 2:
            counter = &BridgeRaiseIrqlOldDispatchCount;
            break;
        default:
            break;
        }
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(counter));
    }
    RecordSynchronizationImport(
        context,
        thunk,
        callerLr,
        argument3,
        argument4,
        argument5);
    if (traceCriticalSection)
    {
        const auto result = context.r3.u32;
        const auto criticalSectionAfter =
            CaptureCriticalSectionSnapshot(context, guestMemory);
        auto failureStreak = std::uint64_t{0};
        auto record = true;
        if (thunk == kRtlTryEnterCriticalSectionThunk)
        {
            if (result == 0)
            {
                failureStreak = ++g_targetLockTryFailureStreak;
                BridgeRtlTryLastFailureStreak = failureStreak;
                record = failureStreak <= 64 ||
                    (failureStreak & (failureStreak - 1)) == 0;
            }
            else
            {
                g_targetLockTryFailureStreak = 0;
                BridgeRtlTryLastFailureStreak = 0;
            }
        }
        if (record)
        {
            RecordCriticalSectionEvent(
                thunk,
                argument3,
                callerLr,
                criticalSectionBefore,
                criticalSectionAfter,
                result,
                failureStreak);
        }
    }
    TryExecuteXInputImport(
        context,
        guestMemory,
        thunk,
        argument3,
        argument4,
        argument5);
    RecordThreadImportAfter(threadImportState, context);
}

std::uint32_t NegotiateVersion(const std::uint32_t requestedVersion) noexcept
{
    if (requestedVersion >= xeo3::kMinimumAbiVersion &&
        requestedVersion <= xeo3::kCurrentAbiVersion)
    {
        return requestedVersion;
    }
    return xeo3::kCurrentAbiVersion;
}

bool BuildMappings() noexcept
{
    constexpr auto kFunctionLookupEntryCount =
        (static_cast<std::size_t>(kImageSize) + 3U) / 4U;
    g_functionCount = 0;
    g_functionLookup.clear();
    BridgeFunctionLookupEntryCount = 0;
    while (PPCFuncMappings[g_functionCount].guest != 0)
    {
        const auto& mapping = PPCFuncMappings[g_functionCount];
        if (mapping.host == nullptr ||
            mapping.guest < kImageBase ||
            mapping.guest >=
                static_cast<std::uint64_t>(kImageBase) + kImageSize ||
            ((mapping.guest - kImageBase) & 3U) != 0)
        {
            return false;
        }
        if (g_functionCount != 0 &&
            PPCFuncMappings[g_functionCount - 1].guest >= mapping.guest)
        {
            return false;
        }
        ++g_functionCount;
    }

    const auto moduleBase =
        reinterpret_cast<std::uintptr_t>(&__ImageBase);
    const auto thunkAddress =
        reinterpret_cast<std::uintptr_t>(&XeO3AotThunk);
    const auto thunkRva = thunkAddress - moduleBase;
    if (thunkRva > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    try
    {
        g_mappings.resize(g_functionCount);
        g_functionLookup.assign(kFunctionLookupEntryCount, nullptr);
    }
    catch (...)
    {
        g_mappings.clear();
        g_functionLookup.clear();
        return false;
    }

    for (std::size_t index = 0; index < g_functionCount; ++index)
    {
        g_mappings[index] = {
            static_cast<std::uint32_t>(
                PPCFuncMappings[index].guest - kImageBase),
            static_cast<std::uint32_t>(thunkRva),
        };
        const auto lookupIndex = static_cast<std::size_t>(
            (PPCFuncMappings[index].guest - kImageBase) >> 2U);
        if (g_functionLookup[lookupIndex] != nullptr)
        {
            g_mappings.clear();
            g_functionLookup.clear();
            return false;
        }
        g_functionLookup[lookupIndex] = PPCFuncMappings[index].host;
    }
    BridgeFunctionLookupEntryCount =
        static_cast<std::uint32_t>(g_functionLookup.size());
    return true;
}
}

namespace xeo3
{
bool PublishActiveState(
    PPCContext& context,
    std::uint8_t* const guestMemory) noexcept
{
    auto* const frame = FindActiveFrame(context, guestMemory);
    if (frame == nullptr ||
        frame->cpuState == nullptr ||
        frame->translated == nullptr)
    {
        return false;
    }

    SetTranslatedIar(*frame->translated, context.xeo3GuestIar);
    CopyToXeO3(
        *frame->translated,
        CpuStateView(frame->cpuState));
    return true;
}

void PublishStackPointer(
    PPCContext& context,
    std::uint8_t* guestMemory) noexcept
{
    auto* const frame = FindActiveFrame(context, guestMemory);
    if (frame == nullptr || frame->cpuState == nullptr)
    {
        return;
    }

    PublishStackPointerToXeO3(
        context,
        CpuStateView(frame->cpuState));
}

void CallIndirect(
    PPCContext& context,
    std::uint8_t* guestMemory,
    const std::uint32_t guestAddress)
{
    if (BridgeIndirectStateSyncEnabled != 0 &&
        context.xeo3IndirectCallsUntilSync <= 1U)
    {
        context.xeo3IndirectCallsUntilSync =
            kIndirectStateSyncInterval;
        static_cast<void>(PublishActiveState(context, guestMemory));
    }
    else if (BridgeIndirectStateSyncEnabled != 0)
    {
        --context.xeo3IndirectCallsUntilSync;
    }
    const auto callerIar = context.xeo3GuestIar;
    if (BridgeIndirectTelemetryEnabled != 0)
    {
        InterlockedIncrement64(reinterpret_cast<volatile LONG64*>(
            &BridgeIndirectCallCount));
        BridgeLastIndirectTarget = guestAddress;
        BridgeLastIndirectCallerIar = callerIar;
    }
    if (guestAddress == 0x8237FF40U)
    {
        const auto object = context.r3.u32;
        const auto vtable = GuestRangeIsValid(object, sizeof(std::uint32_t))
            ? ReadGuestU32(guestMemory, object)
            : 0;
        BridgeFatalIndirectCount = BridgeFatalIndirectCount + 1;
        BridgeLastFatalIndirectTarget = guestAddress;
        BridgeLastFatalIndirectCallerIar = callerIar;
        BridgeLastFatalIndirectObject = object;
        BridgeLastFatalIndirectVtable = vtable;
        BridgeLastFatalIndirectSlot0 =
            GuestRangeIsValid(vtable, sizeof(std::uint32_t) * 2U)
                ? ReadGuestU32(guestMemory, vtable)
                : 0;
        BridgeLastFatalIndirectSlot1 =
            GuestRangeIsValid(vtable, sizeof(std::uint32_t) * 2U)
                ? ReadGuestU32(guestMemory, vtable + 4U)
                : 0;
        BridgeLastFatalIndirectArgument4 = context.r4.u64;
        BridgeLastFatalIndirectThreadId = GetCurrentThreadId();
    }
    const auto workerPrimary = callerIar == 0x823464BCU;
    const auto workerDestroy = callerIar == 0x823464DCU;
    if (!workerPrimary && !workerDestroy)
    {
        if (auto* const function = FindFunction(guestAddress);
            function != nullptr)
        {
            function(context, guestMemory);
            return;
        }

        ExecuteViaXeO3(
            context,
            guestMemory,
            guestAddress,
            guestAddress,
            EventKind::IndirectFallback,
            0,
            2);
        return;
    }

    BridgeWorkerCallbackTargetCount =
        BridgeWorkerCallbackTargetCount + 1;
    BridgeLastWorkerCallbackTarget = guestAddress;
    BridgeLastWorkerCallbackCallerIar = callerIar;
    const auto workerObject = context.r3.u32;
    const auto workerThreadId = GetCurrentThreadId();
    bool ownsActiveWorkerPrimary = false;
    int workerPrimarySlot = -1;
    if (workerPrimary)
    {
        std::uint64_t storedPayload = 0;
        std::memcpy(
            &storedPayload,
            GuestMemoryPointer(guestMemory, workerObject + 8),
            sizeof(storedPayload));
        const auto payload = _byteswap_uint64(storedPayload);
        BridgeLastWorkerPrimaryTarget = guestAddress;
        BridgeLastWorkerPrimaryObject = workerObject;
        BridgeLastWorkerPrimaryPayload = payload;
        BridgeLastWorkerPrimaryThreadId = workerThreadId;
        workerPrimarySlot = ClaimWorkerPrimarySlot(
            workerThreadId,
            guestAddress,
            workerObject,
            payload);
        if (InterlockedCompareExchange(
                reinterpret_cast<volatile LONG*>(
                    &BridgeActiveWorkerPrimaryThreadId),
                static_cast<LONG>(workerThreadId),
                0) == 0)
        {
            BridgeActiveWorkerPrimaryTarget = guestAddress;
            BridgeActiveWorkerPrimaryObject = workerObject;
            BridgeActiveWorkerPrimaryPayload = payload;
            BridgeActiveWorkerPrimaryClaimCount =
                BridgeActiveWorkerPrimaryClaimCount + 1;
            ownsActiveWorkerPrimary = true;
        }
    }
    else if (workerDestroy)
    {
        BridgeLastWorkerDestroyTarget = guestAddress;
        BridgeLastWorkerDestroyObject = workerObject;
    }
    if (auto* const function = FindFunction(guestAddress);
        function != nullptr)
    {
        function(context, guestMemory);
        if (ownsActiveWorkerPrimary)
        {
            BridgeActiveWorkerPrimaryTarget = 0;
            BridgeActiveWorkerPrimaryObject = 0;
            BridgeActiveWorkerPrimaryPayload = 0;
            InterlockedExchange(
                reinterpret_cast<volatile LONG*>(
                    &BridgeActiveWorkerPrimaryThreadId),
                0);
        }
        ReleaseWorkerPrimarySlot(workerPrimarySlot, workerThreadId);
        return;
    }

    ExecuteViaXeO3(
        context,
        guestMemory,
        guestAddress,
        guestAddress,
        EventKind::IndirectFallback,
        0,
        2);
    if (ownsActiveWorkerPrimary)
    {
        BridgeActiveWorkerPrimaryTarget = 0;
        BridgeActiveWorkerPrimaryObject = 0;
        BridgeActiveWorkerPrimaryPayload = 0;
        InterlockedExchange(
            reinterpret_cast<volatile LONG*>(
                &BridgeActiveWorkerPrimaryThreadId),
            0);
    }
    ReleaseWorkerPrimarySlot(workerPrimarySlot, workerThreadId);
}

std::uint32_t MmioLoad32(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress)
{
    if (!IsXeO3MmioAddress(guestAddress))
    {
        std::uint32_t storedValue = 0;
        std::memcpy(
            &storedValue,
            guestMemory + guestAddress,
            sizeof(storedValue));
        return _byteswap_ulong(storedValue);
    }

    const auto callback =
        reinterpret_cast<MmioRead32>(PrecompiledPointers[8]);
    if (callback == nullptr)
    {
        Fail(guestAddress, 0, 6);
    }
    static_cast<void>(PublishActiveState(context, guestMemory));
    const auto value = callback(guestAddress);
    EmitEvent(EventKind::MmioRead, guestAddress, value, 4);
    return SwapMmioWord(value);
}

void MmioStore32(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress,
    const std::uint32_t value)
{
    if (!IsXeO3MmioAddress(guestAddress))
    {
        const auto storedValue = _byteswap_ulong(value);
        std::memcpy(
            guestMemory + guestAddress,
            &storedValue,
            sizeof(storedValue));
        return;
    }

    const auto callback =
        reinterpret_cast<MmioWrite32>(PrecompiledPointers[9]);
    if (callback == nullptr)
    {
        Fail(guestAddress, value, 7);
    }
    static_cast<void>(PublishActiveState(context, guestMemory));
    EmitEvent(EventKind::MmioWrite, guestAddress, value, 4);
    callback(guestAddress, SwapMmioWord(value));
}

std::uint64_t ReadTimeBase(
    PPCContext&,
    std::uint8_t*)
{
    const auto callback =
        reinterpret_cast<ReadGuestTimeBase>(PrecompiledPointers[4]);
    if (callback == nullptr)
    {
        Fail(BridgeLastGuestIar, 4, 10);
    }
    return callback();
}
}

namespace
{
void RecordCodeLengthEvent(
    const CodeLengthEventKind kind,
    const std::uint32_t stackPointer,
    const std::uint32_t index,
    const std::uint32_t symbol,
    const std::uint32_t totalCount,
    const std::uint64_t valueA,
    const std::uint64_t valueB,
    const std::uint64_t valueC) noexcept
{
    const auto sequence = g_codeLengthEvents.nextSequence++;
    auto& event =
        g_codeLengthEvents.events[
            sequence % g_codeLengthEvents.events.size()];
    event = {
        sequence,
        kind,
        stackPointer,
        index,
        symbol,
        totalCount,
        valueA,
        valueB,
        valueC,
    };
}

void CaptureCodeLengthPrefixMismatch(
    const char* const phase,
    const std::uint32_t stackPointer,
    const std::uint32_t prefixCount,
    const std::uint32_t endIndex,
    const std::uint32_t symbol,
    const std::uint32_t totalCount) noexcept
{
    if (!g_codeLengthTokenTrace.expectedArmed ||
        stackPointer !=
            g_codeLengthTokenTrace.expectedStackPointer ||
        prefixCount != g_codeLengthTokenTrace.expectedCount ||
        prefixCount > g_codeLengthTokenTrace.expectedValues.size())
    {
        return;
    }

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    constexpr std::uint32_t kCodeLengthsStackOffset = 112;
    std::uint32_t mismatchIndex =
        std::numeric_limits<std::uint32_t>::max();
    std::uint16_t mismatchExpected = 0;
    std::uint16_t mismatchActual = 0;
    for (std::uint32_t index = 0; index < prefixCount; ++index)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + stackPointer +
                kCodeLengthsStackOffset +
                index * sizeof(raw),
            sizeof(raw));
        const auto actual = _byteswap_ushort(raw);
        const auto expected =
            g_codeLengthTokenTrace.expectedValues[index];
        if (actual != expected)
        {
            mismatchIndex = index;
            mismatchExpected = expected;
            mismatchActual = actual;
            break;
        }
    }
    if (mismatchIndex ==
            std::numeric_limits<std::uint32_t>::max() ||
        g_codeLengthPrefixCaptured.exchange(
            true,
            std::memory_order_acq_rel))
    {
        return;
    }

    std::array<char, 131072> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "phase=%s thread=%lu module=%p guest_base=%p "
        "stack=%08X prefix_count=%u end_index=%u symbol=%u "
        "total_count=%u mismatch_index=%u expected=%04X "
        "actual=%04X event_sequence=%" PRIu64 "\nexpected_prefix:",
        phase,
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        stackPointer,
        prefixCount,
        endIndex,
        symbol,
        totalCount,
        mismatchIndex,
        mismatchExpected,
        mismatchActual,
        g_codeLengthEvents.nextSequence);
    for (std::uint32_t index = 0; index < prefixCount; ++index)
    {
        append(
            " %u:%04X",
            index,
            static_cast<unsigned>(
                g_codeLengthTokenTrace.expectedValues[index]));
    }

    append("%s", "\nactual_prefix:");
    for (std::uint32_t index = 0; index < prefixCount; ++index)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + stackPointer +
                kCodeLengthsStackOffset +
                index * sizeof(raw),
            sizeof(raw));
        append(
            " %u:%04X",
            index,
            static_cast<unsigned>(_byteswap_ushort(raw)));
    }

    append("%s", "\ncode_length_events:");
    const auto firstSequence =
        g_codeLengthEvents.nextSequence >
            g_codeLengthEvents.events.size()
        ? g_codeLengthEvents.nextSequence -
            g_codeLengthEvents.events.size()
        : 0;
    for (auto sequence = firstSequence;
         sequence < g_codeLengthEvents.nextSequence;
         ++sequence)
    {
        const auto& event =
            g_codeLengthEvents.events[
                sequence % g_codeLengthEvents.events.size()];
        if (event.sequence != sequence ||
            event.stackPointer != stackPointer)
        {
            continue;
        }
        append(
            " %c%" PRIu64 ":%u:%u:%u:%016llX:%016llX:%016llX",
            event.kind == CodeLengthEventKind::Refill ? 'R' : 'E',
            event.sequence,
            event.index,
            event.symbol,
            event.totalCount,
            static_cast<unsigned long long>(event.valueA),
            static_cast<unsigned long long>(event.valueB),
            static_cast<unsigned long long>(event.valueC));
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] =
        L".code-length-prefix-overwrite.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}
}

void Ac6TraceHuffmanTableInput(
    PPCRegister& ownerRegister,
    PPCRegister& countRegister,
    PPCRegister& rootBitsRegister)
{
    const auto count = countRegister.u32 & 0xFFFF;
    if (count == 0)
    {
        return;
    }

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    constexpr std::uint64_t kCodeLengthsOffset = 0x2AC;
    const auto owner = ownerRegister.u32;
    const auto inputEnd =
        static_cast<std::uint64_t>(owner) +
        kCodeLengthsOffset +
        static_cast<std::uint64_t>(count) * sizeof(std::uint16_t);
    if (inputEnd > 0x1'0000'0000ULL)
    {
        return;
    }

    std::uint32_t invalidIndex = 0;
    std::uint16_t invalidValue = 0;
    std::uint16_t invalidRaw = 0;
    for (std::uint32_t index = 0; index < count; ++index)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + owner + kCodeLengthsOffset +
                index * sizeof(raw),
            sizeof(raw));
        const auto value = _byteswap_ushort(raw);
        if (value > 15)
        {
            invalidIndex = index;
            invalidValue = value;
            invalidRaw = raw;
            break;
        }
    }
    if (invalidValue == 0 ||
        g_huffmanInputCaptured.exchange(
            true,
            std::memory_order_acq_rel))
    {
        return;
    }

    std::array<void*, 32> stack{};
    const auto stackCount = CaptureStackBackTrace(
        0,
        static_cast<DWORD>(stack.size()),
        stack.data(),
        nullptr);

    std::array<char, 8192> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "thread=%lu module=%p guest_base=%p owner=%08X count=%u "
        "root_bits=%08X invalid_index=%u invalid_value=%04X "
        "invalid_raw_little_endian=%04X\nlengths:",
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        owner,
        count,
        rootBitsRegister.u32,
        invalidIndex,
        invalidValue,
        invalidRaw);
    const auto firstLength =
        invalidIndex > 16 ? invalidIndex - 16 : 0;
    const auto lastLength =
        std::min<std::uint32_t>(count, invalidIndex + 17);
    for (auto index = firstLength; index < lastLength; ++index)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + owner + kCodeLengthsOffset +
                index * sizeof(raw),
            sizeof(raw));
        append(
            " %u:%04X",
            index,
            static_cast<unsigned>(_byteswap_ushort(raw)));
    }
    append("%s", "\nstack:");
    for (USHORT index = 0; index < stackCount; ++index)
    {
        append(" %p", stack[index]);
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] = L".huffman-invalid.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}

void Ac6TraceCodeLengthTokenBegin(
    PPCRegister& stackPointerRegister,
    PPCRegister& indexRegister,
    PPCRegister& inputIndexPointerRegister,
    PPCRegister& ownerRegister)
{
    const auto stackPointer = stackPointerRegister.u32;
    const auto startIndex = indexRegister.u32 & 0xFFFF;
    if (startIndex == 0)
    {
        g_codeLengthTokenTrace.expectedArmed = true;
        g_codeLengthTokenTrace.expectedStackPointer = stackPointer;
        g_codeLengthTokenTrace.expectedCount = 0;
        g_codeLengthTokenTrace.expectedValues.fill(0);
    }
    else
    {
        CaptureCodeLengthPrefixMismatch(
            "begin",
            stackPointer,
            startIndex,
            startIndex,
            std::numeric_limits<std::uint32_t>::max(),
            0);
    }

    g_codeLengthTokenTrace.stackPointer = stackPointer;
    g_codeLengthTokenTrace.startIndex = startIndex;
    g_codeLengthTokenTrace.startIndexRaw = indexRegister.u64;
    g_codeLengthTokenTrace.owner = ownerRegister.u32;
    g_codeLengthTokenTrace.inputIndexPointer =
        inputIndexPointerRegister.u32;
    g_codeLengthTokenTrace.inputBase = 0;
    g_codeLengthTokenTrace.inputEnd = 0;
    g_codeLengthTokenTrace.inputIndex = 0;
    g_codeLengthTokenTrace.bitBuffer = 0;
    g_codeLengthTokenTrace.bitCount = 0;

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    const auto owner = ownerRegister.u32;
    const auto inputIndexPointer = inputIndexPointerRegister.u32;
    if (frame != nullptr &&
        frame->guestMemory != nullptr &&
        static_cast<std::uint64_t>(owner) + 84 <=
            0x1'0000'0000ULL &&
        static_cast<std::uint64_t>(inputIndexPointer) + 8 <=
            0x1'0000'0000ULL)
    {
        const auto readGuestU32 =
            [&](const std::uint32_t address)
            {
                std::uint32_t raw = 0;
                std::memcpy(
                    &raw,
                    frame->guestMemory + address,
                    sizeof(raw));
                return _byteswap_ulong(raw);
            };
        const auto readGuestU64 =
            [&](const std::uint32_t address)
            {
                std::uint64_t raw = 0;
                std::memcpy(
                    &raw,
                    frame->guestMemory + address,
                    sizeof(raw));
                return _byteswap_uint64(raw);
            };
        g_codeLengthTokenTrace.inputBase = readGuestU32(owner + 56);
        g_codeLengthTokenTrace.inputEnd = readGuestU64(owner + 16);
        g_codeLengthTokenTrace.inputIndex =
            readGuestU64(inputIndexPointer);
        g_codeLengthTokenTrace.bitBuffer = readGuestU64(owner + 72);
        g_codeLengthTokenTrace.bitCount = readGuestU32(owner + 80);
        if (g_codeLengthTokenTrace.startIndex == 0)
        {
            g_codeLengthTokenTrace.sequenceArmed = true;
            g_codeLengthTokenTrace.sequenceOwner = owner;
            g_codeLengthTokenTrace.sequenceInputIndexPointer =
                inputIndexPointer;
            g_codeLengthTokenTrace.sequenceInputBase =
                g_codeLengthTokenTrace.inputBase;
            g_codeLengthTokenTrace.sequenceInputEnd =
                g_codeLengthTokenTrace.inputEnd;
            g_codeLengthTokenTrace.sequenceInputIndex =
                g_codeLengthTokenTrace.inputIndex;
            g_codeLengthTokenTrace.sequenceBitBuffer =
                g_codeLengthTokenTrace.bitBuffer;
            g_codeLengthTokenTrace.sequenceBitCount =
                g_codeLengthTokenTrace.bitCount;
        }
    }
    g_codeLengthTokenTrace.armed = true;
}

void Ac6TraceCodeLengthTokenEnd(
    PPCRegister& stackPointerRegister,
    PPCRegister& indexRegister,
    PPCRegister& symbolRegister,
    PPCRegister& r9Register,
    PPCRegister& r10Register,
    PPCRegister& r11Register,
    PPCRegister& totalCountRegister,
    PPCRegister& inputIndexPointerRegister,
    PPCRegister& ownerRegister)
{
    RecordCodeLengthEvent(
        CodeLengthEventKind::TokenEnd,
        stackPointerRegister.u32,
        indexRegister.u32 & 0xFFFF,
        symbolRegister.u32 & 0xFFFF,
        totalCountRegister.u32 & 0xFFFF,
        r9Register.u64,
        r10Register.u64,
        r11Register.u64);

    if (!g_codeLengthTokenTrace.armed)
    {
        return;
    }
    g_codeLengthTokenTrace.armed = false;

    const auto stackPointer = stackPointerRegister.u32;
    const auto startIndex = g_codeLengthTokenTrace.startIndex;
    const auto startIndexRaw = g_codeLengthTokenTrace.startIndexRaw;
    const auto endIndex = indexRegister.u32 & 0xFFFF;
    const auto totalCount = totalCountRegister.u32 & 0xFFFF;
    const auto symbol = symbolRegister.u32 & 0xFFFF;
    CaptureCodeLengthPrefixMismatch(
        "end",
        stackPointer,
        startIndex,
        endIndex,
        symbol,
        totalCount);
    const auto metadataInvalid =
        stackPointer != g_codeLengthTokenTrace.stackPointer ||
        endIndex < startIndex ||
        endIndex > 320 ||
        totalCount > 320 ||
        endIndex > totalCount;

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    const auto owner = ownerRegister.u32;
    const auto inputIndexPointer = inputIndexPointerRegister.u32;
    const auto ownerStateReadable =
        static_cast<std::uint64_t>(owner) + 148 <=
            0x1'0000'0000ULL &&
        static_cast<std::uint64_t>(inputIndexPointer) + 8 <=
            0x1'0000'0000ULL;
    const auto readGuestU32 =
        [&](const std::uint32_t address)
        {
            std::uint32_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + address,
                sizeof(raw));
            return _byteswap_ulong(raw);
        };
    const auto readGuestU64 =
        [&](const std::uint32_t address)
        {
            std::uint64_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + address,
                sizeof(raw));
            return _byteswap_uint64(raw);
        };
    const auto endInputBase =
        ownerStateReadable ? readGuestU32(owner + 56) : 0;
    const auto endInputEnd =
        ownerStateReadable ? readGuestU64(owner + 16) : 0;
    const auto endInputIndex =
        ownerStateReadable ? readGuestU64(inputIndexPointer) : 0;
    const auto endBitBuffer =
        ownerStateReadable ? readGuestU64(owner + 72) : 0;
    const auto endBitCount =
        ownerStateReadable ? readGuestU32(owner + 80) : 0;
    const auto codeLengthRootBits =
        ownerStateReadable ? readGuestU32(owner + 92) : 0;
    const auto codeLengthTable =
        ownerStateReadable ? readGuestU32(owner + 100) : 0;

    constexpr std::uint32_t kCodeLengthsStackOffset = 112;
    const auto boundedStart = std::min<std::uint32_t>(startIndex, 320);
    const auto boundedEnd = std::min<std::uint32_t>(
        std::max(startIndex, endIndex),
        320);
    std::array<std::uint16_t, 320> originalValues{};
    std::uint32_t invalidIndex =
        std::numeric_limits<std::uint32_t>::max();
    std::uint16_t invalidValue = 0;
    for (auto index = boundedStart; index < boundedEnd; ++index)
    {
        const auto address =
            stackPointer + kCodeLengthsStackOffset +
            index * sizeof(std::uint16_t);
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + address,
            sizeof(raw));
        const auto value = _byteswap_ushort(raw);
        originalValues[index] = value;
        if (value > 15 &&
            invalidIndex ==
                std::numeric_limits<std::uint32_t>::max())
        {
            invalidIndex = index;
            invalidValue = value;
        }
    }

    const auto spanLength =
        endIndex >= startIndex ? endIndex - startIndex : 0;
    std::uint32_t minimumSpan = 0;
    std::uint32_t maximumSpan = 0;
    std::uint16_t expectedValue = 0;
    auto semanticInvalid = metadataInvalid;
    auto repairable = !metadataInvalid;
    if (!metadataInvalid && symbol < 16)
    {
        minimumSpan = 1;
        maximumSpan = 1;
        expectedValue = static_cast<std::uint16_t>(symbol);
    }
    else if (!metadataInvalid && symbol == 16)
    {
        minimumSpan = 3;
        maximumSpan = 6;
        if (startIndex == 0)
        {
            semanticInvalid = true;
            repairable = false;
        }
        else
        {
            std::uint16_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + stackPointer +
                    kCodeLengthsStackOffset +
                    (startIndex - 1) * sizeof(raw),
                sizeof(raw));
            expectedValue = _byteswap_ushort(raw);
            if (expectedValue > 15)
            {
                semanticInvalid = true;
                repairable = false;
            }
        }
    }
    else if (!metadataInvalid && symbol == 17)
    {
        minimumSpan = 3;
        maximumSpan = 10;
    }
    else if (!metadataInvalid && symbol == 18)
    {
        minimumSpan = 11;
        maximumSpan = 138;
    }
    else if (!metadataInvalid)
    {
        semanticInvalid = true;
        repairable = false;
    }

    if (repairable &&
        (spanLength < minimumSpan || spanLength > maximumSpan))
    {
        semanticInvalid = true;
        repairable = false;
    }

    const auto expectedSequenceUsable =
        g_codeLengthTokenTrace.expectedArmed &&
        stackPointer ==
            g_codeLengthTokenTrace.expectedStackPointer &&
        startIndex == g_codeLengthTokenTrace.expectedCount &&
        !metadataInvalid &&
        spanLength >= minimumSpan &&
        spanLength <= maximumSpan &&
        (symbol < 16 ||
         symbol == 17 ||
         symbol == 18 ||
         (symbol == 16 && startIndex != 0));
    if (expectedSequenceUsable)
    {
        const auto modeledValue =
            symbol < 16
            ? static_cast<std::uint16_t>(symbol)
            : symbol == 16
                ? g_codeLengthTokenTrace
                    .expectedValues[startIndex - 1]
                : static_cast<std::uint16_t>(0);
        std::fill(
            g_codeLengthTokenTrace.expectedValues.begin() +
                startIndex,
            g_codeLengthTokenTrace.expectedValues.begin() +
                endIndex,
            modeledValue);
        g_codeLengthTokenTrace.expectedCount = endIndex;
    }
    else
    {
        g_codeLengthTokenTrace.expectedArmed = false;
    }

    std::uint32_t mismatchIndex =
        std::numeric_limits<std::uint32_t>::max();
    std::uint16_t mismatchValue = 0;
    if (repairable)
    {
        for (auto index = boundedStart;
             index < boundedEnd;
             ++index)
        {
            if (originalValues[index] != expectedValue)
            {
                semanticInvalid = true;
                mismatchIndex = index;
                mismatchValue = originalValues[index];
                break;
            }
        }
    }

    auto repaired = false;
    if (semanticInvalid && repairable)
    {
        const auto rawExpected = _byteswap_ushort(expectedValue);
        for (auto index = boundedStart;
             index < boundedEnd;
             ++index)
        {
            std::memcpy(
                frame->guestMemory + stackPointer +
                    kCodeLengthsStackOffset +
                    index * sizeof(rawExpected),
                &rawExpected,
                sizeof(rawExpected));
        }
        repaired = true;
    }

    const auto shouldLog =
        semanticInvalid &&
        !g_codeLengthTokenCaptured.exchange(
            true,
            std::memory_order_acq_rel);
    if (!shouldLog)
    {
        return;
    }

    std::array<char, 8192> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "thread=%lu module=%p guest_base=%p stack=%08X "
        "begin_stack=%08X start_index=%u start_index_raw=%016llX "
        "end_index=%u end_index_raw=%016llX total_count=%u "
        "total_count_raw=%016llX metadata_invalid=%u symbol=%04X "
         "symbol_raw=%016llX r9=%016llX r10=%016llX r11=%016llX "
         "invalid_index=%u invalid_value=%04X span=%u expected=%04X "
         "mismatch_index=%u mismatch_value=%04X repaired=%u\n",
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        stackPointer,
        g_codeLengthTokenTrace.stackPointer,
        startIndex,
        static_cast<unsigned long long>(startIndexRaw),
        endIndex,
        static_cast<unsigned long long>(indexRegister.u64),
        totalCount,
        static_cast<unsigned long long>(totalCountRegister.u64),
        metadataInvalid ? 1U : 0U,
        symbolRegister.u32 & 0xFFFF,
        static_cast<unsigned long long>(symbolRegister.u64),
        static_cast<unsigned long long>(r9Register.u64),
        static_cast<unsigned long long>(r10Register.u64),
        static_cast<unsigned long long>(r11Register.u64),
        invalidIndex,
        invalidValue,
        spanLength,
        expectedValue,
         mismatchIndex,
         mismatchValue,
         repaired ? 1U : 0U);
    append(
        "owner=%08X input_index_pointer=%08X "
        "begin_owner=%08X begin_input_index_pointer=%08X "
        "begin_input_base=%08X begin_input_end=%016llX "
        "begin_input_index=%016llX begin_bit_buffer=%016llX "
        "begin_bit_count=%u\n"
        "end_input_base=%08X end_input_end=%016llX "
        "end_input_index=%016llX end_bit_buffer=%016llX "
        "end_bit_count=%u root_bits=%u table=%08X\n"
        "sequence_armed=%u sequence_owner=%08X "
        "sequence_input_index_pointer=%08X "
        "sequence_input_base=%08X sequence_input_end=%016llX "
        "sequence_input_index=%016llX "
        "sequence_bit_buffer=%016llX sequence_bit_count=%u\n",
        owner,
        inputIndexPointer,
        g_codeLengthTokenTrace.owner,
        g_codeLengthTokenTrace.inputIndexPointer,
        g_codeLengthTokenTrace.inputBase,
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.inputEnd),
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.inputIndex),
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.bitBuffer),
        g_codeLengthTokenTrace.bitCount,
        endInputBase,
        static_cast<unsigned long long>(endInputEnd),
        static_cast<unsigned long long>(endInputIndex),
        static_cast<unsigned long long>(endBitBuffer),
        endBitCount,
        codeLengthRootBits,
        codeLengthTable,
        g_codeLengthTokenTrace.sequenceArmed ? 1U : 0U,
        g_codeLengthTokenTrace.sequenceOwner,
        g_codeLengthTokenTrace.sequenceInputIndexPointer,
        g_codeLengthTokenTrace.sequenceInputBase,
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.sequenceInputEnd),
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.sequenceInputIndex),
        static_cast<unsigned long long>(
            g_codeLengthTokenTrace.sequenceBitBuffer),
        g_codeLengthTokenTrace.sequenceBitCount);

    append("%s", "code_length_alphabet:");
    if (ownerStateReadable)
    {
        for (std::uint32_t index = 0; index < 19; ++index)
        {
            std::uint16_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + owner + 108 +
                    index * sizeof(raw),
                sizeof(raw));
            append(
                " %u:%u",
                index,
                static_cast<unsigned>(_byteswap_ushort(raw)));
        }
    }
    append("%s", "\n");

    const auto sequenceStateUsable =
        g_codeLengthTokenTrace.sequenceArmed &&
        g_codeLengthTokenTrace.sequenceInputBase == endInputBase &&
        g_codeLengthTokenTrace.sequenceInputEnd == endInputEnd &&
        g_codeLengthTokenTrace.sequenceInputIndex <= endInputIndex &&
        endInputIndex <= endInputEnd;
    if (sequenceStateUsable)
    {
        auto dumpStart =
            g_codeLengthTokenTrace.sequenceInputIndex > 16
                ? g_codeLengthTokenTrace.sequenceInputIndex - 16
                : 0;
        auto dumpEnd = std::min<std::uint64_t>(
            endInputEnd,
            endInputIndex + 64);
        if (dumpEnd > dumpStart + 1024)
        {
            dumpStart = dumpEnd - 1024;
        }
        append(
            "input_bytes offset=%016llX end=%016llX:",
            static_cast<unsigned long long>(dumpStart),
            static_cast<unsigned long long>(dumpEnd));
        for (auto offset = dumpStart; offset < dumpEnd; ++offset)
        {
            const auto address =
                static_cast<std::uint64_t>(endInputBase) + offset;
            if (address >= 0x1'0000'0000ULL)
            {
                break;
            }
            append(
                " %02X",
                static_cast<unsigned>(frame->guestMemory[address]));
        }
        append("%s", "\n");
    }

    append("%s", "new_values_before_repair:");
    for (auto index = boundedStart; index < boundedEnd; ++index)
    {
        append(
            " %u:%04X",
            index,
            static_cast<unsigned>(originalValues[index]));
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] =
        L".code-length-token-invalid.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}

void Ac6TraceCodeLengthRefillReturn(
    PPCRegister& stackPointerRegister,
    PPCRegister& indexRegister,
    PPCRegister& symbolRegister,
    PPCRegister& resultRegister,
    PPCRegister& inputRegister,
    PPCRegister& requestedBitsRegister,
    PPCRegister& ownerRegister)
{
    RecordCodeLengthEvent(
        CodeLengthEventKind::Refill,
        stackPointerRegister.u32,
        indexRegister.u32 & 0xFFFF,
        symbolRegister.u32 & 0xFFFF,
        0,
        resultRegister.u64,
        inputRegister.u64,
        requestedBitsRegister.u64);
    CaptureCodeLengthPrefixMismatch(
        "refill",
        stackPointerRegister.u32,
        indexRegister.u32 & 0xFFFF,
        indexRegister.u32 & 0xFFFF,
        symbolRegister.u32 & 0xFFFF,
        0);
    return;

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    constexpr std::uint32_t kCodeLengthsStackOffset = 112;
    const auto stackPointer = stackPointerRegister.u32;
    const auto index = indexRegister.u32 & 0xFFFF;
    const auto boundedIndex = std::min<std::uint32_t>(index, 320);
    std::uint32_t invalidIndex =
        std::numeric_limits<std::uint32_t>::max();
    std::uint16_t invalidValue = 0;
    for (std::uint32_t current = 0;
         current < boundedIndex;
         ++current)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + stackPointer +
                kCodeLengthsStackOffset +
                current * sizeof(raw),
            sizeof(raw));
        const auto value = _byteswap_ushort(raw);
        if (value > 15)
        {
            invalidIndex = current;
            invalidValue = value;
            break;
        }
    }

    if ((index <= 320 && invalidValue == 0) ||
        g_codeLengthRefillCaptured.exchange(
            true,
            std::memory_order_acq_rel))
    {
        return;
    }

    const auto owner = ownerRegister.u32;
    const auto readGuestU32 =
        [&](const std::uint32_t address)
        {
            std::uint32_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + address,
                sizeof(raw));
            return _byteswap_ulong(raw);
        };
    const auto readGuestU64 =
        [&](const std::uint32_t address)
        {
            std::uint64_t raw = 0;
            std::memcpy(
                &raw,
                frame->guestMemory + address,
                sizeof(raw));
            return _byteswap_uint64(raw);
        };

    std::array<void*, 32> stack{};
    const auto stackCount = CaptureStackBackTrace(
        0,
        static_cast<DWORD>(stack.size()),
        stack.data(),
        nullptr);

    std::array<char, 16384> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "thread=%lu module=%p guest_base=%p stack=%08X "
        "index=%u index_raw=%016llX symbol=%04X "
        "symbol_raw=%016llX result=%016llX input=%016llX "
        "requested_bits=%016llX owner=%08X invalid_index=%u "
        "invalid_value=%04X owner_bit_buffer=%016llX "
        "owner_bit_count=%08X\nprefix:",
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        stackPointer,
        index,
        static_cast<unsigned long long>(indexRegister.u64),
        symbolRegister.u32 & 0xFFFF,
        static_cast<unsigned long long>(symbolRegister.u64),
        static_cast<unsigned long long>(resultRegister.u64),
        static_cast<unsigned long long>(inputRegister.u64),
        static_cast<unsigned long long>(requestedBitsRegister.u64),
        owner,
        invalidIndex,
        invalidValue,
        static_cast<unsigned long long>(readGuestU64(owner + 72)),
        readGuestU32(owner + 80));
    for (std::uint32_t current = 0;
         current < boundedIndex;
         ++current)
    {
        std::uint16_t raw = 0;
        std::memcpy(
            &raw,
            frame->guestMemory + stackPointer +
                kCodeLengthsStackOffset +
                current * sizeof(raw),
            sizeof(raw));
        append(
            " %u:%04X",
            current,
            static_cast<unsigned>(_byteswap_ushort(raw)));
    }
    append("%s", "\nstack:");
    for (USHORT current = 0; current < stackCount; ++current)
    {
        append(" %p", stack[current]);
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] =
        L".code-length-refill-invalid.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}

void Ac6TraceIndirectTypeIndex(
    PPCRegister& typeIndexRegister,
    PPCRegister& tableBaseRegister,
    PPCRegister& ownerRegister,
    PPCRegister& remainingCountRegister)
{
    const auto typeIndex = typeIndexRegister.u32;
    constexpr std::uint32_t kPlausibleTypeIndexLimit = 4096;
    if (typeIndex <= kPlausibleTypeIndexLimit ||
        g_indirectTypeIndexCaptured.exchange(
            true,
            std::memory_order_acq_rel))
    {
        return;
    }

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    const auto readGuestU32 =
        [&](const std::uint32_t address)
        {
            std::uint32_t raw = 0;
            if (address <=
                std::numeric_limits<std::uint32_t>::max() -
                    sizeof(raw) + 1)
            {
                std::memcpy(
                    &raw,
                    frame->guestMemory + address,
                    sizeof(raw));
            }
            return _byteswap_ulong(raw);
        };

    const auto owner = ownerRegister.u32;
    const auto tableBase = tableBaseRegister.u32;
    const auto tableOffset = typeIndex << 2;
    const auto tableAddress = tableBase + tableOffset;
    const auto item = readGuestU32(owner + 248);

    std::array<void*, 32> stack{};
    const auto stackCount = CaptureStackBackTrace(
        0,
        static_cast<DWORD>(stack.size()),
        stack.data(),
        nullptr);

    std::array<char, 8192> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "thread=%lu module=%p guest_base=%p owner=%08X "
        "remaining_count=%08X item=%08X type_index=%08X "
        "table_base=%08X table_offset=%08X table_address=%08X\n"
        "owner_fields: +216=%08X +220=%08X +228=%08X +244=%08X "
        "+248=%08X +252=%08X\nstack:",
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        owner,
        remainingCountRegister.u32,
        item,
        typeIndex,
        tableBase,
        tableOffset,
        tableAddress,
        readGuestU32(owner + 216),
        readGuestU32(owner + 220),
        readGuestU32(owner + 228),
        readGuestU32(owner + 244),
        item,
        readGuestU32(owner + 252));
    for (USHORT index = 0; index < stackCount; ++index)
    {
        append(" %p", stack[index]);
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] =
        L".indirect-type-index-invalid.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}

namespace
{
void TraceCodeLengthCopyBefore(
    PPCRegister& destinationRegister,
    PPCRegister& sourceRegister,
    PPCRegister& byteCountRegister,
    const char* kind)
{
    g_distanceCopyTrace.armed = false;

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    const auto destination = destinationRegister.u32;
    const auto source = sourceRegister.u32;
    const auto byteCount = byteCountRegister.u32;
    if (byteCount == 0 ||
        byteCount > g_distanceCopyTrace.sourceBefore.size() ||
        static_cast<std::uint64_t>(destination) + byteCount >
            0x1'0000'0000ULL ||
        static_cast<std::uint64_t>(source) + byteCount >
            0x1'0000'0000ULL)
    {
        return;
    }

    g_distanceCopyTrace.kind = kind;
    g_distanceCopyTrace.destination = destination;
    g_distanceCopyTrace.source = source;
    g_distanceCopyTrace.byteCount = byteCount;
    std::memcpy(
        g_distanceCopyTrace.sourceBefore.data(),
        frame->guestMemory + source,
        byteCount);
    std::memcpy(
        g_distanceCopyTrace.destinationBefore.data(),
        frame->guestMemory + destination,
        byteCount);
    g_distanceCopyTrace.armed = true;
}

void TraceCodeLengthCopyAfter()
{
    if (!g_distanceCopyTrace.armed)
    {
        return;
    }
    g_distanceCopyTrace.armed = false;

    auto* const frame = g_activeFrames.find(
        [](const ActiveFrame&)
        {
            return true;
        });
    if (frame == nullptr || frame->guestMemory == nullptr)
    {
        return;
    }

    const auto byteCount = g_distanceCopyTrace.byteCount;
    std::array<std::uint8_t, 640> destinationAfter{};
    std::memcpy(
        destinationAfter.data(),
        frame->guestMemory + g_distanceCopyTrace.destination,
        byteCount);

    const auto findInvalid =
        [byteCount](const std::uint8_t* bytes)
        {
            for (std::uint32_t offset = 0;
                 offset + sizeof(std::uint16_t) <= byteCount;
                 offset += sizeof(std::uint16_t))
            {
                const auto value = static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(bytes[offset]) << 8 |
                    bytes[offset + 1]);
                if (value > 15)
                {
                    return std::pair{
                        offset / static_cast<std::uint32_t>(
                            sizeof(std::uint16_t)),
                        value,
                    };
                }
            }
            return std::pair{
                std::numeric_limits<std::uint32_t>::max(),
                static_cast<std::uint16_t>(0),
            };
        };

    const auto sourceInvalid =
        findInvalid(g_distanceCopyTrace.sourceBefore.data());
    const auto destinationInvalid = findInvalid(destinationAfter.data());
    if ((sourceInvalid.second == 0 && destinationInvalid.second == 0) ||
        g_distanceCopyCaptured.exchange(
            true,
            std::memory_order_acq_rel))
    {
        return;
    }

    std::array<char, 131072> report{};
    std::size_t reportLength = 0;
    const auto append =
        [&](const char* format, const auto... arguments)
        {
            if (reportLength >= report.size() - 1)
            {
                return;
            }
            const auto available = report.size() - reportLength;
            const auto written = std::snprintf(
                report.data() + reportLength,
                available,
                format,
                arguments...);
            if (written <= 0)
            {
                return;
            }
            reportLength += std::min<std::size_t>(
                static_cast<std::size_t>(written),
                available - 1);
        };

    append(
        "kind=%s thread=%lu module=%p guest_base=%p source=%08X "
        "destination=%08X byte_count=%u source_invalid_index=%u "
        "source_invalid_value=%04X destination_invalid_index=%u "
        "destination_invalid_value=%04X\nsource_before:",
        g_distanceCopyTrace.kind,
        GetCurrentThreadId(),
        &__ImageBase,
        frame->guestMemory,
        g_distanceCopyTrace.source,
        g_distanceCopyTrace.destination,
        byteCount,
        sourceInvalid.first,
        sourceInvalid.second,
        destinationInvalid.first,
        destinationInvalid.second);
    const auto elementCount = byteCount / sizeof(std::uint16_t);
    const auto appendValues =
        [&](const std::uint8_t* bytes)
        {
            for (std::uint32_t index = 0; index < elementCount; ++index)
            {
                const auto offset = index * sizeof(std::uint16_t);
                const auto value = static_cast<std::uint16_t>(
                    static_cast<std::uint16_t>(bytes[offset]) << 8 |
                    bytes[offset + 1]);
                append(" %u:%04X", index, value);
            }
        };
    appendValues(g_distanceCopyTrace.sourceBefore.data());
    append("%s", "\ndestination_before:");
    appendValues(g_distanceCopyTrace.destinationBefore.data());
    append("%s", "\ndestination_after:");
    appendValues(destinationAfter.data());
    append("%s", "\ncode_length_events:");
    if (g_distanceCopyTrace.source >= 112)
    {
        const auto expectedStack =
            g_distanceCopyTrace.source - 112;
        const auto firstSequence =
            g_codeLengthEvents.nextSequence >
                g_codeLengthEvents.events.size()
            ? g_codeLengthEvents.nextSequence -
                g_codeLengthEvents.events.size()
            : 0;
        for (auto sequence = firstSequence;
             sequence < g_codeLengthEvents.nextSequence;
             ++sequence)
        {
            const auto& event =
                g_codeLengthEvents.events[
                    sequence % g_codeLengthEvents.events.size()];
            if (event.sequence != sequence ||
                event.stackPointer != expectedStack)
            {
                continue;
            }
            append(
                " %c%" PRIu64 ":%u:%u:%u:%016llX:%016llX:%016llX",
                event.kind == CodeLengthEventKind::Refill ? 'R' : 'E',
                event.sequence,
                event.index,
                event.symbol,
                event.totalCount,
                static_cast<unsigned long long>(event.valueA),
                static_cast<unsigned long long>(event.valueB),
                static_cast<unsigned long long>(event.valueC));
        }
    }
    append("%s", "\n");

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        reinterpret_cast<HMODULE>(&__ImageBase),
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }
    auto* const extension = std::wcsrchr(path.data(), L'.');
    if (extension == nullptr)
    {
        return;
    }
    constexpr wchar_t kLogExtension[] =
        L".code-length-copy-invalid.log";
    const auto prefixLength =
        static_cast<std::size_t>(extension - path.data());
    if (prefixLength + std::size(kLogExtension) > path.size())
    {
        return;
    }
    std::wmemcpy(
        extension,
        kLogExtension,
        std::size(kLogExtension));

    const auto file = CreateFileW(
        path.data(),
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }
    DWORD bytesWritten = 0;
    WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(reportLength),
        &bytesWritten,
        nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
    OutputDebugStringA(report.data());
}
}

void Ac6TraceLiteralCopyBefore(
    PPCRegister& destinationRegister,
    PPCRegister& sourceRegister,
    PPCRegister& byteCountRegister)
{
    TraceCodeLengthCopyBefore(
        destinationRegister,
        sourceRegister,
        byteCountRegister,
        "literal");
}

void Ac6TraceLiteralCopyAfter()
{
    TraceCodeLengthCopyAfter();
}

void Ac6TraceDistanceCopyBefore(
    PPCRegister& destinationRegister,
    PPCRegister& sourceRegister,
    PPCRegister& byteCountRegister)
{
    TraceCodeLengthCopyBefore(
        destinationRegister,
        sourceRegister,
        byteCountRegister,
        "distance");
}

void Ac6TraceDistanceCopyAfter()
{
    TraceCodeLengthCopyAfter();
}

extern "C" void XeO3Dispatch(
    void* cpuState,
    std::uint8_t* guestMemory,
    const std::uint32_t guestIar,
    void* const hostFence) noexcept
{
    BridgeDispatchCount = BridgeDispatchCount + 1;
    BridgeLastGuestIar = guestIar;
    BridgeLastGuestMemory = reinterpret_cast<std::uintptr_t>(guestMemory);
    BridgeLastOuterHostFence = reinterpret_cast<std::uintptr_t>(hostFence);
    EmitEvent(EventKind::Dispatch, guestIar, 0, 0);
    EnsureKernelContinuationReady(cpuState, guestMemory, guestIar);

    xeo3::TranslatedState translated{};
    xeo3::CopyFromXeO3(xeo3::CpuStateView(cpuState), translated);
    translated.ppc.xeo3CpuState = cpuState;
    translated.ppc.xeo3GuestMemory = guestMemory;
    xeo3::SetTranslatedIar(translated, guestIar);

    ActiveFrame frame{
        cpuState,
        guestMemory,
        &translated,
        hostFence,
        nullptr,
    };
    translated.ppc.xeo3ActiveFrame = &frame;
    if (!RegisterActiveFrame(frame))
    {
        Fail(guestIar, 0, 11);
    }

    const auto hostMxcsr = _mm_getcsr();
    if (auto* const function = FindFunction(guestIar); function != nullptr)
    {
        function(translated.ppc, guestMemory);
        xeo3::SetTranslatedIar(
            translated,
            static_cast<std::uint32_t>(translated.ppc.lr));
        xeo3::CopyToXeO3(translated, xeo3::CpuStateView(cpuState));
    }
    else
    {
        UnregisterActiveFrame(frame);
        _mm_setcsr(hostMxcsr);
        Fail(guestIar, 0, 8);
    }

    BridgeDispatchExitCount = BridgeDispatchExitCount + 1;
    EmitEvent(
        EventKind::DispatchExit,
        guestIar,
        static_cast<std::uint32_t>(translated.ppc.lr),
        static_cast<std::uint32_t>(translated.iar));
    if (!UnregisterActiveFrame(frame))
    {
        Fail(guestIar, 0, 12);
    }
    translated.ppc.xeo3ActiveFrame = nullptr;
    _mm_setcsr(hostMxcsr);
}

extern "C" std::uint32_t InitPrecompiledDll(
    xeo3::MappingEntry** mappings,
    std::uint64_t* mappingCount,
    void* hostInterface,
    std::uint32_t options[2])
{
    g_eventSequence.store(0, std::memory_order_relaxed);
    xeo3::video::ResetSwapTrace();
    g_criticalSectionEventSequence.store(0, std::memory_order_relaxed);
    g_huffmanInputCaptured.store(false, std::memory_order_relaxed);
    g_targetLockTryFailureStreak = 0;
    BridgeCriticalSectionEventCount = 0;
    BridgeImportCountersEnabled = 0;
    for (auto& count : BridgeImportCounts)
    {
        count = 0;
    }
    BridgeIntegerImportSyncEnabled = 0;
    BridgeIntegerImportSyncAttemptCount = 0;
    BridgeIntegerImportSyncHitCount = 0;
    BridgeIntegerImportSyncFallbackCount = 0;
    BridgeIntegerImportSyncLastThunk = 0;
    BridgeFastSynchronizationTelemetryEnabled = 0;
    BridgeFastCriticalSectionsEnabled = 0;
    BridgeFastCriticalSectionAttemptCount = 0;
    BridgeFastCriticalSectionHitCount = 0;
    BridgeFastCriticalSectionFallbackCount = 0;
    BridgeFastCriticalSectionSignalCount = 0;
    BridgeFastCriticalSectionLastThunk = 0;
    BridgeFastCriticalSectionLastAddress = 0;
    BridgeFastCriticalSectionLastThread = 0;
    BridgeFastCriticalSectionLastDisposition = 0;
    BridgeFastSpinLocksEnabled = 0;
    BridgeFastSpinLockAttemptCount = 0;
    BridgeFastSpinLockHitCount = 0;
    BridgeFastSpinLockFallbackCount = 0;
    BridgeFastSpinLockContentionCount = 0;
    BridgeFastSpinLockLastThunk = 0;
    BridgeFastSpinLockLastAddress = 0;
    BridgeFastIrqlEnabled = 0;
    BridgeFastIrqlTelemetryEnabled = 0;
    BridgeFastIrqlAttemptCount = 0;
    BridgeFastIrqlHitCount = 0;
    BridgeFastIrqlNativeFallbackCount = 0;
    BridgeFastIrqlInvalidFallbackCount = 0;
    BridgeFastIrqlLastThunk = 0;
    BridgeFastIrqlLastOldIrql = 0;
    BridgeFastIrqlLastNewIrql = 0;
    BridgeFastIrqlLastPendingIrql = 0;
    BridgeRaiseIrqlOldPassiveCount = 0;
    BridgeRaiseIrqlOldApcCount = 0;
    BridgeRaiseIrqlOldDispatchCount = 0;
    BridgeRaiseIrqlUnexpectedCount = 0;
    BridgeIndirectTelemetryEnabled = 0;
    BridgeIndirectStateSyncEnabled = 0;
    BridgeIndirectCallCount = 0;
    BridgeLastIndirectTarget = 0;
    BridgeLastIndirectCallerIar = 0;
    BridgeFatalIndirectCount = 0;
    BridgeLastFatalIndirectTarget = 0;
    BridgeLastFatalIndirectCallerIar = 0;
    BridgeLastFatalIndirectObject = 0;
    BridgeLastFatalIndirectVtable = 0;
    BridgeLastFatalIndirectSlot0 = 0;
    BridgeLastFatalIndirectSlot1 = 0;
    BridgeLastFatalIndirectArgument4 = 0;
    BridgeLastFatalIndirectThreadId = 0;
    BridgeWorkerEntryCount = 0;
    BridgeWorkerLoopCount = 0;
    BridgeWorkerCallbackDispatchCount = 0;
    BridgeWorkerCallbackReturnCount = 0;
    BridgeWorkerActiveDecrementCount = 0;
    BridgeQueueWaitLoopCount = 0;
    BridgeLastQueueWaitObject = 0;
    BridgeLastQueueWaitCount = 0;
    BridgeQueueWaitPositiveCount = 0;
    BridgeQueueWaitPauseCount = 0;
    BridgeQueueWaitSwitchCount = 0;
    BridgeQueueWaitSleepCount = 0;
    BridgeWorkerCallbackTargetCount = 0;
    BridgeLastWorkerCallbackTarget = 0;
    BridgeLastWorkerCallbackCallerIar = 0;
    BridgeLastWorkerPrimaryTarget = 0;
    BridgeLastWorkerPrimaryObject = 0;
    BridgeLastWorkerPrimaryPayload = 0;
    BridgeLastWorkerPrimaryThreadId = 0;
    BridgeActiveWorkerPrimaryTarget = 0;
    BridgeActiveWorkerPrimaryObject = 0;
    BridgeActiveWorkerPrimaryPayload = 0;
    BridgeActiveWorkerPrimaryThreadId = 0;
    BridgeActiveWorkerPrimaryClaimCount = 0;
    BridgeLastWorkerDestroyTarget = 0;
    BridgeLastWorkerDestroyObject = 0;
    BridgeAc6AudioPollClockFallbackEnabled = 1;
    BridgeAc6AudioPollTraceEnabled = 0;
    BridgeGuestIarTelemetryEnabled = 0;
    BridgeAc6AudioPollCallCount = 0;
    BridgeAc6AudioPollReturnCount = 0;
    BridgeAc6AudioPollSampleCount = 0;
    BridgeAc6AudioPollContinueCount = 0;
    BridgeAc6AudioPollTimeoutCount = 0;
    BridgeAc6AudioPollExitCount = 0;
    BridgeAc6AudioPollTimeBaseSampleCount = 0;
    BridgeAc6AudioPollClockFallbackCount = 0;
    BridgeAc6AudioPollDelayCount = 0;
    BridgeAc6AudioPollLastGuestIar = 0;
    BridgeAc6AudioPollLastThreadId = 0;
    BridgeAc6AudioPollLastObject = 0;
    BridgeAc6AudioPollLastFrame = 0;
    BridgeAc6AudioPollLastGuestMemory = 0;
    BridgeAc6AudioPollLastTimeBase = 0;
    BridgeAc6AudioPollLastGlobalTick = 0;
    BridgeAc6AudioPollLastGlobalClock = 0;
    BridgeAc6AudioPollLastObjectClock = 0;
    BridgeAc6AudioPollLastBaselineTick = 0;
    BridgeAc6AudioPollLastDelta = 0;
    BridgeAc6AudioPollLastConsumerPointer = 0;
    BridgeAc6AudioPollLastConsumerValue = 0;
    BridgeAc6AudioPollLastObservedConsumer = 0;
    BridgeAc6AudioPollLastRequiredDistance = 0;
    BridgeAc6AudioPollLastAvailableDistance = 0;
    BridgeAc6AudioPollLastReturn = 0;
    BridgeAc6AudioPollLastResolvedTick = 0;
    BridgeSynchronousQueueEnabled = 1;
    BridgeSynchronousQueueBypassCount = 0;
    BridgeSynchronousQueueSubmitCount = 0;
    BridgeSynchronousQueueTaskCount = 0;
    BridgeSynchronousQueueFailureCount = 0;
    BridgeSynchronousQueueFallbackCount = 0;
    BridgeSynchronousQueueLastObject = 0;
    BridgeSynchronousQueueLastTask = 0;
    BridgeSynchronousQueueLastTarget = 0;
    BridgeSynchronousQueueLastPhase = 0;
    BridgeWorkerPrimarySlot0Target = 0;
    BridgeWorkerPrimarySlot0Object = 0;
    BridgeWorkerPrimarySlot0Payload = 0;
    BridgeWorkerPrimarySlot0ThreadId = 0;
    BridgeWorkerPrimarySlot1Target = 0;
    BridgeWorkerPrimarySlot1Object = 0;
    BridgeWorkerPrimarySlot1Payload = 0;
    BridgeWorkerPrimarySlot1ThreadId = 0;
    BridgeWorkerPrimarySlot2Target = 0;
    BridgeWorkerPrimarySlot2Object = 0;
    BridgeWorkerPrimarySlot2Payload = 0;
    BridgeWorkerPrimarySlot2ThreadId = 0;
    BridgeWorkerPrimarySlot3Target = 0;
    BridgeWorkerPrimarySlot3Object = 0;
    BridgeWorkerPrimarySlot3Payload = 0;
    BridgeWorkerPrimarySlot3ThreadId = 0;
    BridgeLastOuterHostFence = 0;
    BridgeLastNestedHostFence = 0;
    BridgeRtlTryLastFailureStreak = 0;
    BridgeSynchronousArchiveReadCount = 0;
    BridgeSynchronousArchiveReadAttemptCount = 0;
    BridgeSynchronousArchiveReadFailureCount = 0;
    BridgeSynchronousArchiveReadLastPhase = 0;
    BridgeSynchronousArchiveReadLastCallerIar = 0;
    BridgeSynchronousArchiveReadLastSignalThreadId = 0;
    BridgeSynchronousArchiveReadLastOffset = 0;
    BridgeSynchronousArchiveReadLastIndex = 0;
    BridgeSynchronousArchiveReadLastHandle = 0;
    BridgeSynchronousArchiveReadLastLength = 0;
    BridgeSynchronousArchiveReadLastCompleted = 0;
    BridgeSynchronousArchiveReadLastError = 0;
    BridgeSynchronousArchiveSignalCount = 0;
    BridgeSynchronousArchiveSignalFailureCount = 0;
    BridgeSynchronousArchiveReadLastEvent = 0;
    BridgeSynchronousArchiveSignalLastStatus = 0;
    BridgeNtWaitCount = 0;
    BridgeNtWaitLastHandle = 0;
    BridgeNtWaitLastWaitMode = 0;
    BridgeNtWaitLastTimeout = 0;
    BridgeNtWaitLastStatus = 0;
    BridgeNtWaitLastCallerLr = 0;
    BridgeNtReleaseSemaphoreCount = 0;
    BridgeNtReleaseSemaphoreLastHandle = 0;
    BridgeNtReleaseSemaphoreLastReleaseCount = 0;
    BridgeNtReleaseSemaphoreLastPreviousCount = 0;
    BridgeNtReleaseSemaphoreLastStatus = 0;
    BridgeNtReleaseSemaphoreLastCallerLr = 0;
    std::memset(
        BridgeCriticalSectionEvents,
        0,
        sizeof(BridgeCriticalSectionEvents));
    for (auto& count : g_eventCounts)
    {
        count.store(0, std::memory_order_relaxed);
    }
    g_hostInterface = hostInterface;
    const auto pinnedHost = ValidatePinnedHost();
    const auto noCallStackModule = IsNoCallStackAotModule();
    if (!noCallStackModule && !pinnedHost)
    {
        g_hostInterface = nullptr;
        return 0;
    }
    if (!BuildMappings())
    {
        g_hostInterface = nullptr;
        return 0;
    }
    if (!g_activeFrames.initialize())
    {
        g_hostInterface = nullptr;
        return 0;
    }
    if (!noCallStackModule &&
        !xeo3::kernel::Initialize(
            &ExecuteKernelMappedGuest,
            reinterpret_cast<void*>(&XeO3AotThunk),
            reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr)) +
                kUnmappedDispatchRva))
    {
        g_activeFrames.cleanup();
        g_hostInterface = nullptr;
        return 0;
    }
    g_kernelContinuationsEnabled = !noCallStackModule;
    if (pinnedHost &&
        !noCallStackModule &&
        !xeo3::host::InstallPinnedUnmappedIarObserver(
            &HashFileSha256))
    {
        xeo3::kernel::Shutdown();
        g_kernelContinuationsEnabled = false;
        g_activeFrames.cleanup();
        g_hostInterface = nullptr;
        return 0;
    }
    if (pinnedHost &&
        !noCallStackModule &&
        !xeo3::vgpu::InstallPinnedVgpuPatch(&HashFileSha256))
    {
        xeo3::host::RemovePinnedUnmappedIarObserver();
        xeo3::kernel::Shutdown();
        g_kernelContinuationsEnabled = false;
        g_activeFrames.cleanup();
        g_hostInterface = nullptr;
        return 0;
    }

    if (mappings != nullptr)
    {
        *mappings = g_mappings.data();
    }
    if (mappingCount != nullptr)
    {
        *mappingCount = g_mappings.size();
    }

    const auto requestedVersion =
        options == nullptr ? xeo3::kCurrentAbiVersion : options[1];
    if (options != nullptr)
    {
        options[0] = 1;
        options[1] = xeo3::kCurrentAbiVersion;
    }

    EmitEvent(
        EventKind::Dispatch,
        xeo3::kAc6EntryPoint,
        0,
        static_cast<std::uint32_t>(g_functionCount));
    return NegotiateVersion(requestedVersion);
}

extern "C" void CleanupPrecompiledDll()
{
    xeo3::vgpu::RemovePinnedVgpuPatch();
    xeo3::host::RemovePinnedUnmappedIarObserver();
    xeo3::kernel::Shutdown();
    g_kernelContinuationsEnabled = false;
    g_activeFrames.cleanup();
    for (std::size_t index = 0;
         PrecompiledImportTable[index * 2] != 0;
         ++index)
    {
        PrecompiledImportTable[index * 2 + 1] = 0;
    }
    for (auto& pointer : PrecompiledPointers)
    {
        pointer = 0;
    }
    g_hostInterface = nullptr;
    g_functionCount = 0;
    g_functionLookup.clear();
    BridgeFunctionLookupEntryCount = 0;
}

bool HotImportDiagnosticsEnabled() noexcept
{
    return BridgeImportCountersEnabled != 0 ||
        BridgeEventTraceEnabled != 0 ||
        BridgeThreadImportTraceEnabled != 0 ||
        BridgeFastSynchronizationTelemetryEnabled != 0 ||
        BridgeFastIrqlTelemetryEnabled != 0;
}

template <std::size_t Index, std::uint32_t Thunk>
void ExecuteImportEntry(
    PPCContext& context,
    std::uint8_t* const guestMemory)
{
    if constexpr (Thunk == xeo3::video::kVdSwapThunk)
    {
        const xeo3::video::SwapArguments arguments{
            {context.r3.u32, context.r4.u32, context.r5.u32, context.r6.u32,
             context.r7.u32, context.r8.u32, context.r9.u32, context.r10.u32},
            context.r1.u32, static_cast<std::uint32_t>(context.lr)};
        const auto sequence = xeo3::video::BeginSwapTrace(arguments, guestMemory);
        ExecuteImport(context, guestMemory, Index, Thunk);
        xeo3::video::EndSwapTrace(sequence, guestMemory, context.r3.u32,
                                 static_cast<std::uint32_t>(context.lr));
        return;
    }

    if (!HotImportDiagnosticsEnabled())
    {
        if constexpr (
            Thunk == kRtlEnterCriticalSectionThunk ||
            Thunk == kRtlLeaveCriticalSectionThunk ||
            Thunk == kRtlTryEnterCriticalSectionThunk)
        {
            if (TryExecuteFastCriticalSection(context, guestMemory, Thunk))
            {
                return;
            }
        }
        else if constexpr (
            Thunk == kKeAcquireSpinLockAtRaisedIrqlThunk ||
            Thunk == kKeTryToAcquireSpinLockAtRaisedIrqlThunk ||
            Thunk == kKeReleaseSpinLockFromRaisedIrqlThunk)
        {
            if (TryExecuteFastSpinLock(context, guestMemory, Thunk))
            {
                return;
            }
        }
        else if constexpr (
            Thunk == kKeRaiseIrqlToDpcLevelThunk ||
            Thunk == kKfLowerIrqlThunk)
        {
            if (TryExecuteFastIrql(context, guestMemory, Thunk))
            {
                return;
            }
        }
    }

    ExecuteImport(context, guestMemory, Index, Thunk);
}

#define XEO3_AC6_IMPORT(index, address, name)                  \
    PPC_FUNC(name)                                            \
    {                                                         \
        ExecuteImportEntry<index, address>(ctx, base);         \
    }
#include "ac6_imports.inc"
#undef XEO3_AC6_IMPORT

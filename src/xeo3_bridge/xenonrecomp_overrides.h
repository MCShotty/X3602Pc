#pragma once

#include "xeo3_bridge/ac6_audio_poll.h"
#include "xeo3_bridge/ac6_queue_wait.h"
#include "xeo3_bridge/xeo3_cpu_state.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

#define PPC_CONTEXT_EXTENSION \
    void* xeo3CpuState = nullptr; \
    void* xeo3ActiveFrame = nullptr; \
    std::uint8_t* xeo3GuestMemory = nullptr; \
    std::uint32_t xeo3GuestIar = 0; \
    std::uint32_t xeo3IndirectCallsUntilSync = 16;

struct PPCContext;

extern "C"
{
extern __declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollClockFallbackEnabled;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollTraceEnabled;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeGuestIarTelemetryEnabled;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeContinuousStatePublicationMode;
std::uint32_t BridgeAc6AudioPollResolveTick(
    std::uint32_t guestTick,
    std::uint8_t* guestMemory,
    std::uint32_t frameAddress) noexcept;
#if defined(XEO3_RUNTIME_TELEMETRY)
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerEntryCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerLoopCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackDispatchCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackReturnCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerActiveDecrementCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeQueueWaitLoopCount;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastQueueWaitObject;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastQueueWaitCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeQueueWaitPositiveCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeQueueWaitPauseCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeQueueWaitSwitchCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeQueueWaitSleepCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeWorkerCallbackTargetCount;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerCallbackTarget;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerCallbackCallerIar;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryTarget;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryObject;
extern __declspec(dllexport) volatile std::uint64_t BridgeLastWorkerPrimaryPayload;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerPrimaryThreadId;
extern __declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryTarget;
extern __declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryObject;
extern __declspec(dllexport) volatile std::uint64_t BridgeActiveWorkerPrimaryPayload;
extern __declspec(dllexport) volatile std::uint32_t BridgeActiveWorkerPrimaryThreadId;
extern __declspec(dllexport) volatile std::uint64_t BridgeActiveWorkerPrimaryClaimCount;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerDestroyTarget;
extern __declspec(dllexport) volatile std::uint32_t BridgeLastWorkerDestroyObject;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollCallCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollReturnCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollSampleCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollContinueCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollTimeoutCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollExitCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollTimeBaseSampleCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeAc6AudioPollClockFallbackCount;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollDelayCount;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGuestIar;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastThreadId;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastObject;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastFrame;
extern __declspec(dllexport) volatile std::uintptr_t BridgeAc6AudioPollLastGuestMemory;
extern __declspec(dllexport) volatile std::uint64_t BridgeAc6AudioPollLastTimeBase;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGlobalTick;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastGlobalClock;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastObjectClock;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastBaselineTick;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastDelta;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastConsumerPointer;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastConsumerValue;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastObservedConsumer;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastRequiredDistance;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastAvailableDistance;
extern __declspec(dllexport) volatile std::uint32_t BridgeAc6AudioPollLastReturn;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeAc6AudioPollLastResolvedTick;
#endif
void BridgeQueueWaitCooperativeYield(
    std::uint32_t objectAddress,
    std::uint32_t count) noexcept;
#if defined(XEO3_RUNTIME_TELEMETRY)
void BridgeAc6AudioPollObserve(
    std::uint32_t guestIar,
    std::uint32_t r3,
    std::uint32_t r9,
    std::uint32_t r10,
    std::uint64_t r11,
    std::uint32_t r13,
    std::uint32_t r29,
    std::uint32_t r30,
    std::uint32_t r31) noexcept;
#endif
}

namespace xeo3
{
constexpr std::uint32_t kGuestPhysicalAddressBase = 0xA0000000U;
constexpr std::uint32_t kIndirectStateSyncInterval = 16U;
#if defined(XEO3_CONTINUOUS_STATE_PUBLICATION)
inline constexpr bool kContinuousStatePublicationEnabled = true;
#else
inline constexpr bool kContinuousStatePublicationEnabled = false;
#endif

constexpr bool IsWorkerProbeInstruction(
    const std::uint32_t guestIar) noexcept
{
    switch (guestIar)
    {
    case 0x82346428U:
    case 0x8234643CU:
    case 0x823464BCU:
    case 0x823464C0U:
    case 0x82346530U:
    case 0x823466ACU:
    case 0x823466B0U:
        return true;
    default:
        return false;
    }
}

constexpr bool IsAudioPollTraceInstruction(
    const std::uint32_t guestIar) noexcept
{
    switch (guestIar)
    {
    case 0x821E6238U:
    case 0x821E6240U:
    case 0x821E6248U:
    case 0x821E6264U:
    case 0x821E6B3CU:
    case 0x821E6B70U:
    case 0x821E6B74U:
    case 0x821E6B78U:
    case 0x821E6B80U:
    case 0x821E6B88U:
    case 0x821E6B90U:
        return true;
    default:
        return false;
    }
}

constexpr bool HasGuestIarHook(const std::uint32_t guestIar) noexcept
{
    return guestIar == 0x821E6B38U ||
           ac6_audio::IsDelay16CycleInstruction(guestIar) ||
           IsWorkerProbeInstruction(guestIar) ||
           IsAudioPollTraceInstruction(guestIar);
}

static_assert(HasGuestIarHook(0x821E6AE4U));
static_assert(HasGuestIarHook(0x821E6B38U));
static_assert(HasGuestIarHook(0x823466B0U));
static_assert(!HasGuestIarHook(0x82090000U));

inline void RecordWorkerProbeIar(const std::uint32_t guestIar) noexcept
{
#if defined(XEO3_RUNTIME_TELEMETRY)
    if ((guestIar & 0xFFFF0000U) != 0x82340000U)
    {
        return;
    }

    switch (guestIar)
    {
    case 0x82346428U:
        BridgeWorkerEntryCount = BridgeWorkerEntryCount + 1;
        break;
    case 0x8234643CU:
        BridgeWorkerLoopCount = BridgeWorkerLoopCount + 1;
        break;
    case 0x823464BCU:
        BridgeWorkerCallbackDispatchCount =
            BridgeWorkerCallbackDispatchCount + 1;
        break;
    case 0x823464C0U:
        BridgeWorkerCallbackReturnCount =
            BridgeWorkerCallbackReturnCount + 1;
        break;
    case 0x82346530U:
        BridgeWorkerActiveDecrementCount =
            BridgeWorkerActiveDecrementCount + 1;
        break;
    case 0x823466ACU:
        BridgeQueueWaitLoopCount = BridgeQueueWaitLoopCount + 1;
        break;
    default:
        break;
    }
#else
    static_cast<void>(guestIar);
#endif
}

inline void RecordQueueWaitProbe(
    const std::uint32_t objectAddress,
    const std::uint32_t count) noexcept
{
#if defined(XEO3_RUNTIME_TELEMETRY)
    BridgeLastQueueWaitObject = objectAddress;
    BridgeLastQueueWaitCount = count;
    if (IsQueueWaitActive(count))
    {
        BridgeQueueWaitPositiveCount = BridgeQueueWaitPositiveCount + 1;
    }
#else
    static_cast<void>(objectAddress);
    static_cast<void>(count);
#endif
}

constexpr std::uint32_t SwapMmioWord(const std::uint32_t value) noexcept
{
    return ((value & 0x000000FFU) << 24U) |
           ((value & 0x0000FF00U) << 8U) |
           ((value & 0x00FF0000U) >> 8U) |
           ((value & 0xFF000000U) >> 24U);
}

constexpr bool IsXeO3MmioAddress(const std::uint32_t address) noexcept
{
    return address - 0x7FC80000U < 0x00020000U ||
           address - 0x7FEA1800U < 0x00000400U ||
           address - 0x8FFF1000U < 0x00001000U ||
           address - 0x7FEA1000U < 0x00000200U ||
           address - 0x7F000000U < 0x00000800U ||
           address - 0xFD000000U < 0x02000000U;
}

inline std::uint8_t* GuestMemoryPointer(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress) noexcept
{
    // XeO3 supplies R15 as the active full 4 GiB guest-address view. The
    // dispatcher selects the physical or virtual host mapping before entering
    // the AOT thunk, so translated code retains the complete guest offset.
    return guestMemory + guestAddress;
}

__forceinline void TrackGuestIar(
    const std::uint32_t guestIar,
    std::uint8_t* const guestMemory,
    const std::uint32_t r3,
    const std::uint32_t r9,
    const std::uint32_t r10,
    const std::uint32_t r29,
    const std::uint64_t r11,
    const std::uint32_t r13,
    std::uint64_t& r30,
    const std::uint32_t r31) noexcept
{
#if defined(XEO3_RUNTIME_TELEMETRY)
    if (guestIar == 0x821E6B38U)
    {
        r30 = BridgeAc6AudioPollResolveTick(
            static_cast<std::uint32_t>(r30),
            guestMemory,
            r31);
    }
#else
    static_cast<void>(guestMemory);
#endif
    if (ac6_audio::IsDelay16CycleInstruction(guestIar))
    {
        _mm_pause();
#if defined(XEO3_RUNTIME_TELEMETRY)
        if (BridgeAc6AudioPollTraceEnabled != 0)
        {
            BridgeAc6AudioPollDelayCount = BridgeAc6AudioPollDelayCount + 1;
        }
#endif
    }

#if defined(XEO3_RUNTIME_TELEMETRY)
    if ((guestIar & 0xFFFF0000U) == 0x82340000U &&
        BridgeGuestIarTelemetryEnabled != 0)
    {
        RecordWorkerProbeIar(guestIar);
        if (guestIar == 0x823466ACU)
        {
            BridgeLastQueueWaitObject = r29;
            BridgeLastQueueWaitCount = 0xFFFFFFFFU;
        }
        else if (guestIar == 0x823466B0U)
        {
            RecordQueueWaitProbe(r29, static_cast<std::uint32_t>(r11));
        }
    }

    if (guestIar - 0x821E6238U <= 0x00000958U &&
        BridgeAc6AudioPollTraceEnabled != 0)
    {
        switch (guestIar)
        {
        case 0x821E6238U:
        case 0x821E6240U:
        case 0x821E6248U:
        case 0x821E6264U:
        case 0x821E6B3CU:
        case 0x821E6B70U:
        case 0x821E6B74U:
        case 0x821E6B78U:
        case 0x821E6B80U:
        case 0x821E6B88U:
        case 0x821E6B90U:
            BridgeAc6AudioPollObserve(
                guestIar,
                r3,
                r9,
                r10,
                r11,
                r13,
                r29,
                static_cast<std::uint32_t>(r30),
                r31);
            break;
        default:
            break;
        }
    }
#else
    static_cast<void>(r3);
    static_cast<void>(r9);
    static_cast<void>(r10);
    static_cast<void>(r29);
    static_cast<void>(r11);
    static_cast<void>(r13);
    static_cast<void>(r30);
    static_cast<void>(r31);
#endif
    if (guestIar == 0x823466B0U)
    {
        BridgeQueueWaitCooperativeYield(
            r29,
            static_cast<std::uint32_t>(r11));
    }
}

inline void PublishGprValue(
    void* const cpuState,
    const std::size_t index,
    const std::uint64_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setGpr(index, value);
    }
}

inline void PublishIarValue(
    void* const cpuState,
    const std::uint32_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setIar(value);
    }
}

inline void PublishFprValue(
    void* const cpuState,
    const std::size_t index,
    const std::uint64_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setFprBits(index, value);
    }
}

inline void PublishVectorValue(
    void* const cpuState,
    const std::size_t index,
    const void* const value) noexcept
{
    if (cpuState == nullptr)
    {
        return;
    }

    CpuStateView::VectorRegister target{};
    std::memcpy(target.data(), value, target.size());
    CpuStateView(cpuState).setVector(index, target);
}

inline void PublishConditionValue(
    void* const cpuState,
    const std::size_t index,
    const std::uint8_t lt,
    const std::uint8_t gt,
    const std::uint8_t eq,
    const std::uint8_t so) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setCondition(
            index,
            {lt, gt, eq, so});
    }
}

inline void PublishLrValue(
    void* const cpuState,
    const std::uint64_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setLr(value);
    }
}

inline void PublishCtrValue(
    void* const cpuState,
    const std::uint64_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setCtr(value);
    }
}

inline void PublishXerValue(
    void* const cpuState,
    const bool so,
    const bool ov,
    const bool ca) noexcept
{
    if (cpuState == nullptr)
    {
        return;
    }

    auto destination = CpuStateView(cpuState);
    destination.setXer(
        so,
        ov,
        ca,
        destination.xerByteCount());
}

inline void PublishFpscrValue(
    void* const cpuState,
    const std::uint32_t mxcsr,
    const std::uint32_t roundingMode) noexcept
{
    if (cpuState == nullptr)
    {
        return;
    }

    auto destination = CpuStateView(cpuState);
    destination.setMxcsr(mxcsr);
    destination.setFpscr(
        (destination.fpscr() & ~0x3U) |
        (roundingMode & 0x3U));
}

inline void PublishVscrValue(
    void* const cpuState,
    const std::uint32_t value) noexcept
{
    if (cpuState != nullptr)
    {
        CpuStateView(cpuState).setVscr(value);
    }
}

inline void PublishMsrValue(
    void* const cpuState,
    const std::uint32_t value) noexcept
{
    if (cpuState == nullptr)
    {
        return;
    }

    auto destination = CpuStateView(cpuState);
    destination.setMsr(
        (destination.msr() & 0xFFFFFFFF00000000ULL) |
        value);
}

void CallIndirect(PPCContext& context, std::uint8_t* guestMemory, std::uint32_t guestAddress);
void PublishStackPointer(
    PPCContext& context,
    std::uint8_t* guestMemory) noexcept;
std::uint32_t MmioLoad32(
    PPCContext& context,
    std::uint8_t* guestMemory,
    std::uint32_t guestAddress);
void MmioStore32(
    PPCContext& context,
    std::uint8_t* guestMemory,
    std::uint32_t guestAddress,
    std::uint32_t value);
std::uint64_t ReadTimeBase(
    PPCContext& context,
    std::uint8_t* guestMemory);

inline std::uint16_t LoadU16(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress) noexcept
{
    return static_cast<std::uint16_t>(
        _loadbe_i16(GuestMemoryPointer(guestMemory, guestAddress)));
}

inline std::uint8_t LoadU8(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress) noexcept
{
    return *GuestMemoryPointer(guestMemory, guestAddress);
}

inline std::uint32_t LoadU32(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress)
{
    if (IsXeO3MmioAddress(guestAddress))
    {
        return MmioLoad32(context, guestMemory, guestAddress);
    }

    return static_cast<std::uint32_t>(
        _loadbe_i32(GuestMemoryPointer(guestMemory, guestAddress)));
}

inline std::uint64_t LoadU64(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress) noexcept
{
    return static_cast<std::uint64_t>(
        _loadbe_i64(GuestMemoryPointer(guestMemory, guestAddress)));
}

inline void StoreU16(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress,
    const std::uint16_t value) noexcept
{
    _storebe_i16(
        GuestMemoryPointer(guestMemory, guestAddress),
        static_cast<short>(value));
}

inline void StoreU8(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress,
    const std::uint8_t value) noexcept
{
    *GuestMemoryPointer(guestMemory, guestAddress) = value;
}

inline void StoreU32(
    PPCContext& context,
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress,
    const std::uint32_t value)
{
    if (IsXeO3MmioAddress(guestAddress))
    {
        MmioStore32(context, guestMemory, guestAddress, value);
        return;
    }

    _storebe_i32(
        GuestMemoryPointer(guestMemory, guestAddress),
        static_cast<int>(value));
}

inline void StoreU64(
    std::uint8_t* const guestMemory,
    const std::uint32_t guestAddress,
    const std::uint64_t value) noexcept
{
    _storebe_i64(
        GuestMemoryPointer(guestMemory, guestAddress),
        static_cast<long long>(value));
}
}

#define PPC_LOAD_U8(address) \
    ::xeo3::LoadU8( \
        base, static_cast<std::uint32_t>(address))

#define PPC_LOAD_U16(address) \
    ::xeo3::LoadU16( \
        base, static_cast<std::uint32_t>(address))

#define PPC_LOAD_U32(address) \
    ::xeo3::LoadU32( \
        ctx, base, static_cast<std::uint32_t>(address))

#define PPC_LOAD_U64(address) \
    ::xeo3::LoadU64( \
        base, static_cast<std::uint32_t>(address))

#define PPC_STORE_U16(address, value) \
    ::xeo3::StoreU16( \
        base, \
        static_cast<std::uint32_t>(address), \
        static_cast<std::uint16_t>(value))

#define PPC_STORE_U8(address, value) \
    ::xeo3::StoreU8( \
        base, \
        static_cast<std::uint32_t>(address), \
        static_cast<std::uint8_t>(value))

#define PPC_STORE_U32(address, value) \
    ::xeo3::StoreU32( \
        ctx, \
        base, \
        static_cast<std::uint32_t>(address), \
        static_cast<std::uint32_t>(value))

#define PPC_STORE_U64(address, value) \
    ::xeo3::StoreU64( \
        base, \
        static_cast<std::uint32_t>(address), \
        static_cast<std::uint64_t>(value))

#define PPC_CALL_INDIRECT_FUNC(address) \
    ::xeo3::CallIndirect(ctx, base, static_cast<std::uint32_t>(address))

#define PPC_PUBLISH_STACK_POINTER() \
    ::xeo3::PublishStackPointer(ctx, base)

#if defined(XEO3_CONTINUOUS_STATE_PUBLICATION)
#define XEO3_PUBLISH_GUEST_IAR() \
    ::xeo3::PublishIarValue( \
        ctx.xeo3CpuState, \
        ctx.xeo3GuestIar)
#else
#define XEO3_PUBLISH_GUEST_IAR() static_cast<void>(0)
#endif

#define PPC_SET_GUEST_IAR(address) \
    do \
    { \
        ctx.xeo3GuestIar = static_cast<std::uint32_t>(address); \
        XEO3_PUBLISH_GUEST_IAR(); \
        if constexpr (::xeo3::HasGuestIarHook( \
                          static_cast<std::uint32_t>(address))) \
        { \
            ::xeo3::TrackGuestIar( \
                ctx.xeo3GuestIar, \
                ctx.xeo3GuestMemory, \
                ctx.r3.u32, \
                ctx.r9.u32, \
                ctx.r10.u32, \
                ctx.r29.u32, \
                ctx.r11.u64, \
                ctx.r13.u32, \
                ctx.r30.u64, \
                ctx.r31.u32); \
        } \
    } while (false)

#if defined(XEO3_CONTINUOUS_STATE_PUBLICATION)
#define PPC_PUBLISH_GPR(index) \
    do \
    { \
        ::xeo3::PublishGprValue( \
            ctx.xeo3CpuState, \
            index, \
            ctx.r##index.u64); \
    } while (false)

#define PPC_PUBLISH_FPR(index) \
    ::xeo3::PublishFprValue( \
        ctx.xeo3CpuState, \
        index, \
        ctx.f##index.u64)

#define PPC_PUBLISH_VECTOR(index) \
    ::xeo3::PublishVectorValue( \
        ctx.xeo3CpuState, \
        index, \
        &ctx.v##index)

#define PPC_PUBLISH_CR(index) \
    ::xeo3::PublishConditionValue( \
        ctx.xeo3CpuState, \
        index, \
        ctx.cr##index.lt, \
        ctx.cr##index.gt, \
        ctx.cr##index.eq, \
        ctx.cr##index.so)

#define PPC_PUBLISH_LR() \
    ::xeo3::PublishLrValue(ctx.xeo3CpuState, ctx.lr)

#define PPC_PUBLISH_CTR() \
    ::xeo3::PublishCtrValue(ctx.xeo3CpuState, ctx.ctr.u64)

#define PPC_PUBLISH_XER() \
    ::xeo3::PublishXerValue( \
        ctx.xeo3CpuState, \
        ctx.xer.so != 0, \
        ctx.xer.ov != 0, \
        ctx.xer.ca != 0)

#define PPC_PUBLISH_FPSCR() \
    ::xeo3::PublishFpscrValue( \
        ctx.xeo3CpuState, \
        ctx.fpscr.csr, \
        ctx.fpscr.loadFromHost())

#define PPC_PUBLISH_VSCR() \
    ::xeo3::PublishVscrValue(ctx.xeo3CpuState, ctx.vscr)

#define PPC_PUBLISH_MSR() \
    ::xeo3::PublishMsrValue(ctx.xeo3CpuState, ctx.msr)
#else
#define PPC_PUBLISH_GPR(index) static_cast<void>(0)
#define PPC_PUBLISH_FPR(index) static_cast<void>(0)
#define PPC_PUBLISH_VECTOR(index) static_cast<void>(0)
#define PPC_PUBLISH_CR(index) static_cast<void>(0)
#define PPC_PUBLISH_LR() static_cast<void>(0)
#define PPC_PUBLISH_CTR() static_cast<void>(0)
#define PPC_PUBLISH_XER() static_cast<void>(0)
#define PPC_PUBLISH_FPSCR() static_cast<void>(0)
#define PPC_PUBLISH_VSCR() static_cast<void>(0)
#define PPC_PUBLISH_MSR() static_cast<void>(0)
#endif

#define PPC_MM_LOAD_U32(address) \
    ::xeo3::MmioLoad32(ctx, base, static_cast<std::uint32_t>(address))

#define PPC_MM_STORE_U32(address, value) \
    ::xeo3::MmioStore32( \
        ctx, \
        base, \
        static_cast<std::uint32_t>(address), \
        static_cast<std::uint32_t>(value))

#define PPC_READ_TIME_BASE() \
    ::xeo3::ReadTimeBase(ctx, base)

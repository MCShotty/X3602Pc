#pragma once

#include <cstdint>

struct PPCContext;

extern "C"
{
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationInstallCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationInstallFailureCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationRestoreCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationRestoreFailureCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationHitCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationNativeCallCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationNativeReturnCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEpilogueCallCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEpilogueReturnCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEnsureCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationAttemptCount;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationThrottleCount;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastIar;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastFailure;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationFingerprintMask;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationHostFingerprintMask;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationNativeDispatchMask;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationNativeDirectMask;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastCr6;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastPath;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastResult;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastThreadId;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastStage;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastEntryR9;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastBugCheckCode;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastLr;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastStackPointer;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastDispatchBase;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastSlot;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastOriginalTarget;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastGuestMemory;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationKernelModuleBase;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationNativeBugCheckExHost;
}

namespace xeo3::kernel
{
constexpr std::uint32_t kContinuation8005F0B4 = 0x8005F0B4U;
constexpr std::uint32_t kKeBugCheckEntry = 0x8005F0B8U;
constexpr std::uint32_t kNativeBugCheckExTarget = 0x8005EEC8U;

enum class FingerprintResult : std::uint32_t
{
    pending,
    match,
    mismatch,
};

enum class InstallResult : std::uint32_t
{
    pending,
    installed,
    alreadyInstalled,
    fatal,
};

enum class FailureReason : std::uint32_t
{
    none,
    notInitialized,
    invalidArgument,
    dispatchBaseMissing,
    kernelNotReady,
    kernelFingerprintMismatch,
    dispatchSlotUnreadable,
    nativeTargetMissing,
    nativeTargetNotExecutable,
    unexpectedSlotTarget,
    recordCapacityExceeded,
    protectFailed,
    atomicReplaceFailed,
    protectionRestoreFailed,
    cleanupSlotChanged,
    cleanupProtectFailed,
    cleanupAtomicRestoreFailed,
    cleanupProtectionRestoreFailed,
    executorMissing,
    nativeHostProfileMismatch,
};

using ContinuationFunction =
    void(PPCContext& context, std::uint8_t* guestMemory);
using ExecuteMappedGuest = void (*)(
    PPCContext& context,
    std::uint8_t* guestMemory,
    std::uint32_t sourceIar,
    std::uint32_t targetIar) noexcept;
using FingerprintValidator = FingerprintResult (*)(
    const std::uint8_t* guestMemory,
    std::uint32_t& matchedMask) noexcept;
using NativeHostResolver = FingerprintResult (*)(
    void*& bugCheckExHost,
    std::uint32_t& matchedMask) noexcept;

bool Initialize(
    ExecuteMappedGuest executeMappedGuest,
    void* aotThunk,
    void* expectedUnmappedTarget = nullptr,
    FingerprintValidator fingerprintValidator = nullptr,
    NativeHostResolver nativeHostResolver = nullptr) noexcept;

InstallResult EnsureInstalled(
    void* cpuState,
    std::uint8_t* guestMemory) noexcept;

ContinuationFunction* FindContinuation(std::uint32_t guestIar) noexcept;

void* NativeHostTarget(std::uint32_t guestIar) noexcept;

void Shutdown() noexcept;

namespace detail
{
FingerprintResult ValidatePinnedKernel(
    const std::uint8_t* guestMemory,
    std::uint32_t& matchedMask) noexcept;
FingerprintResult ValidatePinnedKernelFile(
    const wchar_t* kernelPath,
    std::uint32_t& matchedMask) noexcept;
FingerprintResult ResolvePinnedNativeHost(
    void*& bugCheckExHost,
    std::uint32_t& matchedMask) noexcept;
}
}

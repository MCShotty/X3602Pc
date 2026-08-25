#include "xeo3_bridge/xenonrecomp_overrides.h"
#include "xeo3_bridge/xeo3_kernel_continuations.h"

#include "ppc_recomp_shared.h"

#include <Windows.h>
#include <bcrypt.h>
#include <intrin.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>
#include <iterator>
#include <limits>

#pragma comment(lib, "bcrypt.lib")

extern "C"
{
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationInstallCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationInstallFailureCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationRestoreCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationRestoreFailureCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationHitCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationNativeCallCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationNativeReturnCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEpilogueCallCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEpilogueReturnCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationEnsureCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationAttemptCount = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationThrottleCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastIar = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastFailure = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationFingerprintMask = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationHostFingerprintMask = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationNativeDispatchMask = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationNativeDirectMask = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastCr6 = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastPath = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastResult = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastThreadId = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeKernelContinuationLastStage = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastEntryR9 = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastBugCheckCode = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastLr = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeKernelContinuationLastStackPointer = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastDispatchBase = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastSlot = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastOriginalTarget = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationLastGuestMemory = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationKernelModuleBase = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeKernelContinuationNativeBugCheckExHost = 0;
}

namespace
{
constexpr std::size_t kMaximumDispatchBases = 64;
constexpr std::ptrdiff_t kDispatchBaseOffset = -0x10;
constexpr ULONGLONG kPendingRetryIntervalMilliseconds = 100;
constexpr std::uint32_t kKernelImageBase = 0x80040000U;
constexpr std::uint32_t kBugCheckExBranchIar = 0x8005F0C8U;
constexpr wchar_t kKernelAotModuleName[] =
    L"xeo3_5fb3687c_001748c4.dll";
constexpr std::uint32_t kKernelAotTimestamp = 0x6A588000U;
constexpr std::uint32_t kKernelAotImageSize = 0x0013E000U;
constexpr std::uint32_t kKernelBugCheckExHostRva = 0x00021899U;
constexpr std::size_t kPinnedKernelFileSize = 1507328;
constexpr wchar_t kPinnedKernelRelativePath[] =
    L"\\Flash\\xboxkrnlcf.bin";

constexpr std::uint32_t kFingerprintBugCheckWrapper = 1U << 0;
constexpr std::uint32_t kFingerprintBugCheckEx = 1U << 1;
constexpr std::uint32_t kAllFingerprints =
    kFingerprintBugCheckWrapper |
    kFingerprintBugCheckEx;
constexpr std::uint32_t kHostFingerprintImage = 1U << 0;
constexpr std::uint32_t kHostFingerprintBugCheckEx = 1U << 1;
constexpr std::uint32_t kAllHostFingerprints =
    kHostFingerprintImage |
    kHostFingerprintBugCheckEx;
constexpr std::uint32_t kNativeBugCheckExMask = 1U << 0;

constexpr std::array<std::uint8_t, 32> kKernelBugCheckExHostSha256{
    0x7E, 0x10, 0x6F, 0x74, 0xFB, 0x12, 0x8E, 0x76,
    0xC6, 0x3F, 0x5C, 0x21, 0xCE, 0xC5, 0x74, 0xBD,
    0x04, 0x0A, 0xE9, 0xA5, 0xF2, 0x13, 0xD7, 0x18,
    0xAD, 0x87, 0x1E, 0xD8, 0xB3, 0xD3, 0xB1, 0x5C,
};
constexpr std::array<std::uint8_t, 32> kPinnedKernelFileSha256{
    0xDA, 0x5B, 0xE6, 0x14, 0xFB, 0x51, 0xB5, 0x80,
    0x9D, 0x70, 0xDA, 0x07, 0x3F, 0x40, 0x6F, 0x07,
    0x1E, 0x5C, 0xCB, 0x1F, 0x8C, 0x0E, 0xBC, 0xD5,
    0x7D, 0xAF, 0xBA, 0xB3, 0x1B, 0x51, 0x9B, 0xDD,
};

struct Fingerprint
{
    std::uint32_t guestAddress;
    std::size_t byteCount;
    std::uint32_t mask;
    std::array<std::uint8_t, 32> sha256;
};

constexpr std::array<Fingerprint, 2> kFingerprints{{
    {
        0x8005F0B0U,
        0xC0,
        kFingerprintBugCheckWrapper,
        {
            0xE1, 0x32, 0x10, 0x53, 0xB7, 0xD3, 0x57, 0xFC,
            0x2D, 0xA1, 0x30, 0xD2, 0x9A, 0xA1, 0x12, 0x7A,
            0x36, 0x41, 0xC2, 0x17, 0x95, 0x94, 0xB9, 0x80,
            0xAC, 0x48, 0x9E, 0x42, 0x15, 0x9C, 0xB0, 0x2B,
        },
    },
    {
        xeo3::kernel::kNativeBugCheckExTarget,
        0x40,
        kFingerprintBugCheckEx,
        {
            0xAB, 0xCE, 0x00, 0xC5, 0x93, 0xE3, 0x4A, 0xF3,
            0xA2, 0x2E, 0xDD, 0x4D, 0xE0, 0xF6, 0xBD, 0xA0,
            0xBB, 0x30, 0x42, 0xF8, 0x17, 0xF6, 0x72, 0x80,
            0x91, 0xAE, 0x35, 0xBE, 0x99, 0x1B, 0xB0, 0x09,
        },
    },
}};

struct DispatchPatchRecord
{
    std::uintptr_t dispatchBase = 0;
    void** slot = nullptr;
    void* originalTarget = nullptr;
};

struct FastDispatchPatchRecord
{
    std::atomic<std::uintptr_t> dispatchBase{0};
    std::atomic<void**> slot{nullptr};
};

std::array<DispatchPatchRecord, kMaximumDispatchBases> g_patchRecords{};
std::size_t g_patchRecordCount = 0;
std::array<FastDispatchPatchRecord, kMaximumDispatchBases>
    g_fastPatchRecords{};
std::atomic<std::size_t> g_fastPatchRecordCount{0};
SRWLOCK g_patchLock = SRWLOCK_INIT;
std::atomic_flag g_installAttemptActive = ATOMIC_FLAG_INIT;
std::atomic<std::uintptr_t> g_pendingDispatchBase{0};
std::atomic<ULONGLONG> g_nextPendingRetryTick{0};
std::atomic<xeo3::kernel::ExecuteMappedGuest> g_executeMappedGuest{nullptr};
std::atomic<void*> g_aotThunk{nullptr};
std::atomic<void*> g_expectedUnmappedTarget{nullptr};
std::atomic<xeo3::kernel::FingerprintValidator>
    g_fingerprintValidator{nullptr};
std::atomic<xeo3::kernel::NativeHostResolver>
    g_nativeHostResolver{nullptr};
std::atomic<void*> g_nativeBugCheckExHost{nullptr};
std::atomic<bool> g_initialized{false};

void SetFailure(const xeo3::kernel::FailureReason reason) noexcept
{
    BridgeKernelContinuationLastFailure =
        static_cast<std::uint32_t>(reason);
}

class InstallAttemptGuard
{
public:
    InstallAttemptGuard() noexcept
        : acquired_(
              !g_installAttemptActive.test_and_set(
                  std::memory_order_acquire))
    {
    }

    ~InstallAttemptGuard()
    {
        if (acquired_)
        {
            g_installAttemptActive.clear(std::memory_order_release);
        }
    }

    explicit operator bool() const noexcept
    {
        return acquired_;
    }

    InstallAttemptGuard(const InstallAttemptGuard&) = delete;
    InstallAttemptGuard& operator=(const InstallAttemptGuard&) = delete;

private:
    bool acquired_ = false;
};

bool RetryIsDeferred(const std::uintptr_t dispatchBase) noexcept
{
    if (g_pendingDispatchBase.load(std::memory_order_acquire) !=
        dispatchBase)
    {
        return false;
    }

    return GetTickCount64() <
        g_nextPendingRetryTick.load(std::memory_order_acquire);
}

void DeferRetry(const std::uintptr_t dispatchBase) noexcept
{
    g_nextPendingRetryTick.store(
        GetTickCount64() + kPendingRetryIntervalMilliseconds,
        std::memory_order_relaxed);
    g_pendingDispatchBase.store(dispatchBase, std::memory_order_release);
}

void ClearDeferredRetry(const std::uintptr_t dispatchBase) noexcept
{
    auto expected = dispatchBase;
    if (g_pendingDispatchBase.compare_exchange_strong(
            expected,
            0,
            std::memory_order_acq_rel))
    {
        g_nextPendingRetryTick.store(0, std::memory_order_release);
    }
}

xeo3::kernel::InstallResult TryFastInstalledLookup(
    const std::uintptr_t dispatchBase,
    const void* const thunk) noexcept
{
    const auto count = (std::min)(
        g_fastPatchRecordCount.load(std::memory_order_acquire),
        g_fastPatchRecords.size());
    for (std::size_t index = 0; index < count; ++index)
    {
        if (g_fastPatchRecords[index].dispatchBase.load(
                std::memory_order_acquire) != dispatchBase)
        {
            continue;
        }

        auto** const slot =
            g_fastPatchRecords[index].slot.load(std::memory_order_acquire);
        if (slot == nullptr)
        {
            BridgeKernelContinuationInstallFailureCount =
                BridgeKernelContinuationInstallFailureCount + 1;
            SetFailure(xeo3::kernel::FailureReason::dispatchSlotUnreadable);
            return xeo3::kernel::InstallResult::fatal;
        }

        std::atomic_ref<void*> currentTarget(*slot);
        if (currentTarget.load(std::memory_order_acquire) != thunk)
        {
            BridgeKernelContinuationInstallFailureCount =
                BridgeKernelContinuationInstallFailureCount + 1;
            SetFailure(xeo3::kernel::FailureReason::unexpectedSlotTarget);
            return xeo3::kernel::InstallResult::fatal;
        }

        SetFailure(xeo3::kernel::FailureReason::none);
        return xeo3::kernel::InstallResult::alreadyInstalled;
    }

    return xeo3::kernel::InstallResult::pending;
}

void PublishFastInstalledRecord(
    const std::size_t index,
    const DispatchPatchRecord& record) noexcept
{
    g_fastPatchRecords[index].slot.store(
        record.slot,
        std::memory_order_relaxed);
    g_fastPatchRecords[index].dispatchBase.store(
        record.dispatchBase,
        std::memory_order_release);
    g_fastPatchRecordCount.store(index + 1, std::memory_order_release);
    ClearDeferredRetry(record.dispatchBase);
}

void EmitKernelEvent(
    const char* const action,
    const std::uintptr_t dispatchBase,
    const std::uintptr_t slot,
    const std::uintptr_t value,
    const std::uint32_t detail) noexcept
{
    std::array<char, 320> buffer{};
    const auto length = std::snprintf(
        buffer.data(),
        buffer.size(),
        "[xeo3-ac6] kernel_continuation action=%s tid=%lu "
        "iar=%08X dispatch=%p slot=%p value=%p detail=%u\n",
        action,
        static_cast<unsigned long>(GetCurrentThreadId()),
        xeo3::kernel::kContinuation8005F0B4,
        reinterpret_cast<void*>(dispatchBase),
        reinterpret_cast<void*>(slot),
        reinterpret_cast<void*>(value),
        detail);
    if (length > 0)
    {
        OutputDebugStringA(buffer.data());
    }
}

bool IsReadableProtection(const DWORD protection) noexcept
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
    {
        return false;
    }
    switch (protection & 0xFFU)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsExecutableProtection(const DWORD protection) noexcept
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
    {
        return false;
    }
    switch (protection & 0xFFU)
    {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsReadableRange(
    const void* const address,
    const std::size_t byteCount) noexcept
{
    if (address == nullptr || byteCount == 0)
    {
        return false;
    }

    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor >
        (std::numeric_limits<std::uintptr_t>::max)() - byteCount)
    {
        return false;
    }
    const auto end = cursor + byteCount;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(cursor),
                &information,
                sizeof(information)) != sizeof(information) ||
            information.State != MEM_COMMIT ||
            !IsReadableProtection(information.Protect))
        {
            return false;
        }
        const auto regionEnd =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
            information.RegionSize;
        if (regionEnd <= cursor)
        {
            return false;
        }
        cursor = regionEnd < end ? regionEnd : end;
    }
    return true;
}

bool IsExecutableAddress(const void* const address) noexcept
{
    MEMORY_BASIC_INFORMATION information{};
    return address != nullptr &&
        VirtualQuery(address, &information, sizeof(information)) ==
            sizeof(information) &&
        information.State == MEM_COMMIT &&
        IsExecutableProtection(information.Protect);
}

void** CalculateSlot(
    const std::uintptr_t dispatchBase,
    const std::uint32_t guestIar) noexcept
{
    const auto offset = static_cast<std::uintptr_t>(guestIar) * 2U;
    if (dispatchBase >
        (std::numeric_limits<std::uintptr_t>::max)() - offset)
    {
        return nullptr;
    }
    const auto address = dispatchBase + offset;
    if ((address & (alignof(void*) - 1U)) != 0)
    {
        return nullptr;
    }
    return reinterpret_cast<void**>(address);
}

bool ReadSlot(void** const slot, void*& value) noexcept
{
    if (!IsReadableRange(slot, sizeof(*slot)))
    {
        return false;
    }
    std::memcpy(&value, slot, sizeof(value));
    return true;
}

bool HashBytesSha256(
    const void* const data,
    const std::size_t byteCount,
    std::array<std::uint8_t, 32>& digest) noexcept
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<std::uint8_t, 512> hashObject{};
    DWORD objectSize = 0;
    DWORD bytesWritten = 0;
    bool succeeded = false;

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
            0) < 0 ||
        objectSize > hashObject.size() ||
        BCryptCreateHash(
            algorithm,
            &hash,
            hashObject.data(),
            objectSize,
            nullptr,
            0,
            0) < 0 ||
        byteCount > (std::numeric_limits<ULONG>::max)() ||
        BCryptHashData(
            hash,
            static_cast<PUCHAR>(const_cast<void*>(data)),
            static_cast<ULONG>(byteCount),
            0) < 0)
    {
        goto cleanup;
    }

    succeeded = BCryptFinishHash(
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
    return succeeded;
}

std::uint32_t PackCr6(const PPCCRRegister& cr6) noexcept
{
    return
        static_cast<std::uint32_t>(cr6.lt) |
        (static_cast<std::uint32_t>(cr6.gt) << 8U) |
        (static_cast<std::uint32_t>(cr6.eq) << 16U) |
        (static_cast<std::uint32_t>(cr6.so) << 24U);
}

[[noreturn]] void FailMissingExecutor() noexcept
{
    SetFailure(xeo3::kernel::FailureReason::executorMissing);
    EmitKernelEvent(
        "executor_missing",
        BridgeKernelContinuationLastDispatchBase,
        BridgeKernelContinuationLastSlot,
        0,
        BridgeKernelContinuationLastFailure);
    __fastfail(FAST_FAIL_FATAL_APP_EXIT);
}

void ExecuteContinuation8005F0B4(
    PPCContext& context,
    std::uint8_t* const guestMemory)
{
    BridgeKernelContinuationHitCount =
        BridgeKernelContinuationHitCount + 1;
    BridgeKernelContinuationLastIar =
        xeo3::kernel::kContinuation8005F0B4;
    BridgeKernelContinuationLastThreadId = GetCurrentThreadId();
    BridgeKernelContinuationLastStage = 1;
    BridgeKernelContinuationLastEntryR9 = context.r9.u64;
    BridgeKernelContinuationLastBugCheckCode = context.r3.u64;
    BridgeKernelContinuationLastGuestMemory =
        reinterpret_cast<std::uintptr_t>(guestMemory);
    BridgeKernelContinuationLastCr6 = PackCr6(context.cr6);
    BridgeKernelContinuationLastLr = context.lr;
    BridgeKernelContinuationLastStackPointer = context.r1.u64;
    BridgeKernelContinuationLastPath = 3;

    // KeBugCheck is a five-instruction tail wrapper: it zeroes the four
    // extended arguments and branches to KeBugCheckEx without changing LR.
    context.r4.u64 = 0;
    context.r5.u64 = 0;
    context.r6.u64 = 0;
    context.r7.u64 = 0;

    const auto executor =
        g_executeMappedGuest.load(std::memory_order_acquire);
    if (executor == nullptr)
    {
        FailMissingExecutor();
    }

    BridgeKernelContinuationLastStage = 2;
    BridgeKernelContinuationNativeCallCount =
        BridgeKernelContinuationNativeCallCount + 1;
    executor(
        context,
        guestMemory,
        kBugCheckExBranchIar,
        xeo3::kernel::kNativeBugCheckExTarget);
    BridgeKernelContinuationNativeReturnCount =
        BridgeKernelContinuationNativeReturnCount + 1;
    BridgeKernelContinuationLastStage = 3;
    BridgeKernelContinuationLastResult = context.r3.u32;
    BridgeKernelContinuationLastLr = context.lr;
    BridgeKernelContinuationLastStackPointer = context.r1.u64;
}

struct ExclusiveSrwLock
{
    explicit ExclusiveSrwLock(SRWLOCK& lock) noexcept : lock_(lock)
    {
        AcquireSRWLockExclusive(&lock_);
    }

    ~ExclusiveSrwLock()
    {
        ReleaseSRWLockExclusive(&lock_);
    }

    ExclusiveSrwLock(const ExclusiveSrwLock&) = delete;
    ExclusiveSrwLock& operator=(const ExclusiveSrwLock&) = delete;

private:
    SRWLOCK& lock_;
};
}

namespace xeo3::kernel
{
namespace detail
{
FingerprintResult ValidatePinnedKernelFile(
    const wchar_t* const kernelPath,
    std::uint32_t& matchedMask) noexcept
{
    matchedMask = 0;
    if (kernelPath == nullptr || *kernelPath == L'\0')
    {
        return FingerprintResult::mismatch;
    }

    const auto file = CreateFileW(
        kernelPath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return FingerprintResult::mismatch;
    }

    LARGE_INTEGER fileSize{};
    const auto sizeMatches =
        GetFileSizeEx(file, &fileSize) != FALSE &&
        fileSize.QuadPart ==
            static_cast<LONGLONG>(kPinnedKernelFileSize);
    const auto mapping = sizeMatches
        ? CreateFileMappingW(
              file,
              nullptr,
              PAGE_READONLY,
              0,
              0,
              nullptr)
        : nullptr;
    const auto* const bytes = mapping == nullptr
        ? nullptr
        : static_cast<const std::uint8_t*>(
              MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));

    FingerprintResult result = FingerprintResult::mismatch;
    std::array<std::uint8_t, 32> digest{};
    if (bytes != nullptr &&
        HashBytesSha256(
            bytes,
            kPinnedKernelFileSize,
            digest) &&
        digest == kPinnedKernelFileSha256)
    {
        result = FingerprintResult::match;
    }

    for (const auto& fingerprint : kFingerprints)
    {
        if (fingerprint.guestAddress < kKernelImageBase)
        {
            result = FingerprintResult::mismatch;
            break;
        }
        const auto fileOffset =
            static_cast<std::size_t>(
                fingerprint.guestAddress - kKernelImageBase);
        if (result != FingerprintResult::match ||
            fileOffset >
                kPinnedKernelFileSize - fingerprint.byteCount)
        {
            result = FingerprintResult::mismatch;
            break;
        }

        if (!HashBytesSha256(
                bytes + fileOffset,
                fingerprint.byteCount,
                digest) ||
            digest != fingerprint.sha256)
        {
            result = FingerprintResult::mismatch;
            break;
        }
        matchedMask |= fingerprint.mask;
    }

    if (bytes != nullptr)
    {
        UnmapViewOfFile(bytes);
    }
    if (mapping != nullptr)
    {
        CloseHandle(mapping);
    }
    CloseHandle(file);

    return result == FingerprintResult::match &&
            matchedMask == kAllFingerprints
        ? FingerprintResult::match
        : FingerprintResult::mismatch;
}

FingerprintResult ValidatePinnedKernel(
    const std::uint8_t* const guestMemory,
    std::uint32_t& matchedMask) noexcept
{
    matchedMask = 0;
    if (guestMemory == nullptr)
    {
        return FingerprintResult::pending;
    }

    HMODULE ownModule = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(
                const_cast<std::uint32_t*>(
                    &BridgeKernelContinuationFingerprintMask)),
            &ownModule) ||
        ownModule == nullptr)
    {
        return FingerprintResult::mismatch;
    }

    std::array<wchar_t, 32768> modulePath{};
    const auto pathLength = GetModuleFileNameW(
        ownModule,
        modulePath.data(),
        static_cast<DWORD>(modulePath.size()));
    if (pathLength == 0 || pathLength >= modulePath.size())
    {
        return FingerprintResult::mismatch;
    }

    auto* const separator =
        std::wcsrchr(modulePath.data(), L'\\');
    if (separator == nullptr)
    {
        return FingerprintResult::mismatch;
    }
    const auto directoryLength =
        static_cast<std::size_t>(separator - modulePath.data());
    if (directoryLength +
            std::size(kPinnedKernelRelativePath) >
        modulePath.size())
    {
        return FingerprintResult::mismatch;
    }
    std::wmemcpy(
        modulePath.data() + directoryLength,
        kPinnedKernelRelativePath,
        std::size(kPinnedKernelRelativePath));

    std::uint32_t fileMask = 0;
    const auto fileResult = ValidatePinnedKernelFile(
        modulePath.data(),
        fileMask);
    if (fileResult != FingerprintResult::match)
    {
        return fileResult;
    }

    std::array<std::uint8_t, 32> digest{};
    for (const auto& fingerprint : kFingerprints)
    {
        const auto* const liveBytes =
            guestMemory + fingerprint.guestAddress;
        if (!IsReadableRange(liveBytes, fingerprint.byteCount))
        {
            matchedMask = 0;
            return FingerprintResult::pending;
        }
        if (!HashBytesSha256(
                liveBytes,
                fingerprint.byteCount,
                digest) ||
            digest != fingerprint.sha256)
        {
            matchedMask = 0;
            return FingerprintResult::mismatch;
        }
        matchedMask |= fingerprint.mask;
    }

    return matchedMask == kAllFingerprints &&
            fileMask == kAllFingerprints
        ? FingerprintResult::match
        : FingerprintResult::mismatch;
}

FingerprintResult ResolvePinnedNativeHost(
    void*& bugCheckExHost,
    std::uint32_t& matchedMask) noexcept
{
    bugCheckExHost = nullptr;
    matchedMask = 0;

    const auto module = GetModuleHandleW(kKernelAotModuleName);
    if (module == nullptr)
    {
        BridgeKernelContinuationKernelModuleBase = 0;
        return FingerprintResult::pending;
    }

    auto* const imageBase =
        reinterpret_cast<std::uint8_t*>(module);
    BridgeKernelContinuationKernelModuleBase =
        reinterpret_cast<std::uintptr_t>(imageBase);
    if (!IsReadableRange(imageBase, sizeof(IMAGE_DOS_HEADER)))
    {
        return FingerprintResult::mismatch;
    }

    const auto* const dosHeader =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE ||
        dosHeader->e_lfanew < static_cast<LONG>(sizeof(*dosHeader)) ||
        dosHeader->e_lfanew > 0x1000)
    {
        return FingerprintResult::mismatch;
    }

    const auto* const ntHeaders =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            imageBase + dosHeader->e_lfanew);
    if (!IsReadableRange(ntHeaders, sizeof(*ntHeaders)) ||
        ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        ntHeaders->FileHeader.TimeDateStamp != kKernelAotTimestamp ||
        ntHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        ntHeaders->OptionalHeader.SizeOfImage != kKernelAotImageSize)
    {
        return FingerprintResult::mismatch;
    }
    matchedMask |= kHostFingerprintImage;

    auto* const host =
        imageBase + kKernelBugCheckExHostRva;
    if (!IsReadableRange(host, 0x40))
    {
        return FingerprintResult::mismatch;
    }

    std::array<std::uint8_t, 32> digest{};
    if (!HashBytesSha256(host, 0x40, digest) ||
        digest != kKernelBugCheckExHostSha256)
    {
        return FingerprintResult::mismatch;
    }
    matchedMask |= kHostFingerprintBugCheckEx;

    if (!IsExecutableAddress(host))
    {
        return FingerprintResult::mismatch;
    }

    bugCheckExHost = host;
    BridgeKernelContinuationNativeBugCheckExHost =
        reinterpret_cast<std::uintptr_t>(host);
    return matchedMask == kAllHostFingerprints
        ? FingerprintResult::match
        : FingerprintResult::mismatch;
}
}

bool Initialize(
    const ExecuteMappedGuest executeMappedGuest,
    void* const aotThunk,
    void* const expectedUnmappedTarget,
    const FingerprintValidator fingerprintValidator,
    const NativeHostResolver nativeHostResolver) noexcept
{
    if (executeMappedGuest == nullptr || aotThunk == nullptr)
    {
        SetFailure(FailureReason::invalidArgument);
        return false;
    }

    ExclusiveSrwLock lock(g_patchLock);
    if (g_patchRecordCount != 0)
    {
        SetFailure(FailureReason::cleanupSlotChanged);
        return false;
    }

    BridgeKernelContinuationInstallCount = 0;
    BridgeKernelContinuationInstallFailureCount = 0;
    BridgeKernelContinuationRestoreCount = 0;
    BridgeKernelContinuationRestoreFailureCount = 0;
    BridgeKernelContinuationHitCount = 0;
    BridgeKernelContinuationNativeCallCount = 0;
    BridgeKernelContinuationNativeReturnCount = 0;
    BridgeKernelContinuationEpilogueCallCount = 0;
    BridgeKernelContinuationEpilogueReturnCount = 0;
    BridgeKernelContinuationEnsureCount = 0;
    BridgeKernelContinuationAttemptCount = 0;
    BridgeKernelContinuationThrottleCount = 0;
    BridgeKernelContinuationLastIar = 0;
    BridgeKernelContinuationLastFailure = 0;
    BridgeKernelContinuationFingerprintMask = 0;
    BridgeKernelContinuationHostFingerprintMask = 0;
    BridgeKernelContinuationNativeDispatchMask = 0;
    BridgeKernelContinuationNativeDirectMask = 0;
    BridgeKernelContinuationLastCr6 = 0;
    BridgeKernelContinuationLastPath = 0;
    BridgeKernelContinuationLastResult = 0;
    BridgeKernelContinuationLastThreadId = 0;
    BridgeKernelContinuationLastStage = 0;
    BridgeKernelContinuationLastEntryR9 = 0;
    BridgeKernelContinuationLastBugCheckCode = 0;
    BridgeKernelContinuationLastLr = 0;
    BridgeKernelContinuationLastStackPointer = 0;
    BridgeKernelContinuationLastDispatchBase = 0;
    BridgeKernelContinuationLastSlot = 0;
    BridgeKernelContinuationLastOriginalTarget = 0;
    BridgeKernelContinuationLastGuestMemory = 0;
    BridgeKernelContinuationKernelModuleBase = 0;
    BridgeKernelContinuationNativeBugCheckExHost = 0;
    for (auto& record : g_fastPatchRecords)
    {
        record.dispatchBase.store(0, std::memory_order_relaxed);
        record.slot.store(nullptr, std::memory_order_relaxed);
    }
    g_fastPatchRecordCount.store(0, std::memory_order_release);
    g_pendingDispatchBase.store(0, std::memory_order_release);
    g_nextPendingRetryTick.store(0, std::memory_order_release);
    g_installAttemptActive.clear(std::memory_order_release);

    g_executeMappedGuest.store(
        executeMappedGuest,
        std::memory_order_release);
    g_aotThunk.store(aotThunk, std::memory_order_release);
    g_expectedUnmappedTarget.store(
        expectedUnmappedTarget,
        std::memory_order_release);
    g_fingerprintValidator.store(
        fingerprintValidator == nullptr
            ? &detail::ValidatePinnedKernel
            : fingerprintValidator,
        std::memory_order_release);
    g_nativeHostResolver.store(
        nativeHostResolver == nullptr
            ? &detail::ResolvePinnedNativeHost
            : nativeHostResolver,
        std::memory_order_release);
    g_nativeBugCheckExHost.store(nullptr, std::memory_order_release);
    g_initialized.store(true, std::memory_order_release);
    return true;
}

InstallResult EnsureInstalled(
    void* const cpuState,
    std::uint8_t* const guestMemory) noexcept
{
    BridgeKernelContinuationEnsureCount =
        BridgeKernelContinuationEnsureCount + 1;
    if (!g_initialized.load(std::memory_order_acquire))
    {
        SetFailure(FailureReason::notInitialized);
        return InstallResult::fatal;
    }
    if (cpuState == nullptr || guestMemory == nullptr)
    {
        SetFailure(FailureReason::invalidArgument);
        return InstallResult::fatal;
    }

    std::uintptr_t dispatchBase = 0;
    std::memcpy(
        &dispatchBase,
        static_cast<const std::uint8_t*>(cpuState) +
            kDispatchBaseOffset,
        sizeof(dispatchBase));
    BridgeKernelContinuationLastDispatchBase = dispatchBase;
    BridgeKernelContinuationLastGuestMemory =
        reinterpret_cast<std::uintptr_t>(guestMemory);
    if (dispatchBase == 0)
    {
        SetFailure(FailureReason::dispatchBaseMissing);
        return InstallResult::pending;
    }

    const auto thunk = g_aotThunk.load(std::memory_order_acquire);
    const auto fastResult = TryFastInstalledLookup(dispatchBase, thunk);
    if (fastResult != InstallResult::pending)
    {
        return fastResult;
    }
    if (RetryIsDeferred(dispatchBase))
    {
        BridgeKernelContinuationThrottleCount =
            BridgeKernelContinuationThrottleCount + 1;
        return InstallResult::pending;
    }

    InstallAttemptGuard attempt;
    if (!attempt)
    {
        BridgeKernelContinuationThrottleCount =
            BridgeKernelContinuationThrottleCount + 1;
        return InstallResult::pending;
    }
    BridgeKernelContinuationAttemptCount =
        BridgeKernelContinuationAttemptCount + 1;

    ExclusiveSrwLock lock(g_patchLock);
    const auto lockedFastResult =
        TryFastInstalledLookup(dispatchBase, thunk);
    if (lockedFastResult != InstallResult::pending)
    {
        return lockedFastResult;
    }
    if (RetryIsDeferred(dispatchBase))
    {
        BridgeKernelContinuationThrottleCount =
            BridgeKernelContinuationThrottleCount + 1;
        return InstallResult::pending;
    }

    for (std::size_t index = 0; index < g_patchRecordCount; ++index)
    {
        if (g_patchRecords[index].dispatchBase == dispatchBase)
        {
            void* currentTarget = nullptr;
            if (!ReadSlot(g_patchRecords[index].slot, currentTarget) ||
                currentTarget != thunk)
            {
                BridgeKernelContinuationInstallFailureCount =
                    BridgeKernelContinuationInstallFailureCount + 1;
                SetFailure(FailureReason::unexpectedSlotTarget);
                EmitKernelEvent(
                    "installed_slot_changed",
                    dispatchBase,
                    reinterpret_cast<std::uintptr_t>(
                        g_patchRecords[index].slot),
                    reinterpret_cast<std::uintptr_t>(currentTarget),
                    BridgeKernelContinuationLastFailure);
                return InstallResult::fatal;
            }
            PublishFastInstalledRecord(index, g_patchRecords[index]);
            SetFailure(FailureReason::none);
            return InstallResult::alreadyInstalled;
        }
    }

    auto** const slot =
        CalculateSlot(dispatchBase, kContinuation8005F0B4);
    BridgeKernelContinuationLastSlot =
        reinterpret_cast<std::uintptr_t>(slot);
    void* originalTarget = nullptr;
    if (slot == nullptr || !ReadSlot(slot, originalTarget))
    {
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::dispatchSlotUnreadable);
        return InstallResult::fatal;
    }
    BridgeKernelContinuationLastOriginalTarget =
        reinterpret_cast<std::uintptr_t>(originalTarget);

    const auto validator =
        g_fingerprintValidator.load(std::memory_order_acquire);
    std::uint32_t fingerprintMask = 0;
    const auto fingerprintResult =
        validator == nullptr
            ? FingerprintResult::mismatch
            : validator(guestMemory, fingerprintMask);
    BridgeKernelContinuationFingerprintMask = fingerprintMask;
    if (fingerprintResult == FingerprintResult::pending)
    {
        SetFailure(FailureReason::kernelNotReady);
        DeferRetry(dispatchBase);
        return InstallResult::pending;
    }
    if (fingerprintResult != FingerprintResult::match)
    {
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::kernelFingerprintMismatch);
        EmitKernelEvent(
            "fingerprint_mismatch",
            dispatchBase,
            reinterpret_cast<std::uintptr_t>(slot),
            reinterpret_cast<std::uintptr_t>(guestMemory),
            fingerprintMask);
        return InstallResult::fatal;
    }

    auto** const nativeSlot = CalculateSlot(
        dispatchBase,
        kNativeBugCheckExTarget);
    void* nativeTarget = nullptr;
    std::uint32_t nativeDispatchMask = 0;
    if (nativeSlot != nullptr &&
        ReadSlot(nativeSlot, nativeTarget) &&
        nativeTarget != nullptr)
    {
        if (!IsExecutableAddress(nativeTarget))
        {
            BridgeKernelContinuationInstallFailureCount =
                BridgeKernelContinuationInstallFailureCount + 1;
            SetFailure(FailureReason::nativeTargetNotExecutable);
            return InstallResult::fatal;
        }
        nativeDispatchMask = kNativeBugCheckExMask;
        BridgeKernelContinuationNativeBugCheckExHost =
            reinterpret_cast<std::uintptr_t>(nativeTarget);
    }
    BridgeKernelContinuationNativeDispatchMask =
        nativeDispatchMask;

    if (nativeDispatchMask != kNativeBugCheckExMask)
    {
        void* host =
            g_nativeBugCheckExHost.load(std::memory_order_acquire);
        if (host == nullptr)
        {
            const auto resolver =
                g_nativeHostResolver.load(std::memory_order_acquire);
            std::uint32_t hostFingerprintMask = 0;
            const auto hostResult =
                resolver == nullptr
                    ? FingerprintResult::mismatch
                    : resolver(
                          host,
                          hostFingerprintMask);
            BridgeKernelContinuationHostFingerprintMask =
                hostFingerprintMask;
            if (hostResult == FingerprintResult::pending)
            {
                SetFailure(FailureReason::nativeTargetMissing);
                DeferRetry(dispatchBase);
                return InstallResult::pending;
            }
            if (hostResult != FingerprintResult::match)
            {
                BridgeKernelContinuationInstallFailureCount =
                    BridgeKernelContinuationInstallFailureCount + 1;
                SetFailure(FailureReason::nativeHostProfileMismatch);
                EmitKernelEvent(
                    "native_host_profile_mismatch",
                    dispatchBase,
                    reinterpret_cast<std::uintptr_t>(slot),
                    BridgeKernelContinuationKernelModuleBase,
                    hostFingerprintMask);
                return InstallResult::fatal;
            }
            if (!IsExecutableAddress(host))
            {
                BridgeKernelContinuationInstallFailureCount =
                    BridgeKernelContinuationInstallFailureCount + 1;
                SetFailure(FailureReason::nativeTargetNotExecutable);
                return InstallResult::fatal;
            }
            g_nativeBugCheckExHost.store(
                host,
                std::memory_order_release);
        }

        BridgeKernelContinuationNativeDirectMask =
            kNativeBugCheckExMask;
        BridgeKernelContinuationNativeBugCheckExHost =
            reinterpret_cast<std::uintptr_t>(host);
    }
    else
    {
        BridgeKernelContinuationNativeDirectMask = 0;
    }

    const auto expectedOriginal =
        g_expectedUnmappedTarget.load(std::memory_order_acquire);
    if (originalTarget == thunk)
    {
        if (g_patchRecordCount >= g_patchRecords.size())
        {
            BridgeKernelContinuationInstallFailureCount =
                BridgeKernelContinuationInstallFailureCount + 1;
            SetFailure(FailureReason::recordCapacityExceeded);
            return InstallResult::fatal;
        }
        const auto recordIndex = g_patchRecordCount++;
        g_patchRecords[recordIndex] = {
            dispatchBase,
            slot,
            expectedOriginal,
        };
        PublishFastInstalledRecord(
            recordIndex,
            g_patchRecords[recordIndex]);
        SetFailure(FailureReason::none);
        EmitKernelEvent(
            "already_installed",
            dispatchBase,
            reinterpret_cast<std::uintptr_t>(slot),
            reinterpret_cast<std::uintptr_t>(thunk),
            fingerprintMask);
        return InstallResult::alreadyInstalled;
    }
    if (originalTarget != expectedOriginal)
    {
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::unexpectedSlotTarget);
        EmitKernelEvent(
            "unexpected_slot",
            dispatchBase,
            reinterpret_cast<std::uintptr_t>(slot),
            reinterpret_cast<std::uintptr_t>(originalTarget),
            BridgeKernelContinuationLastFailure);
        return InstallResult::fatal;
    }
    if (g_patchRecordCount >= g_patchRecords.size())
    {
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::recordCapacityExceeded);
        return InstallResult::fatal;
    }

    DWORD oldProtection = 0;
    if (!VirtualProtect(
            slot,
            sizeof(*slot),
            PAGE_READWRITE,
            &oldProtection))
    {
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::protectFailed);
        return InstallResult::fatal;
    }

    auto* const observed = InterlockedCompareExchangePointer(
        reinterpret_cast<void* volatile*>(slot),
        thunk,
        expectedOriginal);
    if (observed != expectedOriginal)
    {
        DWORD ignoredProtection = 0;
        VirtualProtect(
            slot,
            sizeof(*slot),
            oldProtection,
            &ignoredProtection);
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::atomicReplaceFailed);
        return InstallResult::fatal;
    }

    DWORD ignoredProtection = 0;
    if (!VirtualProtect(
            slot,
            sizeof(*slot),
            oldProtection,
            &ignoredProtection))
    {
        InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(slot),
            expectedOriginal,
            thunk);
        DWORD rollbackProtection = 0;
        VirtualProtect(
            slot,
            sizeof(*slot),
            oldProtection,
            &rollbackProtection);
        BridgeKernelContinuationInstallFailureCount =
            BridgeKernelContinuationInstallFailureCount + 1;
        SetFailure(FailureReason::protectionRestoreFailed);
        return InstallResult::fatal;
    }

    const auto recordIndex = g_patchRecordCount++;
    g_patchRecords[recordIndex] = {
        dispatchBase,
        slot,
        expectedOriginal,
    };
    PublishFastInstalledRecord(
        recordIndex,
        g_patchRecords[recordIndex]);
    BridgeKernelContinuationInstallCount =
        BridgeKernelContinuationInstallCount + 1;
    SetFailure(FailureReason::none);
    EmitKernelEvent(
        "installed",
        dispatchBase,
        reinterpret_cast<std::uintptr_t>(slot),
        reinterpret_cast<std::uintptr_t>(thunk),
        fingerprintMask);
    return InstallResult::installed;
}

ContinuationFunction* FindContinuation(
    const std::uint32_t guestIar) noexcept
{
    return guestIar == kContinuation8005F0B4
        ? &ExecuteContinuation8005F0B4
        : nullptr;
}

void* NativeHostTarget(const std::uint32_t guestIar) noexcept
{
    if (guestIar == kNativeBugCheckExTarget)
    {
        return g_nativeBugCheckExHost.load(std::memory_order_acquire);
    }
    return nullptr;
}

void Shutdown() noexcept
{
    g_initialized.store(false, std::memory_order_release);
    g_fastPatchRecordCount.store(0, std::memory_order_release);
    ExclusiveSrwLock lock(g_patchLock);
    const auto thunk = g_aotThunk.load(std::memory_order_acquire);
    for (std::size_t remaining = g_patchRecordCount;
         remaining != 0;
         --remaining)
    {
        const auto& record = g_patchRecords[remaining - 1];
        void* currentTarget = nullptr;
        if (!ReadSlot(record.slot, currentTarget) ||
            currentTarget != thunk)
        {
            BridgeKernelContinuationRestoreFailureCount =
                BridgeKernelContinuationRestoreFailureCount + 1;
            SetFailure(FailureReason::cleanupSlotChanged);
            EmitKernelEvent(
                "restore_slot_changed",
                record.dispatchBase,
                reinterpret_cast<std::uintptr_t>(record.slot),
                reinterpret_cast<std::uintptr_t>(currentTarget),
                BridgeKernelContinuationLastFailure);
            continue;
        }

        DWORD oldProtection = 0;
        if (!VirtualProtect(
                record.slot,
                sizeof(*record.slot),
                PAGE_READWRITE,
                &oldProtection))
        {
            BridgeKernelContinuationRestoreFailureCount =
                BridgeKernelContinuationRestoreFailureCount + 1;
            SetFailure(FailureReason::cleanupProtectFailed);
            continue;
        }

        const auto observed = InterlockedCompareExchangePointer(
            reinterpret_cast<void* volatile*>(record.slot),
            record.originalTarget,
            thunk);
        if (observed != thunk)
        {
            BridgeKernelContinuationRestoreFailureCount =
                BridgeKernelContinuationRestoreFailureCount + 1;
            SetFailure(FailureReason::cleanupAtomicRestoreFailed);
        }

        DWORD ignoredProtection = 0;
        if (!VirtualProtect(
                record.slot,
                sizeof(*record.slot),
                oldProtection,
                &ignoredProtection))
        {
            BridgeKernelContinuationRestoreFailureCount =
                BridgeKernelContinuationRestoreFailureCount + 1;
            SetFailure(FailureReason::cleanupProtectionRestoreFailed);
        }
        else if (observed == thunk)
        {
            BridgeKernelContinuationRestoreCount =
                BridgeKernelContinuationRestoreCount + 1;
            EmitKernelEvent(
                "restored",
                record.dispatchBase,
                reinterpret_cast<std::uintptr_t>(record.slot),
                reinterpret_cast<std::uintptr_t>(record.originalTarget),
                0);
        }
    }

    g_patchRecords = {};
    g_patchRecordCount = 0;
    for (auto& record : g_fastPatchRecords)
    {
        record.dispatchBase.store(0, std::memory_order_relaxed);
        record.slot.store(nullptr, std::memory_order_relaxed);
    }
    g_pendingDispatchBase.store(0, std::memory_order_release);
    g_nextPendingRetryTick.store(0, std::memory_order_release);
    g_installAttemptActive.clear(std::memory_order_release);
    g_executeMappedGuest.store(nullptr, std::memory_order_release);
    g_aotThunk.store(nullptr, std::memory_order_release);
    g_expectedUnmappedTarget.store(nullptr, std::memory_order_release);
    g_fingerprintValidator.store(nullptr, std::memory_order_release);
    g_nativeHostResolver.store(nullptr, std::memory_order_release);
    g_nativeBugCheckExHost.store(nullptr, std::memory_order_release);
}
}

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

extern "C"
{
extern __declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarObserverInstalled;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarObserverFailure;
extern __declspec(dllexport) volatile std::uint64_t
    BridgeHostUnmappedIarCount;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarLastIar;
extern __declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarLastThreadId;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastCpuObject;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastCpuStateAnchor;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastGuestMemory;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastDispatchBase;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastSlot;
extern __declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastSlotTarget;
}

namespace xeo3::host
{
constexpr std::size_t kUnmappedFatalDetourSize = 16;

using HashFileSha256 = bool (*)(
    const wchar_t* path,
    std::array<std::uint8_t, 32>& digest) noexcept;

enum class ObserverFailure : std::uint32_t
{
    none,
    missingHashCallback,
    moduleMissing,
    invalidPe,
    identityMismatch,
    pathFailure,
    hashMismatch,
    fatalPathMismatch,
    targetProtectionFailure,
    targetProtectionRestoreFailure,
    detourChanged,
    differentTargetInstalled,
};

struct Observation
{
    std::uint32_t guestIar = 0;
    std::uintptr_t cpuObject = 0;
    std::uintptr_t cpuStateAnchor = 0;
    std::uintptr_t guestMemory = 0;
    std::uintptr_t dispatchBase = 0;
    std::uintptr_t slot = 0;
    std::uintptr_t slotTarget = 0;
};

bool InstallPinnedUnmappedIarObserver(HashFileSha256 hashFile) noexcept;
bool RemovePinnedUnmappedIarObserver() noexcept;

namespace detail
{
std::array<std::uint8_t, kUnmappedFatalDetourSize>
EncodeAbsoluteJump(const void* target) noexcept;

bool HasExpectedFatalPath(
    const std::uint8_t* bytes,
    std::size_t byteCount) noexcept;

Observation CaptureObservation(
    std::uint32_t guestIar,
    void* cpuObject,
    std::uint8_t* guestMemory) noexcept;

bool InstallDetour(
    std::uint8_t* target,
    const void* thunk) noexcept;

bool RemoveDetour() noexcept;
}
}

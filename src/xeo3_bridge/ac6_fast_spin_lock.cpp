#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "xeo3_bridge/ac6_fast_spin_lock.h"

#include <Windows.h>
#include <intrin.h>

#include <cstdint>

namespace
{
constexpr std::uint32_t kBlockingSpinAttempts = 256;

volatile LONG* ResolveSpinLock(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t address) noexcept
{
    if (guestMemory == nullptr || address == 0 ||
        static_cast<std::size_t>(address) > guestMemorySize ||
        sizeof(std::uint32_t) >
            guestMemorySize - static_cast<std::size_t>(address))
    {
        return nullptr;
    }

    auto* const field = guestMemory + address;
    if ((reinterpret_cast<std::uintptr_t>(field) & 3U) != 0)
    {
        return nullptr;
    }
    return reinterpret_cast<volatile LONG*>(field);
}

LONG EncodeGuestU32(const std::uint32_t value) noexcept
{
    return static_cast<LONG>(_byteswap_ulong(value));
}
}

namespace xeo3::fast_spin
{
SpinLockResult AcquireSpinLock(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t spinLockAddress,
    const AcquireMode mode) noexcept
{
    auto* const lock = ResolveSpinLock(
        guestMemory,
        guestMemorySize,
        spinLockAddress);
    if (lock == nullptr)
    {
        return {};
    }

    const auto unlocked = EncodeGuestU32(0);
    const auto locked = EncodeGuestU32(1);
    bool contended = false;
    const auto attempts = mode == AcquireMode::tryOnly
        ? 1U
        : kBlockingSpinAttempts;
    for (std::uint32_t attempt = 0; attempt < attempts; ++attempt)
    {
        if (InterlockedCompareExchange(lock, locked, unlocked) == unlocked)
        {
            return {Disposition::completed, 1, contended};
        }

        contended = true;
        if (mode == AcquireMode::tryOnly)
        {
            return {Disposition::completed, 0, true};
        }
        if ((attempt & 31U) == 31U)
        {
            SwitchToThread();
        }
        else
        {
            YieldProcessor();
        }
    }

    return {Disposition::fallback, 0, contended};
}

SpinLockResult ReleaseSpinLock(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t spinLockAddress) noexcept
{
    auto* const lock = ResolveSpinLock(
        guestMemory,
        guestMemorySize,
        spinLockAddress);
    if (lock == nullptr)
    {
        return {};
    }

    const auto unlocked = EncodeGuestU32(0);
    const auto locked = EncodeGuestU32(1);
    if (InterlockedCompareExchange(lock, unlocked, locked) != locked)
    {
        return {};
    }
    return {Disposition::completed, 0, false};
}
}

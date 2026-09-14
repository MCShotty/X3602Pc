#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "xeo3_bridge/ac6_fast_critical_section.h"

#include <Windows.h>
#include <intrin.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace
{
bool GuestRangeIsValid(
    const std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t address,
    const std::size_t size) noexcept
{
    return guestMemory != nullptr && address != 0 &&
           static_cast<std::size_t>(address) <= guestMemorySize &&
           size <= guestMemorySize - static_cast<std::size_t>(address);
}

volatile LONG* AtomicGuestField(
    std::uint8_t* const guestMemory,
    const std::uint32_t address) noexcept
{
    auto* const field = guestMemory + address;
    if ((reinterpret_cast<std::uintptr_t>(field) & 3U) != 0)
    {
        return nullptr;
    }
    return reinterpret_cast<volatile LONG*>(field);
}

std::uint32_t EncodeGuestU32(const std::uint32_t value) noexcept
{
    return _byteswap_ulong(value);
}

std::uint32_t DecodeGuestU32(const LONG raw) noexcept
{
    return _byteswap_ulong(static_cast<std::uint32_t>(raw));
}

LONG AtomicLoadRaw(volatile LONG* const field) noexcept
{
    return InterlockedCompareExchange(field, 0, 0);
}

std::uint32_t AtomicLoadGuestU32(volatile LONG* const field) noexcept
{
    return DecodeGuestU32(AtomicLoadRaw(field));
}

void AtomicStoreGuestU32(
    volatile LONG* const field,
    const std::uint32_t value) noexcept
{
    InterlockedExchange(
        field,
        static_cast<LONG>(EncodeGuestU32(value)));
}

bool AtomicCompareExchangeGuestI32(
    volatile LONG* const field,
    const std::int32_t expected,
    const std::int32_t desired) noexcept
{
    const auto expectedRaw = static_cast<LONG>(EncodeGuestU32(
        static_cast<std::uint32_t>(expected)));
    const auto desiredRaw = static_cast<LONG>(EncodeGuestU32(
        static_cast<std::uint32_t>(desired)));
    return InterlockedCompareExchange(field, desiredRaw, expectedRaw) ==
           expectedRaw;
}

std::int32_t AtomicAddGuestI32(
    volatile LONG* const field,
    const std::int32_t delta) noexcept
{
    for (;;)
    {
        const auto beforeRaw = AtomicLoadRaw(field);
        const auto before = static_cast<std::int32_t>(
            DecodeGuestU32(beforeRaw));
        const auto after64 = static_cast<std::int64_t>(before) + delta;
        if (after64 < std::numeric_limits<std::int32_t>::min() ||
            after64 > std::numeric_limits<std::int32_t>::max())
        {
            return before;
        }
        const auto after = static_cast<std::int32_t>(after64);
        const auto afterRaw = static_cast<LONG>(EncodeGuestU32(
            static_cast<std::uint32_t>(after)));
        if (InterlockedCompareExchange(field, afterRaw, beforeRaw) ==
            beforeRaw)
        {
            return after;
        }
        YieldProcessor();
    }
}

struct CriticalSectionFields
{
    volatile LONG* lockCount = nullptr;
    volatile LONG* recursionCount = nullptr;
    volatile LONG* owner = nullptr;
};

bool ResolveCriticalSection(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t address,
    CriticalSectionFields& fields) noexcept
{
    if (!GuestRangeIsValid(
            guestMemory,
            guestMemorySize,
            address,
            xeo3::fast_sync::kCriticalSectionSize) ||
        guestMemory[address] != 1)
    {
        return false;
    }

    fields.lockCount = AtomicGuestField(
        guestMemory,
        address + xeo3::fast_sync::kCriticalSectionLockCountOffset);
    fields.recursionCount = AtomicGuestField(
        guestMemory,
        address + xeo3::fast_sync::kCriticalSectionRecursionCountOffset);
    fields.owner = AtomicGuestField(
        guestMemory,
        address + xeo3::fast_sync::kCriticalSectionOwnerOffset);
    return fields.lockCount != nullptr &&
           fields.recursionCount != nullptr &&
           fields.owner != nullptr;
}

bool CompleteRecursiveEnter(
    const CriticalSectionFields& fields,
    const std::int32_t lockCount,
    const std::uint32_t recursionCount) noexcept
{
    if (recursionCount == 0 ||
        recursionCount == std::numeric_limits<std::uint32_t>::max() ||
        lockCount < 0 ||
        lockCount == std::numeric_limits<std::int32_t>::max() ||
        static_cast<std::uint32_t>(lockCount) < recursionCount - 1)
    {
        return false;
    }

    AtomicAddGuestI32(fields.lockCount, 1);
    AtomicStoreGuestU32(fields.recursionCount, recursionCount + 1);
    return true;
}
}

namespace xeo3::fast_sync
{
bool ReadCurrentThread(
    const std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t pcrAddress,
    std::uint32_t& currentThread) noexcept
{
    const auto address64 =
        static_cast<std::uint64_t>(pcrAddress) + kPcrCurrentThreadOffset;
    if (address64 > std::numeric_limits<std::uint32_t>::max() ||
        !GuestRangeIsValid(
            guestMemory,
            guestMemorySize,
            static_cast<std::uint32_t>(address64),
            sizeof(std::uint32_t)))
    {
        return false;
    }

    std::uint32_t raw = 0;
    std::memcpy(
        &raw,
        guestMemory + static_cast<std::uint32_t>(address64),
        sizeof(raw));
    currentThread = DecodeGuestU32(static_cast<LONG>(raw));
    return currentThread != 0;
}

CriticalSectionResult EnterCriticalSection(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t criticalSectionAddress,
    const std::uint32_t currentThread,
    const EnterMode mode) noexcept
{
    CriticalSectionFields fields{};
    if (currentThread == 0 ||
        !ResolveCriticalSection(
            guestMemory,
            guestMemorySize,
            criticalSectionAddress,
            fields))
    {
        return {};
    }

    const auto lockCount = static_cast<std::int32_t>(
        AtomicLoadGuestU32(fields.lockCount));
    const auto recursionCount =
        AtomicLoadGuestU32(fields.recursionCount);
    const auto owner = AtomicLoadGuestU32(fields.owner);

    if (lockCount == -1 && recursionCount == 0 && owner == 0)
    {
        if (AtomicCompareExchangeGuestI32(fields.lockCount, -1, 0))
        {
            AtomicStoreGuestU32(fields.owner, currentThread);
            AtomicStoreGuestU32(fields.recursionCount, 1);
            return {Disposition::completed, 1};
        }
        return mode == EnterMode::tryOnly
            ? CriticalSectionResult{Disposition::completed, 0}
            : CriticalSectionResult{};
    }

    if (owner == currentThread &&
        CompleteRecursiveEnter(fields, lockCount, recursionCount))
    {
        return {Disposition::completed, 1};
    }

    return mode == EnterMode::tryOnly
        ? CriticalSectionResult{Disposition::completed, 0}
        : CriticalSectionResult{};
}

CriticalSectionResult LeaveCriticalSection(
    std::uint8_t* const guestMemory,
    const std::size_t guestMemorySize,
    const std::uint32_t criticalSectionAddress,
    const std::uint32_t currentThread) noexcept
{
    CriticalSectionFields fields{};
    if (currentThread == 0 ||
        !ResolveCriticalSection(
            guestMemory,
            guestMemorySize,
            criticalSectionAddress,
            fields))
    {
        return {};
    }

    const auto lockCount = static_cast<std::int32_t>(
        AtomicLoadGuestU32(fields.lockCount));
    const auto recursionCount =
        AtomicLoadGuestU32(fields.recursionCount);
    const auto owner = AtomicLoadGuestU32(fields.owner);
    if (owner != currentThread || recursionCount == 0 || lockCount < 0 ||
        static_cast<std::uint32_t>(lockCount) < recursionCount - 1)
    {
        return {};
    }

    if (recursionCount > 1)
    {
        AtomicStoreGuestU32(fields.recursionCount, recursionCount - 1);
        AtomicAddGuestI32(fields.lockCount, -1);
        return {Disposition::completed, 0};
    }

    // This follows the Xbox critical-section release order used by Xenia at
    // commit 95a5c3ee: publish the unowned state before decrementing the lock
    // count. A racing entrant either acquires -1 directly or becomes a waiter
    // included in the post-decrement count and is signaled by the caller.
    AtomicStoreGuestU32(fields.recursionCount, 0);
    AtomicStoreGuestU32(fields.owner, 0);
    const auto remainingWaiters = AtomicAddGuestI32(fields.lockCount, -1);
    return {
        remainingWaiters == -1
            ? Disposition::completed
            : Disposition::completedAndSignal,
        0};
}
}

#pragma once

#include <cstddef>
#include <cstdint>

namespace xeo3::fast_spin
{
enum class AcquireMode : std::uint8_t
{
    wait,
    tryOnly,
};

enum class Disposition : std::uint8_t
{
    fallback,
    completed,
};

struct SpinLockResult
{
    Disposition disposition = Disposition::fallback;
    std::uint32_t returnValue = 0;
    bool contended = false;
};

SpinLockResult AcquireSpinLock(
    std::uint8_t* guestMemory,
    std::size_t guestMemorySize,
    std::uint32_t spinLockAddress,
    AcquireMode mode) noexcept;

SpinLockResult ReleaseSpinLock(
    std::uint8_t* guestMemory,
    std::size_t guestMemorySize,
    std::uint32_t spinLockAddress) noexcept;
}

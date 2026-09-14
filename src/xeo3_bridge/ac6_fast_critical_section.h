#pragma once

#include <cstddef>
#include <cstdint>

namespace xeo3::fast_sync
{
constexpr std::size_t kCriticalSectionSize = 0x1C;
constexpr std::uint32_t kCriticalSectionLockCountOffset = 0x10;
constexpr std::uint32_t kCriticalSectionRecursionCountOffset = 0x14;
constexpr std::uint32_t kCriticalSectionOwnerOffset = 0x18;
constexpr std::uint32_t kPcrCurrentThreadOffset = 0x100;

enum class EnterMode : std::uint8_t
{
    wait,
    tryOnly,
};

enum class Disposition : std::uint8_t
{
    fallback,
    completed,
    completedAndSignal,
};

struct CriticalSectionResult
{
    Disposition disposition = Disposition::fallback;
    std::uint32_t returnValue = 0;
};

bool ReadCurrentThread(
    const std::uint8_t* guestMemory,
    std::size_t guestMemorySize,
    std::uint32_t pcrAddress,
    std::uint32_t& currentThread) noexcept;

CriticalSectionResult EnterCriticalSection(
    std::uint8_t* guestMemory,
    std::size_t guestMemorySize,
    std::uint32_t criticalSectionAddress,
    std::uint32_t currentThread,
    EnterMode mode) noexcept;

CriticalSectionResult LeaveCriticalSection(
    std::uint8_t* guestMemory,
    std::size_t guestMemorySize,
    std::uint32_t criticalSectionAddress,
    std::uint32_t currentThread) noexcept;
}

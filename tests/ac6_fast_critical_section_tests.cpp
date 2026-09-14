#include "xeo3_bridge/ac6_fast_critical_section.h"

#include <intrin.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#define CHECK(expression)        \
    do                           \
    {                            \
        if (!(expression))       \
        {                        \
            return __LINE__;     \
        }                        \
    } while (false)

namespace
{
constexpr std::uint32_t kCriticalSection = 0x100;
constexpr std::uint32_t kPcr = 0x200;
constexpr std::uint32_t kThread = 0x30016020;

void WriteGuestU32(
    std::uint8_t* const memory,
    const std::uint32_t address,
    const std::uint32_t value)
{
    const auto raw = _byteswap_ulong(value);
    std::memcpy(memory + address, &raw, sizeof(raw));
}

std::uint32_t ReadGuestU32(
    const std::uint8_t* const memory,
    const std::uint32_t address)
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, memory + address, sizeof(raw));
    return _byteswap_ulong(raw);
}

template <std::size_t Size>
void InitializeCriticalSection(
    std::array<std::uint8_t, Size>& memory,
    const std::int32_t lockCount = -1,
    const std::uint32_t recursionCount = 0,
    const std::uint32_t owner = 0)
{
    memory[kCriticalSection] = 1;
    WriteGuestU32(
        memory.data(),
        kCriticalSection + xeo3::fast_sync::kCriticalSectionLockCountOffset,
        static_cast<std::uint32_t>(lockCount));
    WriteGuestU32(
        memory.data(),
        kCriticalSection +
            xeo3::fast_sync::kCriticalSectionRecursionCountOffset,
        recursionCount);
    WriteGuestU32(
        memory.data(),
        kCriticalSection + xeo3::fast_sync::kCriticalSectionOwnerOffset,
        owner);
}
}

int main()
{
    using xeo3::fast_sync::Disposition;
    using xeo3::fast_sync::EnterMode;

    alignas(16) std::array<std::uint8_t, 0x400> memory{};
    WriteGuestU32(
        memory.data(),
        kPcr + xeo3::fast_sync::kPcrCurrentThreadOffset,
        kThread);
    std::uint32_t currentThread = 0;
    CHECK(xeo3::fast_sync::ReadCurrentThread(
        memory.data(), memory.size(), kPcr, currentThread));
    CHECK(currentThread == kThread);

    InitializeCriticalSection(memory);
    auto result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::wait);
    CHECK(result.disposition == Disposition::completed);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 0);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 1);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x18) == kThread);

    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::wait);
    CHECK(result.disposition == Disposition::completed);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 1);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 2);

    result = xeo3::fast_sync::LeaveCriticalSection(
        memory.data(), memory.size(), kCriticalSection, currentThread);
    CHECK(result.disposition == Disposition::completed);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 0);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 1);

    result = xeo3::fast_sync::LeaveCriticalSection(
        memory.data(), memory.size(), kCriticalSection, currentThread);
    CHECK(result.disposition == Disposition::completed);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 0xFFFFFFFFU);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 0);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x18) == 0);

    InitializeCriticalSection(memory, 0, 1, 0x30017020);
    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::wait);
    CHECK(result.disposition == Disposition::fallback);
    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::tryOnly);
    CHECK(result.disposition == Disposition::completed);
    CHECK(result.returnValue == 0);

    InitializeCriticalSection(memory, 2, 1, currentThread);
    result = xeo3::fast_sync::LeaveCriticalSection(
        memory.data(), memory.size(), kCriticalSection, currentThread);
    CHECK(result.disposition == Disposition::completedAndSignal);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 1);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 0);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x18) == 0);

    InitializeCriticalSection(memory);
    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::tryOnly);
    CHECK(result.disposition == Disposition::completed);
    CHECK(result.returnValue == 1);

    InitializeCriticalSection(memory);
    memory[kCriticalSection] = 0;
    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection,
        currentThread,
        EnterMode::wait);
    CHECK(result.disposition == Disposition::fallback);
    result = xeo3::fast_sync::EnterCriticalSection(
        memory.data(),
        memory.size(),
        kCriticalSection + 1,
        currentThread,
        EnterMode::wait);
    CHECK(result.disposition == Disposition::fallback);

    InitializeCriticalSection(memory);
    std::atomic<std::uint32_t> acquired{0};
    std::vector<std::thread> contenders;
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        contenders.emplace_back(
            [&, index]
            {
                const auto attempt =
                    xeo3::fast_sync::EnterCriticalSection(
                        memory.data(),
                        memory.size(),
                        kCriticalSection,
                        kThread + index * 0x1000,
                        EnterMode::tryOnly);
                if (attempt.returnValue != 0)
                {
                    acquired.fetch_add(1, std::memory_order_relaxed);
                }
            });
    }
    for (auto& contender : contenders)
    {
        contender.join();
    }
    CHECK(acquired.load(std::memory_order_relaxed) == 1);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x10) == 0);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x14) == 1);
    CHECK(ReadGuestU32(memory.data(), kCriticalSection + 0x18) != 0);

    return 0;
}

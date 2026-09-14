#include "xeo3_bridge/ac6_fast_spin_lock.h"

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
constexpr std::uint32_t kSpinLock = 0x100;

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
}

int main()
{
    using xeo3::fast_spin::AcquireMode;
    using xeo3::fast_spin::Disposition;

    alignas(16) std::array<std::uint8_t, 0x200> memory{};
    auto result = xeo3::fast_spin::AcquireSpinLock(
        memory.data(), memory.size(), kSpinLock, AcquireMode::wait);
    CHECK(result.disposition == Disposition::completed);
    CHECK(result.returnValue == 1);
    CHECK(!result.contended);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 1);

    result = xeo3::fast_spin::AcquireSpinLock(
        memory.data(), memory.size(), kSpinLock, AcquireMode::tryOnly);
    CHECK(result.disposition == Disposition::completed);
    CHECK(result.returnValue == 0);
    CHECK(result.contended);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 1);

    result = xeo3::fast_spin::AcquireSpinLock(
        memory.data(), memory.size(), kSpinLock, AcquireMode::wait);
    CHECK(result.disposition == Disposition::fallback);
    CHECK(result.contended);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 1);

    result = xeo3::fast_spin::ReleaseSpinLock(
        memory.data(), memory.size(), kSpinLock);
    CHECK(result.disposition == Disposition::completed);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 0);

    result = xeo3::fast_spin::ReleaseSpinLock(
        memory.data(), memory.size(), kSpinLock);
    CHECK(result.disposition == Disposition::fallback);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 0);

    WriteGuestU32(memory.data(), kSpinLock, 2);
    result = xeo3::fast_spin::ReleaseSpinLock(
        memory.data(), memory.size(), kSpinLock);
    CHECK(result.disposition == Disposition::fallback);
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 2);

    CHECK(xeo3::fast_spin::AcquireSpinLock(
              nullptr, memory.size(), kSpinLock, AcquireMode::wait)
              .disposition == Disposition::fallback);
    CHECK(xeo3::fast_spin::AcquireSpinLock(
              memory.data(), memory.size(), 0, AcquireMode::wait)
              .disposition == Disposition::fallback);
    CHECK(xeo3::fast_spin::AcquireSpinLock(
              memory.data(), memory.size(), kSpinLock + 1, AcquireMode::wait)
              .disposition == Disposition::fallback);
    CHECK(xeo3::fast_spin::AcquireSpinLock(
              memory.data(), memory.size(),
              static_cast<std::uint32_t>(memory.size() - 2),
              AcquireMode::wait)
              .disposition == Disposition::fallback);

    WriteGuestU32(memory.data(), kSpinLock, 0);
    std::atomic<std::uint32_t> acquired{0};
    std::vector<std::thread> contenders;
    for (std::uint32_t index = 0; index < 8; ++index)
    {
        contenders.emplace_back(
            [&]
            {
                const auto attempt = xeo3::fast_spin::AcquireSpinLock(
                    memory.data(),
                    memory.size(),
                    kSpinLock,
                    AcquireMode::tryOnly);
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
    CHECK(ReadGuestU32(memory.data(), kSpinLock) == 1);
    CHECK(xeo3::fast_spin::ReleaseSpinLock(
              memory.data(), memory.size(), kSpinLock)
              .disposition == Disposition::completed);

    return 0;
}

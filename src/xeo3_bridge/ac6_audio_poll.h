#pragma once

#include <cstdint>

namespace xeo3::ac6_audio
{
struct TickResolution
{
    std::uint32_t tick;
    bool usedFallback;
    bool initializeBaseline;
};

constexpr TickResolution ResolvePollTick(
    const std::uint32_t guestTick,
    const std::uint32_t baselineTick,
    const std::uint32_t hostTick,
    const bool fallbackEnabled) noexcept
{
    if (guestTick != 0 || !fallbackEnabled)
    {
        return {guestTick, false, false};
    }
    return {hostTick, true, baselineTick == 0};
}

constexpr bool IsDelay16CycleInstruction(
    const std::uint32_t guestIar) noexcept
{
    return guestIar >= 0x821E6AE4U &&
           guestIar <= 0x821E6B00U &&
           ((guestIar - 0x821E6AE4U) & 3U) == 0;
}
}

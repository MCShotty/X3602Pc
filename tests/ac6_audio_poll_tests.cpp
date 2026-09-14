#include "xeo3_bridge/ac6_audio_poll.h"

#include <array>
#include <cstdint>
#include <iostream>

namespace
{
bool Expect(const bool condition, const char* const message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}
}

int main()
{
    bool ok = true;

    const auto native = xeo3::ac6_audio::ResolvePollTick(
        0x12345678U,
        0,
        0xABCDEF01U,
        true);
    ok &= Expect(native.tick == 0x12345678U, "nonzero guest tick changed");
    ok &= Expect(!native.usedFallback, "nonzero guest tick used fallback");
    ok &= Expect(!native.initializeBaseline, "native tick initialized baseline");

    const auto fallback = xeo3::ac6_audio::ResolvePollTick(
        0,
        0,
        0xFFFFFFF0U,
        true);
    ok &= Expect(fallback.tick == 0xFFFFFFF0U, "host fallback tick changed");
    ok &= Expect(fallback.usedFallback, "zero guest tick did not use fallback");
    ok &= Expect(fallback.initializeBaseline, "zero baseline was not initialized");

    const auto activeFallback = xeo3::ac6_audio::ResolvePollTick(
        0,
        0xFFFFFF00U,
        0x00000010U,
        true);
    ok &= Expect(activeFallback.tick == 0x00000010U, "active fallback tick changed");
    ok &= Expect(activeFallback.usedFallback, "active fallback was disabled");
    ok &= Expect(
        !activeFallback.initializeBaseline,
        "active fallback reinitialized its baseline");

    const auto disabled = xeo3::ac6_audio::ResolvePollTick(
        0,
        0,
        0xABCDEF01U,
        false);
    ok &= Expect(disabled.tick == 0, "disabled fallback changed guest tick");
    ok &= Expect(!disabled.usedFallback, "disabled fallback reported use");
    ok &= Expect(
        !disabled.initializeBaseline,
        "disabled fallback initialized baseline");

    constexpr std::array<std::uint32_t, 8> delayIars{
        0x821E6AE4U,
        0x821E6AE8U,
        0x821E6AECU,
        0x821E6AF0U,
        0x821E6AF4U,
        0x821E6AF8U,
        0x821E6AFCU,
        0x821E6B00U,
    };
    for (const auto iar : delayIars)
    {
        ok &= Expect(
            xeo3::ac6_audio::IsDelay16CycleInstruction(iar),
            "delay instruction classification mismatch");
    }
    ok &= Expect(
        !xeo3::ac6_audio::IsDelay16CycleInstruction(0x821E6AE0U),
        "instruction before delay range matched");
    ok &= Expect(
        !xeo3::ac6_audio::IsDelay16CycleInstruction(0x821E6B04U),
        "instruction after delay range matched");
    ok &= Expect(
        !xeo3::ac6_audio::IsDelay16CycleInstruction(0x821E6AE5U),
        "unaligned address matched delay instruction");

    return ok ? 0 : 1;
}

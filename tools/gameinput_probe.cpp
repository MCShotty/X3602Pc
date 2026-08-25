#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <GameInput.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace
{
void PrintReading(IGameInput* input)
{
    IGameInputReading* reading = nullptr;
    const auto result = input->GetCurrentReading(
        GameInputKindGamepad,
        nullptr,
        &reading);
    if (FAILED(result))
    {
        std::printf("result=0x%08lX no_gamepad_reading\n", result);
        return;
    }

    GameInputGamepadState state{};
    const auto hasState = reading->GetGamepadState(&state);
    IGameInputDevice* device = nullptr;
    reading->GetDevice(&device);
    const auto* const info =
        device != nullptr ? device->GetDeviceInfo() : nullptr;
    const auto status =
        device != nullptr
            ? static_cast<unsigned>(device->GetDeviceStatus())
            : 0U;
    const auto timestamp =
        static_cast<unsigned long long>(reading->GetTimestamp());
    const auto sequence = static_cast<unsigned long long>(
        reading->GetSequenceNumber(GameInputKindGamepad));

    std::printf(
        "result=0x%08lX state=%u timestamp=%llu sequence=%llu "
        "buttons=0x%08X lt=%.6f rt=%.6f "
        "lx=%.6f ly=%.6f rx=%.6f ry=%.6f "
        "vid=0x%04X pid=0x%04X family=%d status=0x%08X\n",
        result,
        hasState ? 1U : 0U,
        timestamp,
        sequence,
        static_cast<unsigned>(state.buttons),
        state.leftTrigger,
        state.rightTrigger,
        state.leftThumbstickX,
        state.leftThumbstickY,
        state.rightThumbstickX,
        state.rightThumbstickY,
        info != nullptr ? info->vendorId : 0,
        info != nullptr ? info->productId : 0,
        info != nullptr ? static_cast<int>(info->deviceFamily) : -999,
        status);

    if (device != nullptr)
    {
        device->Release();
    }
    reading->Release();
}
}

int main(int argc, char** argv)
{
    auto watchMilliseconds = 0;
    if (argc == 3 && std::strcmp(argv[1], "--watch-ms") == 0)
    {
        watchMilliseconds = std::max(0, std::atoi(argv[2]));
    }
    else if (argc != 1)
    {
        std::fprintf(stderr, "usage: gameinput_probe [--watch-ms N]\n");
        return 2;
    }

    IGameInput* input = nullptr;
    const auto result = GameInputCreate(&input);
    if (FAILED(result))
    {
        std::fprintf(
            stderr,
            "GameInputCreate failed: 0x%08lX\n",
            result);
        return 1;
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(watchMilliseconds);
    do
    {
        PrintReading(input);
        if (watchMilliseconds == 0)
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    while (std::chrono::steady_clock::now() < deadline);

    input->Release();
    return 0;
}

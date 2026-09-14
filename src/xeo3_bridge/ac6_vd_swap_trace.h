#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xeo3::video {
constexpr std::uint32_t kVdSwapThunk = 0x823D05BCu;
constexpr std::uint32_t kMaximumSwapTraces = 16;

struct SwapArguments {
    std::array<std::uint32_t, 8> registers{};
    std::uint32_t stack = 0;
    std::uint32_t callerLr = 0;
};

void ResetSwapTrace() noexcept;
std::uint32_t BeginSwapTrace(const SwapArguments& arguments,
                             const std::uint8_t* guestMemory) noexcept;
void EndSwapTrace(std::uint32_t sequence, const std::uint8_t* guestMemory,
                   std::uint32_t result, std::uint32_t lr) noexcept;

namespace detail {
// Read-only and bounded. Invalid or inaccessible ranges produce no guest write.
bool ReadGuestWords(const std::uint8_t* guestMemory, std::uint32_t address,
                     std::uint32_t* output, std::size_t wordCount) noexcept;
}
}

extern "C" {
extern volatile std::uint32_t BridgeVdSwapTraceEnabled;
extern volatile std::uint32_t BridgeVdSwapTraceCount;
extern volatile std::uint32_t BridgeVdSwapTraceFailure;
}

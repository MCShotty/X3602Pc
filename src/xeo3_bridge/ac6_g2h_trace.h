#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace xeo3::vgpu {
struct G2HTraceSnapshot {
  std::uintptr_t metadataAddress = 0;
  std::array<std::uint32_t, 32> metadata{};
  std::array<std::uint32_t, 6> original{};
  std::array<std::uint32_t, 6> applied{};
};
void ResetG2HTrace() noexcept;
void RecordG2HTrace(const std::uint8_t* moduleBase, const void* returnStack,
                    std::uint64_t context, const void* original,
                    const void* applied, std::size_t size) noexcept;
namespace detail {
// Pinned 2608 callers only. Reads fail closed and never modify guest or host data.
bool CaptureG2HTraceSnapshot(const std::uint8_t* moduleBase,
                             const void* returnStack, const void* original,
                             const void* applied, std::size_t size,
                             G2HTraceSnapshot& output) noexcept;
}
}
extern "C" {
extern volatile std::uint32_t BridgeVgpuG2HTraceEnabled;
extern volatile std::uint32_t BridgeVgpuG2HTraceCount;
extern volatile std::uint32_t BridgeVgpuG2HTraceFailure;
}

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace xeo3::vgpu::detail {
// Bounded, allocation-free shadow of the pinned host's fixed CBV pools.
// Values are GPU addresses, never aliases to mutable descriptor storage.
template <std::size_t Capacity> class ConstantDescriptorShadow {
  static_assert(Capacity != 0 && (Capacity & (Capacity - 1)) == 0);
  struct Entry {
    std::uintptr_t handle = 0;
    std::uint64_t gpuAddress = 0;
  };
  std::array<Entry, Capacity> entries_{};
  static constexpr std::size_t kProbeLimit = Capacity < 32 ? Capacity : 32;
  static std::size_t Index(std::uintptr_t handle) noexcept {
    auto value = static_cast<std::uint64_t>(handle);
    value ^= value >> 29;
    value *= 0x9E3779B185EBCA87ULL;
    return static_cast<std::size_t>((value ^ (value >> 32)) & (Capacity - 1));
  }
public:
  bool Assign(std::uintptr_t handle, std::uint64_t gpuAddress) noexcept {
    if (handle == 0) return false;
    const auto start = Index(handle);
    for (std::size_t probe = 0; probe < kProbeLimit; ++probe) {
      auto &entry = entries_[(start + probe) & (Capacity - 1)];
      if (entry.handle == 0 || entry.handle == handle) {
        entry = {handle, gpuAddress};
        return true;
      }
    }
    return false;
  }
  std::uint64_t Resolve(std::uintptr_t handle) const noexcept {
    if (handle == 0) return 0;
    const auto start = Index(handle);
    for (std::size_t probe = 0; probe < kProbeLimit; ++probe) {
      const auto &entry = entries_[(start + probe) & (Capacity - 1)];
      if (entry.handle == handle) return entry.gpuAddress;
      if (entry.handle == 0) break;
    }
    return 0;
  }
  void Clear() noexcept { entries_.fill({}); }
};
} // namespace xeo3::vgpu::detail

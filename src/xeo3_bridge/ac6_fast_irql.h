#pragma once

#include <cstddef>
#include <cstdint>

namespace xeo3::fast_irql {
constexpr std::uint32_t kPendingIrqlOffset = 0x08;
constexpr std::uint32_t kCurrentIrqlOffset = 0x18;
constexpr std::uint32_t kDispatchLevel = 2;

enum class Disposition : std::uint32_t {
  invalid,
  completed,
  nativeCheckRequired,
};

struct RaiseResult {
  Disposition disposition = Disposition::invalid;
  std::uint32_t oldIrql = 0;
};

struct LowerResult {
  Disposition disposition = Disposition::invalid;
  std::uint32_t pendingIrql = 0;
};

RaiseResult RaiseToDpc(std::uint8_t *guestMemory, std::size_t guestMemorySize,
                       std::uint32_t pcrAddress) noexcept;

LowerResult Lower(std::uint8_t *guestMemory, std::size_t guestMemorySize,
                  std::uint32_t pcrAddress, std::uint32_t newIrql) noexcept;
} // namespace xeo3::fast_irql

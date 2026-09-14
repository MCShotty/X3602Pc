#include "xeo3_bridge/ac6_fast_irql.h"

#include <intrin.h>

#include <cstring>
#include <limits>

namespace {
bool ResolveGuestAddress(std::uint8_t *const guestMemory,
                         const std::size_t guestMemorySize,
                         const std::uint32_t baseAddress,
                         const std::uint32_t offset, const std::size_t size,
                         std::uint8_t *&resolved) noexcept {
  const auto address = static_cast<std::uint64_t>(baseAddress) + offset;
  if (guestMemory == nullptr ||
      address > std::numeric_limits<std::uint32_t>::max() ||
      address > guestMemorySize ||
      size > guestMemorySize - static_cast<std::size_t>(address)) {
    return false;
  }

  resolved = guestMemory + static_cast<std::size_t>(address);
  return true;
}

std::uint16_t ReadGuestU16(const std::uint8_t *const address) noexcept {
  std::uint16_t raw = 0;
  std::memcpy(&raw, address, sizeof(raw));
  return _byteswap_ushort(raw);
}
} // namespace

namespace xeo3::fast_irql {
RaiseResult RaiseToDpc(std::uint8_t *const guestMemory,
                       const std::size_t guestMemorySize,
                       const std::uint32_t pcrAddress) noexcept {
  std::uint8_t *currentIrql = nullptr;
  if (!ResolveGuestAddress(guestMemory, guestMemorySize, pcrAddress,
                           kCurrentIrqlOffset, sizeof(*currentIrql),
                           currentIrql)) {
    return {};
  }

  const auto oldIrql = *currentIrql;
  *currentIrql = static_cast<std::uint8_t>(kDispatchLevel);
  return {Disposition::completed, oldIrql};
}

LowerResult Lower(std::uint8_t *const guestMemory,
                  const std::size_t guestMemorySize,
                  const std::uint32_t pcrAddress,
                  const std::uint32_t newIrql) noexcept {
  std::uint8_t *currentIrql = nullptr;
  if (!ResolveGuestAddress(guestMemory, guestMemorySize, pcrAddress,
                           kCurrentIrqlOffset, sizeof(*currentIrql),
                           currentIrql)) {
    return {};
  }

  *currentIrql = static_cast<std::uint8_t>(newIrql);
  if (newIrql >= kDispatchLevel) {
    return {Disposition::completed, 0};
  }

  std::uint8_t *pendingIrqlAddress = nullptr;
  if (!ResolveGuestAddress(guestMemory, guestMemorySize, pcrAddress,
                           kPendingIrqlOffset, sizeof(std::uint16_t),
                           pendingIrqlAddress)) {
    return {};
  }

  const auto pendingIrql = ReadGuestU16(pendingIrqlAddress);
  return {
      newIrql >= pendingIrql ? Disposition::completed
                             : Disposition::nativeCheckRequired,
      pendingIrql,
  };
}
} // namespace xeo3::fast_irql

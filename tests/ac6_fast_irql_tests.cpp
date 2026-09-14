#include "xeo3_bridge/ac6_fast_irql.h"

#include <array>
#include <cstddef>
#include <cstdint>

#define CHECK(expression)                                                      \
  do {                                                                         \
    if (!(expression)) {                                                       \
      return __LINE__;                                                         \
    }                                                                          \
  } while (false)

int main() {
  using xeo3::fast_irql::Disposition;
  constexpr std::uint32_t kPcr = 0x40;
  std::array<std::uint8_t, 0x100> memory{};

  memory[kPcr + xeo3::fast_irql::kCurrentIrqlOffset] = 0;
  auto raise = xeo3::fast_irql::RaiseToDpc(memory.data(), memory.size(), kPcr);
  CHECK(raise.disposition == Disposition::completed);
  CHECK(raise.oldIrql == 0);
  CHECK(memory[kPcr + xeo3::fast_irql::kCurrentIrqlOffset] ==
        xeo3::fast_irql::kDispatchLevel);

  raise = xeo3::fast_irql::RaiseToDpc(memory.data(), memory.size(), kPcr);
  CHECK(raise.disposition == Disposition::completed);
  CHECK(raise.oldIrql == xeo3::fast_irql::kDispatchLevel);

  memory[kPcr + xeo3::fast_irql::kPendingIrqlOffset] = 0;
  memory[kPcr + xeo3::fast_irql::kPendingIrqlOffset + 1] = 1;
  auto lower = xeo3::fast_irql::Lower(memory.data(), memory.size(), kPcr, 1);
  CHECK(lower.disposition == Disposition::completed);
  CHECK(lower.pendingIrql == 1);
  CHECK(memory[kPcr + xeo3::fast_irql::kCurrentIrqlOffset] == 1);

  lower = xeo3::fast_irql::Lower(memory.data(), memory.size(), kPcr, 0);
  CHECK(lower.disposition == Disposition::nativeCheckRequired);
  CHECK(lower.pendingIrql == 1);
  CHECK(memory[kPcr + xeo3::fast_irql::kCurrentIrqlOffset] == 0);

  lower = xeo3::fast_irql::Lower(memory.data(), memory.size(), kPcr, 2);
  CHECK(lower.disposition == Disposition::completed);
  CHECK(memory[kPcr + xeo3::fast_irql::kCurrentIrqlOffset] == 2);

  memory[kPcr + xeo3::fast_irql::kPendingIrqlOffset] = 0x01;
  memory[kPcr + xeo3::fast_irql::kPendingIrqlOffset + 1] = 0x02;
  lower = xeo3::fast_irql::Lower(memory.data(), memory.size(), kPcr, 1);
  CHECK(lower.disposition == Disposition::nativeCheckRequired);
  CHECK(lower.pendingIrql == 0x0102);

  CHECK(xeo3::fast_irql::RaiseToDpc(nullptr, memory.size(), kPcr).disposition ==
        Disposition::invalid);
  CHECK(xeo3::fast_irql::RaiseToDpc(memory.data(), memory.size(), 0xFFFFFFF0U)
            .disposition == Disposition::invalid);
  CHECK(xeo3::fast_irql::Lower(memory.data(),
                               xeo3::fast_irql::kCurrentIrqlOffset, 1, 0)
            .disposition == Disposition::invalid);
  return 0;
}

#pragma once

#include "xeo3_bridge/xenonrecomp_overrides.h"

#include "ppc_context.h"

#include <cstdint>

namespace xeo3
{
struct TranslatedState
{
    PPCContext ppc{};
    std::uint32_t fpscr{};
    std::uint8_t xerByteCount{};
    std::uint64_t msr{};
    std::uint32_t iar{};
};

void CopyFromXeO3(CpuStateView source, TranslatedState& destination) noexcept;
void CopyToXeO3(TranslatedState& source, CpuStateView destination) noexcept;
void CopyIntegerFromXeO3(
    CpuStateView source,
    TranslatedState& destination) noexcept;
void CopyIntegerToXeO3(
    TranslatedState& source,
    CpuStateView destination) noexcept;
void SetTranslatedIar(
    TranslatedState& state,
    std::uint32_t guestIar) noexcept;
void PublishStackPointerToXeO3(
    const PPCContext& source,
    CpuStateView destination) noexcept;
}

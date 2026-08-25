#include "xeo3_bridge/xenonrecomp_overrides.h"
#include "xeo3_bridge/xeo3_state_sync.h"

#include <array>
#include <cstddef>
#include <cstdint>

#define CHECK(expression)        \
    do                           \
    {                            \
        if (!(expression))       \
        {                        \
            return __LINE__;     \
        }                        \
    } while (false)

extern "C" void BridgeQueueWaitCooperativeYield(
    std::uint32_t,
    std::uint32_t) noexcept
{
}

int main()
{
    alignas(64) std::array<std::byte, 0xB00> firstStorage{};
    alignas(64) std::array<std::byte, 0xB00> secondStorage{};
    xeo3::CpuStateView first(
        firstStorage.data() + xeo3::CpuStateView::kContextPrefixSize);
    xeo3::CpuStateView second(
        secondStorage.data() + xeo3::CpuStateView::kContextPrefixSize);

    for (std::size_t index = 0; index < 32; ++index)
    {
        first.setGpr(index, 0x0102030405060708ULL + index);
        first.setFprBits(index, 0xFFF0000000000000ULL + index);
    }
    for (std::size_t index = 0; index < 8; ++index)
    {
        first.setCondition(
            index,
            {
                static_cast<std::uint8_t>(index & 1),
                static_cast<std::uint8_t>((index >> 1) & 1),
                static_cast<std::uint8_t>((index >> 2) & 1),
                static_cast<std::uint8_t>((index + 1) & 1),
            });
    }
    for (std::size_t index = 0; index < 128; ++index)
    {
        xeo3::CpuStateView::VectorRegister vector{};
        for (std::size_t byteIndex = 0;
             byteIndex < vector.size();
             ++byteIndex)
        {
            vector[byteIndex] =
                std::byte((index * 17 + byteIndex) & 0xFF);
        }
        first.setVector(index, vector);
    }

    first.setLr(0x82100004);
    first.setCtr(0x82110008);
    first.setIar(0x821F5ED0);
    first.setMxcsr(0x1F80);
    first.setXer(true, false, true, 0x5A);
    first.setFpscr(0xA5A55A58);
    first.setVscr(0x00010001);
    first.setMsr(0xABCDEF000200A000ULL);

    const auto originalMxcsr = _mm_getcsr();
    xeo3::TranslatedState translated{};
    xeo3::CopyFromXeO3(first, translated);
    xeo3::CopyToXeO3(translated, second);
    _mm_setcsr(originalMxcsr);

    for (std::size_t index = 0; index < 32; ++index)
    {
        CHECK(second.gpr(index) == first.gpr(index));
        CHECK(second.fprBits(index) == first.fprBits(index));
    }
    for (std::size_t index = 0; index < 8; ++index)
    {
        CHECK(second.condition(index) == first.condition(index));
    }
    for (std::size_t index = 0; index < 128; ++index)
    {
        CHECK(second.vector(index) == first.vector(index));
    }

    CHECK(second.lr() == first.lr());
    CHECK(second.ctr() == first.ctr());
    CHECK(second.iar() == first.iar());
    CHECK(second.xerSo() == first.xerSo());
    CHECK(second.xerOv() == first.xerOv());
    CHECK(second.xerCa() == first.xerCa());
    CHECK(second.xerByteCount() == first.xerByteCount());
    CHECK(second.fpscr() == first.fpscr());
    CHECK(second.vscr() == first.vscr());
    CHECK(second.msr() == first.msr());

    constexpr std::uint64_t kCallerStackPointer =
        0x000000007022FAE0ULL;
    constexpr std::uint64_t kTranslatedFrameSize = 0x390;
    constexpr std::uint64_t kSystemApcFrameSize = 0x360;
    constexpr std::uint64_t kPublishedStackPointer =
        kCallerStackPointer - kTranslatedFrameSize;
    const auto staleApcFrame =
        kCallerStackPointer - kSystemApcFrameSize;
    CHECK(staleApcFrame >= kPublishedStackPointer);
    CHECK(staleApcFrame < kCallerStackPointer);

    translated.ppc.r1.u64 = kPublishedStackPointer;
    second.setGpr(1, kCallerStackPointer);
    xeo3::PublishStackPointerToXeO3(translated.ppc, second);
    CHECK(second.gpr(1) == kPublishedStackPointer);
    CHECK(second.gpr(1) - kSystemApcFrameSize <
          kPublishedStackPointer);

    auto& ctx = translated.ppc;
    ctx.xeo3CpuState =
        secondStorage.data() + xeo3::CpuStateView::kContextPrefixSize;
    CHECK(
        ctx.xeo3IndirectCallsUntilSync ==
        xeo3::kIndirectStateSyncInterval);
    ctx.r1.u64 = 0x8877665544332211ULL;
    ctx.r28.u64 = 0x1122334455667788ULL;
    ctx.f13.u64 = 0x7FF8000012345678ULL;
    for (std::size_t index = 0; index < 16; ++index)
    {
        ctx.v127.u8[index] =
            static_cast<std::uint8_t>(0xF0U - index);
    }
    ctx.cr6.lt = 1;
    ctx.cr6.gt = 0;
    ctx.cr6.eq = 1;
    ctx.cr6.so = 1;
    ctx.lr = 0x8228AFE4;
    ctx.ctr.u64 = 0x8228AFE0;
    ctx.xer = {1, 1, 0};
    ctx.fpscr.csr =
        static_cast<std::uint32_t>(
            PPCFPSCRRegister::GuestToHost[PPC_ROUND_UP]);
    ctx.vscr = 0x00010001;
    ctx.msr = 0x0200A000;
    second.setXer(false, false, false, 0x5A);
    second.setFpscr(0xA5A55A58);
    second.setMsr(0xABCDEF0000000000ULL);
    PPC_SET_GUEST_IAR(0x8228AFE0);
    CHECK(ctx.xeo3GuestIar == 0x8228AFE0);
    CHECK(second.iar() == 0x8228AFE0);

    PPC_SET_GUEST_IAR(0x8228AFE4);
    CHECK(ctx.xeo3GuestIar == 0x8228AFE4);
    CHECK(second.iar() == 0x8228AFE4);

    PPC_PUBLISH_GPR(1);
    PPC_PUBLISH_GPR(28);
    PPC_PUBLISH_FPR(13);
    PPC_PUBLISH_VECTOR(127);
    PPC_PUBLISH_CR(6);
    PPC_PUBLISH_LR();
    PPC_PUBLISH_CTR();
    PPC_PUBLISH_XER();
    ctx.fpscr.setcsr(ctx.fpscr.csr);
    PPC_PUBLISH_FPSCR();
    PPC_PUBLISH_VSCR();
    PPC_PUBLISH_MSR();

    CHECK(second.gpr(1) == ctx.r1.u64);
    CHECK(second.gpr(28) == ctx.r28.u64);
    CHECK(second.fprBits(13) == ctx.f13.u64);
    for (std::size_t index = 0; index < 16; ++index)
    {
        CHECK(
            second.vector(127)[index] ==
            static_cast<std::byte>(ctx.v127.u8[index]));
    }
    const xeo3::CpuStateView::ConditionField expectedCondition{
        1, 0, 1, 1};
    CHECK(second.condition(6) == expectedCondition);
    CHECK(second.lr() == ctx.lr);
    CHECK(second.ctr() == ctx.ctr.u64);
    CHECK(second.xerSo());
    CHECK(second.xerOv());
    CHECK(!second.xerCa());
    CHECK(second.xerByteCount() == 0x5A);
    CHECK((second.fpscr() & PPC_ROUND_MASK) == PPC_ROUND_UP);
    CHECK(second.mxcsr() == ctx.fpscr.csr);
    CHECK(second.vscr() == ctx.vscr);
    CHECK(second.msr() == 0xABCDEF000200A000ULL);

    ctx.fpscr.setcsr(ctx.fpscr.csr);
    xeo3::CopyToXeO3(translated, second);
    _mm_setcsr(originalMxcsr);
    CHECK(second.iar() == 0x8228AFE4);
    CHECK(second.gpr(28) == ctx.r28.u64);
    CHECK(second.fprBits(13) == ctx.f13.u64);
    for (std::size_t index = 0; index < 16; ++index)
    {
        CHECK(
            second.vector(127)[index] ==
            static_cast<std::byte>(ctx.v127.u8[index]));
    }
    CHECK(second.condition(6) == expectedCondition);
    CHECK(second.lr() == ctx.lr);
    CHECK(second.ctr() == ctx.ctr.u64);
    CHECK(second.xerSo());
    CHECK(second.xerOv());
    CHECK(!second.xerCa());
    CHECK(second.xerByteCount() == 0x5A);
    CHECK((second.fpscr() & PPC_ROUND_MASK) == PPC_ROUND_UP);
    CHECK(second.mxcsr() == ctx.fpscr.csr);
    CHECK(second.vscr() == ctx.vscr);
    CHECK(second.msr() == 0xABCDEF000200A000ULL);
    return 0;
}

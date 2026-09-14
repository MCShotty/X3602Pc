#include "xeo3_bridge/xenonrecomp_overrides.h"
#include "xeo3_bridge/xeo3_state_sync.h"

#include <array>
#include <cstring>

namespace
{
using GprMember = PPCRegister PPCContext::*;
using CrMember = PPCCRRegister PPCContext::*;
using VectorMember = PPCVRegister PPCContext::*;

constexpr std::array<GprMember, 32> kGprMembers{
    &PPCContext::r0, &PPCContext::r1, &PPCContext::r2, &PPCContext::r3,
    &PPCContext::r4, &PPCContext::r5, &PPCContext::r6, &PPCContext::r7,
    &PPCContext::r8, &PPCContext::r9, &PPCContext::r10, &PPCContext::r11,
    &PPCContext::r12, &PPCContext::r13, &PPCContext::r14, &PPCContext::r15,
    &PPCContext::r16, &PPCContext::r17, &PPCContext::r18, &PPCContext::r19,
    &PPCContext::r20, &PPCContext::r21, &PPCContext::r22, &PPCContext::r23,
    &PPCContext::r24, &PPCContext::r25, &PPCContext::r26, &PPCContext::r27,
    &PPCContext::r28, &PPCContext::r29, &PPCContext::r30, &PPCContext::r31,
};

constexpr std::array<GprMember, 32> kFprMembers{
    &PPCContext::f0, &PPCContext::f1, &PPCContext::f2, &PPCContext::f3,
    &PPCContext::f4, &PPCContext::f5, &PPCContext::f6, &PPCContext::f7,
    &PPCContext::f8, &PPCContext::f9, &PPCContext::f10, &PPCContext::f11,
    &PPCContext::f12, &PPCContext::f13, &PPCContext::f14, &PPCContext::f15,
    &PPCContext::f16, &PPCContext::f17, &PPCContext::f18, &PPCContext::f19,
    &PPCContext::f20, &PPCContext::f21, &PPCContext::f22, &PPCContext::f23,
    &PPCContext::f24, &PPCContext::f25, &PPCContext::f26, &PPCContext::f27,
    &PPCContext::f28, &PPCContext::f29, &PPCContext::f30, &PPCContext::f31,
};

constexpr std::array<CrMember, 8> kCrMembers{
    &PPCContext::cr0,
    &PPCContext::cr1,
    &PPCContext::cr2,
    &PPCContext::cr3,
    &PPCContext::cr4,
    &PPCContext::cr5,
    &PPCContext::cr6,
    &PPCContext::cr7,
};

#define XEO3_VECTOR_MEMBER(index) &PPCContext::v##index
constexpr std::array<VectorMember, 128> kVectorMembers{
    XEO3_VECTOR_MEMBER(0), XEO3_VECTOR_MEMBER(1),
    XEO3_VECTOR_MEMBER(2), XEO3_VECTOR_MEMBER(3),
    XEO3_VECTOR_MEMBER(4), XEO3_VECTOR_MEMBER(5),
    XEO3_VECTOR_MEMBER(6), XEO3_VECTOR_MEMBER(7),
    XEO3_VECTOR_MEMBER(8), XEO3_VECTOR_MEMBER(9),
    XEO3_VECTOR_MEMBER(10), XEO3_VECTOR_MEMBER(11),
    XEO3_VECTOR_MEMBER(12), XEO3_VECTOR_MEMBER(13),
    XEO3_VECTOR_MEMBER(14), XEO3_VECTOR_MEMBER(15),
    XEO3_VECTOR_MEMBER(16), XEO3_VECTOR_MEMBER(17),
    XEO3_VECTOR_MEMBER(18), XEO3_VECTOR_MEMBER(19),
    XEO3_VECTOR_MEMBER(20), XEO3_VECTOR_MEMBER(21),
    XEO3_VECTOR_MEMBER(22), XEO3_VECTOR_MEMBER(23),
    XEO3_VECTOR_MEMBER(24), XEO3_VECTOR_MEMBER(25),
    XEO3_VECTOR_MEMBER(26), XEO3_VECTOR_MEMBER(27),
    XEO3_VECTOR_MEMBER(28), XEO3_VECTOR_MEMBER(29),
    XEO3_VECTOR_MEMBER(30), XEO3_VECTOR_MEMBER(31),
    XEO3_VECTOR_MEMBER(32), XEO3_VECTOR_MEMBER(33),
    XEO3_VECTOR_MEMBER(34), XEO3_VECTOR_MEMBER(35),
    XEO3_VECTOR_MEMBER(36), XEO3_VECTOR_MEMBER(37),
    XEO3_VECTOR_MEMBER(38), XEO3_VECTOR_MEMBER(39),
    XEO3_VECTOR_MEMBER(40), XEO3_VECTOR_MEMBER(41),
    XEO3_VECTOR_MEMBER(42), XEO3_VECTOR_MEMBER(43),
    XEO3_VECTOR_MEMBER(44), XEO3_VECTOR_MEMBER(45),
    XEO3_VECTOR_MEMBER(46), XEO3_VECTOR_MEMBER(47),
    XEO3_VECTOR_MEMBER(48), XEO3_VECTOR_MEMBER(49),
    XEO3_VECTOR_MEMBER(50), XEO3_VECTOR_MEMBER(51),
    XEO3_VECTOR_MEMBER(52), XEO3_VECTOR_MEMBER(53),
    XEO3_VECTOR_MEMBER(54), XEO3_VECTOR_MEMBER(55),
    XEO3_VECTOR_MEMBER(56), XEO3_VECTOR_MEMBER(57),
    XEO3_VECTOR_MEMBER(58), XEO3_VECTOR_MEMBER(59),
    XEO3_VECTOR_MEMBER(60), XEO3_VECTOR_MEMBER(61),
    XEO3_VECTOR_MEMBER(62), XEO3_VECTOR_MEMBER(63),
    XEO3_VECTOR_MEMBER(64), XEO3_VECTOR_MEMBER(65),
    XEO3_VECTOR_MEMBER(66), XEO3_VECTOR_MEMBER(67),
    XEO3_VECTOR_MEMBER(68), XEO3_VECTOR_MEMBER(69),
    XEO3_VECTOR_MEMBER(70), XEO3_VECTOR_MEMBER(71),
    XEO3_VECTOR_MEMBER(72), XEO3_VECTOR_MEMBER(73),
    XEO3_VECTOR_MEMBER(74), XEO3_VECTOR_MEMBER(75),
    XEO3_VECTOR_MEMBER(76), XEO3_VECTOR_MEMBER(77),
    XEO3_VECTOR_MEMBER(78), XEO3_VECTOR_MEMBER(79),
    XEO3_VECTOR_MEMBER(80), XEO3_VECTOR_MEMBER(81),
    XEO3_VECTOR_MEMBER(82), XEO3_VECTOR_MEMBER(83),
    XEO3_VECTOR_MEMBER(84), XEO3_VECTOR_MEMBER(85),
    XEO3_VECTOR_MEMBER(86), XEO3_VECTOR_MEMBER(87),
    XEO3_VECTOR_MEMBER(88), XEO3_VECTOR_MEMBER(89),
    XEO3_VECTOR_MEMBER(90), XEO3_VECTOR_MEMBER(91),
    XEO3_VECTOR_MEMBER(92), XEO3_VECTOR_MEMBER(93),
    XEO3_VECTOR_MEMBER(94), XEO3_VECTOR_MEMBER(95),
    XEO3_VECTOR_MEMBER(96), XEO3_VECTOR_MEMBER(97),
    XEO3_VECTOR_MEMBER(98), XEO3_VECTOR_MEMBER(99),
    XEO3_VECTOR_MEMBER(100), XEO3_VECTOR_MEMBER(101),
    XEO3_VECTOR_MEMBER(102), XEO3_VECTOR_MEMBER(103),
    XEO3_VECTOR_MEMBER(104), XEO3_VECTOR_MEMBER(105),
    XEO3_VECTOR_MEMBER(106), XEO3_VECTOR_MEMBER(107),
    XEO3_VECTOR_MEMBER(108), XEO3_VECTOR_MEMBER(109),
    XEO3_VECTOR_MEMBER(110), XEO3_VECTOR_MEMBER(111),
    XEO3_VECTOR_MEMBER(112), XEO3_VECTOR_MEMBER(113),
    XEO3_VECTOR_MEMBER(114), XEO3_VECTOR_MEMBER(115),
    XEO3_VECTOR_MEMBER(116), XEO3_VECTOR_MEMBER(117),
    XEO3_VECTOR_MEMBER(118), XEO3_VECTOR_MEMBER(119),
    XEO3_VECTOR_MEMBER(120), XEO3_VECTOR_MEMBER(121),
    XEO3_VECTOR_MEMBER(122), XEO3_VECTOR_MEMBER(123),
    XEO3_VECTOR_MEMBER(124), XEO3_VECTOR_MEMBER(125),
    XEO3_VECTOR_MEMBER(126), XEO3_VECTOR_MEMBER(127),
};
#undef XEO3_VECTOR_MEMBER

static_assert(sizeof(PPCVRegister) == 16);
static_assert(sizeof(xeo3::CpuStateView::VectorRegister) == 16);
static_assert(sizeof(PPCCRRegister) ==
              sizeof(xeo3::CpuStateView::ConditionField));
}

namespace xeo3
{
void CopyFromXeO3(
    const CpuStateView source,
    TranslatedState& destination) noexcept
{
    for (std::size_t index = 0; index < kGprMembers.size(); ++index)
    {
        (destination.ppc.*kGprMembers[index]).u64 = source.gpr(index);
        (destination.ppc.*kFprMembers[index]).u64 = source.fprBits(index);
    }

    for (std::size_t index = 0; index < kCrMembers.size(); ++index)
    {
        const auto field = source.condition(index);
        auto& target = destination.ppc.*kCrMembers[index];
        std::memcpy(&target, &field, sizeof(target));
    }

    for (std::size_t index = 0; index < kVectorMembers.size(); ++index)
    {
        const auto value = source.vector(index);
        auto& target = destination.ppc.*kVectorMembers[index];
        std::memcpy(&target, value.data(), value.size());
    }

    destination.ppc.lr = source.lr();
    destination.ppc.ctr.u64 = source.ctr();
    destination.ppc.xer.so = static_cast<std::uint8_t>(source.xerSo());
    destination.ppc.xer.ov = static_cast<std::uint8_t>(source.xerOv());
    destination.ppc.xer.ca = static_cast<std::uint8_t>(source.xerCa());
    destination.xerByteCount = source.xerByteCount();
    destination.fpscr = source.fpscr();
    destination.ppc.fpscr.csr = source.mxcsr();
    destination.ppc.fpscr.storeFromGuest(destination.fpscr);
    destination.ppc.vscr = source.vscr();
    destination.msr = source.msr();
    destination.ppc.msr = static_cast<std::uint32_t>(destination.msr);
    SetTranslatedIar(
        destination,
        static_cast<std::uint32_t>(source.iar()));
    destination.ppc.xeo3IndirectCallsUntilSync =
        kIndirectStateSyncInterval;
}

void CopyToXeO3(
    TranslatedState& source,
    CpuStateView destination) noexcept
{
    for (std::size_t index = 0; index < kGprMembers.size(); ++index)
    {
        destination.setGpr(
            index,
            (source.ppc.*kGprMembers[index]).u64);
        destination.setFprBits(
            index,
            (source.ppc.*kFprMembers[index]).u64);
    }

    for (std::size_t index = 0; index < kCrMembers.size(); ++index)
    {
        const auto& field = source.ppc.*kCrMembers[index];
        CpuStateView::ConditionField target{};
        std::memcpy(&target, &field, sizeof(target));
        destination.setCondition(index, target);
    }

    for (std::size_t index = 0; index < kVectorMembers.size(); ++index)
    {
        const auto& value = source.ppc.*kVectorMembers[index];
        CpuStateView::VectorRegister target{};
        std::memcpy(target.data(), &value, target.size());
        destination.setVector(index, target);
    }

    destination.setLr(source.ppc.lr);
    destination.setCtr(source.ppc.ctr.u64);
    destination.setXer(
        source.ppc.xer.so != 0,
        source.ppc.xer.ov != 0,
        source.ppc.xer.ca != 0,
        source.xerByteCount);

    const auto roundingMode = source.ppc.fpscr.loadFromHost();
    destination.setMxcsr(source.ppc.fpscr.csr);
    destination.setFpscr((source.fpscr & ~PPC_ROUND_MASK) | roundingMode);
    destination.setVscr(source.ppc.vscr);
    destination.setMsr(
        (source.msr & 0xFFFFFFFF00000000ULL) | source.ppc.msr);
    source.iar = source.ppc.xeo3GuestIar;
    destination.setIar(source.iar);
}

void CopyIntegerFromXeO3(
    const CpuStateView source,
    TranslatedState& destination) noexcept
{
    for (std::size_t index = 0; index < kGprMembers.size(); ++index)
    {
        (destination.ppc.*kGprMembers[index]).u64 = source.gpr(index);
    }

    for (std::size_t index = 0; index < kCrMembers.size(); ++index)
    {
        const auto field = source.condition(index);
        auto& target = destination.ppc.*kCrMembers[index];
        std::memcpy(&target, &field, sizeof(target));
    }

    destination.ppc.lr = source.lr();
    destination.ppc.ctr.u64 = source.ctr();
    destination.ppc.xer.so = static_cast<std::uint8_t>(source.xerSo());
    destination.ppc.xer.ov = static_cast<std::uint8_t>(source.xerOv());
    destination.ppc.xer.ca = static_cast<std::uint8_t>(source.xerCa());
    destination.xerByteCount = source.xerByteCount();
    destination.msr = source.msr();
    destination.ppc.msr = static_cast<std::uint32_t>(destination.msr);
    SetTranslatedIar(
        destination,
        static_cast<std::uint32_t>(source.iar()));
}

void CopyIntegerToXeO3(
    TranslatedState& source,
    CpuStateView destination) noexcept
{
    for (std::size_t index = 0; index < kGprMembers.size(); ++index)
    {
        destination.setGpr(
            index,
            (source.ppc.*kGprMembers[index]).u64);
    }

    for (std::size_t index = 0; index < kCrMembers.size(); ++index)
    {
        const auto& field = source.ppc.*kCrMembers[index];
        CpuStateView::ConditionField target{};
        std::memcpy(&target, &field, sizeof(target));
        destination.setCondition(index, target);
    }

    destination.setLr(source.ppc.lr);
    destination.setCtr(source.ppc.ctr.u64);
    destination.setXer(
        source.ppc.xer.so != 0,
        source.ppc.xer.ov != 0,
        source.ppc.xer.ca != 0,
        source.xerByteCount);
    destination.setMsr(
        (source.msr & 0xFFFFFFFF00000000ULL) | source.ppc.msr);
    source.iar = source.ppc.xeo3GuestIar;
    destination.setIar(source.iar);
}

void SetTranslatedIar(
    TranslatedState& state,
    const std::uint32_t guestIar) noexcept
{
    state.iar = guestIar;
    state.ppc.xeo3GuestIar = guestIar;
}

void PublishStackPointerToXeO3(
    const PPCContext& source,
    CpuStateView destination) noexcept
{
    destination.setGpr(1, source.r1.u64);
}
}

#include "xeo3_bridge/xeo3_cpu_state.h"

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

int main()
{
    alignas(64) std::array<std::byte, 0xB00> storage{};
    xeo3::CpuStateView state(storage.data() +
                             xeo3::CpuStateView::kContextPrefixSize);

    for (std::size_t index = 0; index < 32; ++index)
    {
        const auto value =
            0x1020304050607080ULL ^ (index * 0x0101010101010101ULL);
        state.setGpr(index, value);
        state.setFprBits(index, ~value);
    }

    for (std::size_t index = 0; index < 32; ++index)
    {
        const auto value =
            0x1020304050607080ULL ^ (index * 0x0101010101010101ULL);
        CHECK(state.gpr(index) == value);
        CHECK(state.fprBits(index) == ~value);
    }

    for (std::size_t index = 0; index < 128; ++index)
    {
        xeo3::CpuStateView::VectorRegister value{};
        for (std::size_t byteIndex = 0; byteIndex < value.size(); ++byteIndex)
        {
            value[byteIndex] =
                std::byte((index * value.size() + byteIndex) & 0xFF);
        }
        state.setVector(index, value);
        CHECK(state.vector(index) == value);
    }

    for (std::size_t index = 0; index < 8; ++index)
    {
        const xeo3::CpuStateView::ConditionField value{
            static_cast<std::uint8_t>((index >> 0) & 1),
            static_cast<std::uint8_t>((index >> 1) & 1),
            static_cast<std::uint8_t>((index >> 2) & 1),
            static_cast<std::uint8_t>((index + 1) & 1),
        };
        state.setCondition(index, value);
        CHECK(state.condition(index) == value);
    }

    state.setLr(0x82100004);
    state.setCtr(0x82110008);
    state.setIar(0x821F5ED0);
    state.setMxcsr(0x1F80);
    state.setXer(true, false, true, 0x55);
    state.setFpscr(0xA5A55A5A);
    state.setVscr(0x00010001);
    state.setMsr(0x0200A000);

    CHECK(state.lr() == 0x82100004);
    CHECK(state.ctr() == 0x82110008);
    CHECK(state.iar() == 0x821F5ED0);
    CHECK(state.mxcsr() == 0x1F80);
    CHECK(state.xerSo());
    CHECK(!state.xerOv());
    CHECK(state.xerCa());
    CHECK(state.xerByteCount() == 0x55);
    CHECK(state.fpscr() == 0xA5A55A5A);
    CHECK(state.vscr() == 0x00010001);
    CHECK(state.msr() == 0x0200A000);

    return 0;
}

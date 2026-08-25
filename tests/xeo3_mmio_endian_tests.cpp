#include "xeo3_bridge/xenonrecomp_overrides.h"

#include <array>
#include <cstdint>
#include <utility>

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
    std::array<std::uint8_t, 1> guestAnchor{};
    auto* const guestBase = guestAnchor.data();
    const auto guestBaseAddress =
        reinterpret_cast<std::uintptr_t>(guestBase);
    for (const auto guestAddress : {
             0x82000000U,
             0xA0000000U,
             0xA45F1000U,
             0xD9E90AACU,
         })
    {
        CHECK(
            reinterpret_cast<std::uintptr_t>(
                xeo3::GuestMemoryPointer(guestBase, guestAddress)) -
                guestBaseAddress ==
            guestAddress);
    }

    CHECK(xeo3::SwapMmioWord(0x00000016U) == 0x16000000U);
    CHECK(xeo3::SwapMmioWord(0x16000000U) == 0x00000016U);
    CHECK(xeo3::SwapMmioWord(0x12345678U) == 0x78563412U);

    constexpr std::array<std::uint32_t, 6> values{
        0x00000000U,
        0xFFFFFFFFU,
        0x00000001U,
        0x80000000U,
        0x7FC80714U,
        0xA5C33C5AU,
    };
    for (const auto value : values)
    {
        CHECK(xeo3::SwapMmioWord(xeo3::SwapMmioWord(value)) == value);
    }

    constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 6>
        mmioRanges{{
            {0x7FC80000U, 0x00020000U},
            {0x7FEA1800U, 0x00000400U},
            {0x8FFF1000U, 0x00001000U},
            {0x7FEA1000U, 0x00000200U},
            {0x7F000000U, 0x00000800U},
            {0xFD000000U, 0x02000000U},
        }};
    for (const auto [start, size] : mmioRanges)
    {
        CHECK(xeo3::IsXeO3MmioAddress(start));
        CHECK(xeo3::IsXeO3MmioAddress(start + size - 1U));
        CHECK(!xeo3::IsXeO3MmioAddress(start - 1U));
        CHECK(!xeo3::IsXeO3MmioAddress(start + size));
    }

    CHECK(xeo3::IsXeO3MmioAddress(0x7FC86544U));
    CHECK(!xeo3::IsXeO3MmioAddress(0x00000000U));
    CHECK(!xeo3::IsXeO3MmioAddress(0x40000000U));
    CHECK(!xeo3::IsXeO3MmioAddress(0x82000000U));
    CHECK(!xeo3::IsXeO3MmioAddress(0xB9E90AACU));
    CHECK(!xeo3::IsXeO3MmioAddress(0xD9E90AACU));
    CHECK(!xeo3::IsXeO3MmioAddress(0xFCFFFFFFU));
    CHECK(!xeo3::IsXeO3MmioAddress(0xFF000000U));

    return 0;
}

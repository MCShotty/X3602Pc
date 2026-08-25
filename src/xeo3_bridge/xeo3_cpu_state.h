#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace xeo3
{
class CpuStateView
{
public:
    struct ConditionField
    {
        std::uint8_t lt;
        std::uint8_t gt;
        std::uint8_t eq;
        std::uint8_t so;

        bool operator==(const ConditionField&) const = default;
    };

    using VectorRegister = std::array<std::byte, 16>;

    static constexpr std::ptrdiff_t kContextPrefixSize = 0x80;
    static constexpr std::ptrdiff_t kMxcsrOffset = 0x10;
    static constexpr std::ptrdiff_t kXerSoOffset = 0x14;
    static constexpr std::ptrdiff_t kXerOvOffset = 0x15;
    static constexpr std::ptrdiff_t kXerCaOffset = 0x16;
    static constexpr std::ptrdiff_t kXerByteCountOffset = 0x17;
    static constexpr std::ptrdiff_t kLrOffset = -0x20;
    static constexpr std::ptrdiff_t kCtrOffset = -0x18;
    static constexpr std::ptrdiff_t kDispatchCookieOffset = -0x10;
    static constexpr std::ptrdiff_t kIarOffset = -0x08;
    static constexpr std::ptrdiff_t kFpscrOffset = 0x1E8;
    static constexpr std::ptrdiff_t kVscrOffset = 0x1EC;
    static constexpr std::ptrdiff_t kMsrOffset = 0x1F0;

    explicit CpuStateView(void* anchor) noexcept
        : anchor_(static_cast<std::byte*>(anchor))
    {
    }

    static constexpr std::ptrdiff_t gprOffset(std::size_t index) noexcept
    {
        constexpr std::array<std::ptrdiff_t, 32> offsets{
            0x80, 0x08, 0x88, 0x00, 0x18, 0x20, 0x28, 0x30,
            0x38, 0x40, 0x48, 0x50, 0x58, 0x90, 0x98, 0xA0,
            0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0,
            0xE8, 0xF0, 0xF8, 0x100, 0x60, 0x68, 0x70, 0x78,
        };
        return offsets[index];
    }

    static constexpr std::ptrdiff_t fprOffset(std::size_t index) noexcept
    {
        if (index == 0)
        {
            return -0x60;
        }
        if (index == 1)
        {
            return -0x58;
        }
        if (index <= 12)
        {
            return 0x108 + static_cast<std::ptrdiff_t>(index - 2) * 8;
        }
        if (index == 13)
        {
            return -0x50;
        }
        if (index <= 30)
        {
            return 0x160 + static_cast<std::ptrdiff_t>(index - 14) * 8;
        }
        return -0x48;
    }

    static constexpr std::ptrdiff_t vectorOffset(std::size_t index) noexcept
    {
        if (index <= 62)
        {
            return 0x200 + static_cast<std::ptrdiff_t>(index) * 16;
        }
        if (index == 63)
        {
            return -0x80;
        }
        if (index <= 126)
        {
            return 0x5F0 + static_cast<std::ptrdiff_t>(index - 64) * 16;
        }
        return -0x70;
    }

    static constexpr std::ptrdiff_t conditionOffset(
        std::size_t index) noexcept
    {
        return -0x40 + static_cast<std::ptrdiff_t>(index) * 4;
    }

    std::uint64_t gpr(std::size_t index) const noexcept
    {
        return read<std::uint64_t>(gprOffset(index));
    }

    void setGpr(std::size_t index, std::uint64_t value) noexcept
    {
        write(gprOffset(index), value);
    }

    std::uint64_t fprBits(std::size_t index) const noexcept
    {
        return read<std::uint64_t>(fprOffset(index));
    }

    void setFprBits(std::size_t index, std::uint64_t value) noexcept
    {
        write(fprOffset(index), value);
    }

    VectorRegister vector(std::size_t index) const noexcept
    {
        return read<VectorRegister>(vectorOffset(index));
    }

    void setVector(
        std::size_t index,
        const VectorRegister& value) noexcept
    {
        write(vectorOffset(index), value);
    }

    ConditionField condition(std::size_t index) const noexcept
    {
        return read<ConditionField>(conditionOffset(index));
    }

    void setCondition(
        std::size_t index,
        const ConditionField& value) noexcept
    {
        write(conditionOffset(index), value);
    }

    std::uint64_t lr() const noexcept
    {
        return read<std::uint64_t>(kLrOffset);
    }

    void setLr(std::uint64_t value) noexcept
    {
        write(kLrOffset, value);
    }

    std::uint64_t ctr() const noexcept
    {
        return read<std::uint64_t>(kCtrOffset);
    }

    void setCtr(std::uint64_t value) noexcept
    {
        write(kCtrOffset, value);
    }

    std::uint64_t dispatchCookie() const noexcept
    {
        return read<std::uint64_t>(kDispatchCookieOffset);
    }

    std::uint64_t iar() const noexcept
    {
        return read<std::uint64_t>(kIarOffset);
    }

    void setIar(std::uint64_t value) noexcept
    {
        write(kIarOffset, value);
    }

    std::uint32_t mxcsr() const noexcept
    {
        return read<std::uint32_t>(kMxcsrOffset);
    }

    void setMxcsr(std::uint32_t value) noexcept
    {
        write(kMxcsrOffset, value);
    }

    bool xerSo() const noexcept
    {
        return read<std::uint8_t>(kXerSoOffset) != 0;
    }

    bool xerOv() const noexcept
    {
        return read<std::uint8_t>(kXerOvOffset) != 0;
    }

    bool xerCa() const noexcept
    {
        return read<std::uint8_t>(kXerCaOffset) != 0;
    }

    std::uint8_t xerByteCount() const noexcept
    {
        return read<std::uint8_t>(kXerByteCountOffset);
    }

    void setXer(
        bool so,
        bool ov,
        bool ca,
        std::uint8_t byteCount) noexcept
    {
        write(kXerSoOffset, static_cast<std::uint8_t>(so));
        write(kXerOvOffset, static_cast<std::uint8_t>(ov));
        write(kXerCaOffset, static_cast<std::uint8_t>(ca));
        write(kXerByteCountOffset, byteCount);
    }

    std::uint32_t fpscr() const noexcept
    {
        return read<std::uint32_t>(kFpscrOffset);
    }

    void setFpscr(std::uint32_t value) noexcept
    {
        write(kFpscrOffset, value);
    }

    std::uint32_t vscr() const noexcept
    {
        return read<std::uint32_t>(kVscrOffset);
    }

    void setVscr(std::uint32_t value) noexcept
    {
        write(kVscrOffset, value);
    }

    std::uint64_t msr() const noexcept
    {
        return read<std::uint64_t>(kMsrOffset);
    }

    void setMsr(std::uint64_t value) noexcept
    {
        write(kMsrOffset, value);
    }

private:
    template <typename T>
    T read(std::ptrdiff_t offset) const noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        T value{};
        // XeO3 may update this state from an APC or exception handler while
        // the translated guest thread is interrupted. Keep the compiler from
        // reusing a value across that same-thread asynchronous boundary.
        std::atomic_signal_fence(std::memory_order_seq_cst);
        std::memcpy(&value, anchor_ + offset, sizeof(value));
        return value;
    }

    template <typename T>
    void write(std::ptrdiff_t offset, const T& value) noexcept
    {
        static_assert(std::is_trivially_copyable_v<T>);
        std::memcpy(anchor_ + offset, &value, sizeof(value));
        // Every published instruction state must remain observable to XeO3's
        // same-thread APC/exception machinery. This is a compiler fence only;
        // no cross-core hardware fence is needed.
        std::atomic_signal_fence(std::memory_order_seq_cst);
    }

    std::byte* anchor_;
};

static_assert(sizeof(CpuStateView::ConditionField) == 4);
static_assert(CpuStateView::gprOffset(0) == 0x80);
static_assert(CpuStateView::gprOffset(1) == 0x08);
static_assert(CpuStateView::gprOffset(2) == 0x88);
static_assert(CpuStateView::gprOffset(3) == 0x00);
static_assert(CpuStateView::gprOffset(13) == 0x90);
static_assert(CpuStateView::gprOffset(27) == 0x100);
static_assert(CpuStateView::gprOffset(28) == 0x60);
static_assert(CpuStateView::gprOffset(29) == 0x68);
static_assert(CpuStateView::gprOffset(31) == 0x78);
static_assert(CpuStateView::fprOffset(0) == -0x60);
static_assert(CpuStateView::fprOffset(12) == 0x158);
static_assert(CpuStateView::fprOffset(13) == -0x50);
static_assert(CpuStateView::fprOffset(31) == -0x48);
static_assert(CpuStateView::vectorOffset(0) == 0x200);
static_assert(CpuStateView::vectorOffset(63) == -0x80);
static_assert(CpuStateView::vectorOffset(64) == 0x5F0);
static_assert(CpuStateView::vectorOffset(127) == -0x70);
static_assert(CpuStateView::conditionOffset(0) == -0x40);
static_assert(CpuStateView::conditionOffset(7) == -0x24);
}

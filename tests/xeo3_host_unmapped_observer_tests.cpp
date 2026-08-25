#include "xeo3_bridge/xeo3_host_unmapped_observer.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#define CHECK(expression)        \
    do                           \
    {                            \
        if (!(expression))       \
        {                        \
            return __LINE__;     \
        }                        \
    } while (false)

namespace
{
constexpr std::array<
    std::uint8_t,
    xeo3::host::kUnmappedFatalDetourSize> kExpectedFatalPath{
    0x48, 0x8B, 0x57, 0x78,
    0x48, 0x8D, 0x0D, 0xE5, 0xE5, 0x09, 0x00,
    0xE8, 0xF8, 0xB8, 0xFD, 0xFF,
};

void TestThunk()
{
}

bool Protect(
    void* const address,
    const DWORD protection) noexcept
{
    DWORD oldProtection = 0;
    return VirtualProtect(
        address,
        xeo3::host::kUnmappedFatalDetourSize,
        protection,
        &oldProtection) != FALSE;
}
}

int main()
{
    constexpr auto encodedTarget =
        static_cast<std::uintptr_t>(0x1122334455667788ULL);
    const auto jump =
        xeo3::host::detail::EncodeAbsoluteJump(
            reinterpret_cast<const void*>(encodedTarget));
    CHECK(jump[0] == 0xFF);
    CHECK(jump[1] == 0x25);
    for (std::size_t index = 2; index < 6; ++index)
    {
        CHECK(jump[index] == 0);
    }
    std::uintptr_t decodedTarget = 0;
    std::memcpy(
        &decodedTarget,
        jump.data() + 6,
        sizeof(decodedTarget));
    CHECK(decodedTarget == encodedTarget);
    CHECK(jump[14] == 0x90);
    CHECK(jump[15] == 0x90);

    CHECK(xeo3::host::detail::HasExpectedFatalPath(
        kExpectedFatalPath.data(),
        kExpectedFatalPath.size()));
    auto badFatalPath = kExpectedFatalPath;
    badFatalPath[5] ^= 1U;
    CHECK(!xeo3::host::detail::HasExpectedFatalPath(
        badFatalPath.data(),
        badFatalPath.size()));
    CHECK(!xeo3::host::detail::HasExpectedFatalPath(
        nullptr,
        kExpectedFatalPath.size()));

    constexpr std::uint32_t guestIar = 0x82345678U;
    alignas(16) std::array<std::uint8_t, 0x100> cpuObject{};
    alignas(16) std::array<std::uint8_t, 0x20> guestMemory{};
    void* slotTarget = reinterpret_cast<void*>(&TestThunk);
    const auto slot = reinterpret_cast<std::uintptr_t>(&slotTarget);
    const auto dispatchOffset =
        static_cast<std::uintptr_t>(guestIar) * 2U;
    CHECK(slot > dispatchOffset);
    const auto dispatchBase = slot - dispatchOffset;
    std::memcpy(
        cpuObject.data() + 0x70,
        &dispatchBase,
        sizeof(dispatchBase));

    const auto observation =
        xeo3::host::detail::CaptureObservation(
            guestIar,
            cpuObject.data(),
            guestMemory.data());
    CHECK(observation.guestIar == guestIar);
    CHECK(observation.cpuObject ==
        reinterpret_cast<std::uintptr_t>(cpuObject.data()));
    CHECK(observation.cpuStateAnchor ==
        reinterpret_cast<std::uintptr_t>(cpuObject.data() + 0x80));
    CHECK(observation.guestMemory ==
        reinterpret_cast<std::uintptr_t>(guestMemory.data()));
    CHECK(observation.dispatchBase == dispatchBase);
    CHECK(observation.slot == slot);
    CHECK(observation.slotTarget ==
        reinterpret_cast<std::uintptr_t>(slotTarget));

    const auto emptyObservation =
        xeo3::host::detail::CaptureObservation(
            guestIar,
            nullptr,
            nullptr);
    CHECK(emptyObservation.guestIar == guestIar);
    CHECK(emptyObservation.cpuObject == 0);
    CHECK(emptyObservation.slot == 0);

    auto* const patchPage = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        0x1000,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE));
    CHECK(patchPage != nullptr);
    std::memcpy(
        patchPage,
        kExpectedFatalPath.data(),
        kExpectedFatalPath.size());
    CHECK(Protect(patchPage, PAGE_EXECUTE_READ));
    CHECK(xeo3::host::detail::InstallDetour(
        patchPage,
        reinterpret_cast<const void*>(&TestThunk)));
    CHECK(BridgeHostUnmappedIarObserverInstalled == 1);

    const auto expectedInstalled =
        xeo3::host::detail::EncodeAbsoluteJump(
            reinterpret_cast<const void*>(&TestThunk));
    CHECK(std::equal(
        expectedInstalled.begin(),
        expectedInstalled.end(),
        patchPage));
    CHECK(xeo3::host::detail::InstallDetour(
        patchPage,
        reinterpret_cast<const void*>(&TestThunk)));

    CHECK(Protect(patchPage, PAGE_EXECUTE_READWRITE));
    patchPage[15] ^= 1U;
    CHECK(Protect(patchPage, PAGE_EXECUTE_READ));
    CHECK(!xeo3::host::detail::RemoveDetour());
    CHECK(BridgeHostUnmappedIarObserverFailure ==
        static_cast<std::uint32_t>(
            xeo3::host::ObserverFailure::detourChanged));

    CHECK(Protect(patchPage, PAGE_EXECUTE_READWRITE));
    std::memcpy(
        patchPage,
        expectedInstalled.data(),
        expectedInstalled.size());
    CHECK(Protect(patchPage, PAGE_EXECUTE_READ));
    CHECK(xeo3::host::detail::RemoveDetour());
    CHECK(BridgeHostUnmappedIarObserverInstalled == 0);
    CHECK(std::equal(
        kExpectedFatalPath.begin(),
        kExpectedFatalPath.end(),
        patchPage));

    MEMORY_BASIC_INFORMATION information{};
    CHECK(VirtualQuery(
        patchPage,
        &information,
        sizeof(information)) == sizeof(information));
    CHECK((information.Protect & 0xFFU) == PAGE_EXECUTE_READ);
    CHECK(VirtualFree(patchPage, 0, MEM_RELEASE));

    auto* const badPage = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr,
        0x1000,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    CHECK(badPage != nullptr);
    std::memcpy(
        badPage,
        badFatalPath.data(),
        badFatalPath.size());
    CHECK(!xeo3::host::detail::InstallDetour(
        badPage,
        reinterpret_cast<const void*>(&TestThunk)));
    CHECK(BridgeHostUnmappedIarObserverFailure ==
        static_cast<std::uint32_t>(
            xeo3::host::ObserverFailure::fatalPathMismatch));
    CHECK(VirtualFree(badPage, 0, MEM_RELEASE));

    return 0;
}

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

extern "C"
{
void XeO3CallMappedGuest(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostFence);
void XeO3CallHostGuest(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostTarget,
    void* hostFence);
void XeO3DispatchTestTarget();
void XeO3DispatchTestInvokeAotThunk(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostFence);

void* XeO3DispatchTestObservedCpuState = nullptr;
void* XeO3DispatchTestObservedGuestMemory = nullptr;
std::uint32_t XeO3DispatchTestObservedGuestTarget = 0;
void* XeO3DispatchTestObservedHostFence = nullptr;
std::uint64_t XeO3DispatchTestInvocationCount = 0;
void* XeO3DispatchTestObservedOuterCpuState = nullptr;
void* XeO3DispatchTestObservedOuterGuestMemory = nullptr;
std::uint32_t XeO3DispatchTestObservedOuterGuestTarget = 0;
void* XeO3DispatchTestObservedOuterHostFence = nullptr;

void XeO3Dispatch(
    void* cpuState,
    std::uint8_t* guestMemory,
    std::uint32_t guestTarget,
    void* hostFence)
{
    XeO3DispatchTestObservedOuterCpuState = cpuState;
    XeO3DispatchTestObservedOuterGuestMemory = guestMemory;
    XeO3DispatchTestObservedOuterGuestTarget = guestTarget;
    XeO3DispatchTestObservedOuterHostFence = hostFence;
}
}

int main()
{
    alignas(16) std::array<std::uint8_t, 0x300> cpuStateStorage{};
    alignas(16) std::array<std::uint8_t, 0x100> guestMemory{};
    alignas(16) std::array<std::uint8_t, 0x100> dispatchMap{};

    constexpr std::uint32_t guestTarget = 0x20;
    auto* const cpuState = cpuStateStorage.data() + 0x80;
    const auto dispatchBase =
        reinterpret_cast<std::uintptr_t>(dispatchMap.data());
    const auto nativeTarget =
        reinterpret_cast<std::uintptr_t>(&XeO3DispatchTestTarget);
    auto* const hostFence = reinterpret_cast<void*>(0x123456789ABCDEF0ULL);
    std::memcpy(cpuState - 0x10, &dispatchBase, sizeof(dispatchBase));
    std::memcpy(
        dispatchMap.data() + guestTarget * 2,
        &nativeTarget,
        sizeof(nativeTarget));

    XeO3DispatchTestInvokeAotThunk(
        cpuState,
        guestMemory.data(),
        guestTarget,
        hostFence);

    CHECK(XeO3DispatchTestObservedOuterCpuState == cpuState);
    CHECK(XeO3DispatchTestObservedOuterGuestMemory == guestMemory.data());
    CHECK(XeO3DispatchTestObservedOuterGuestTarget == guestTarget);
    CHECK(XeO3DispatchTestObservedOuterHostFence == hostFence);

    XeO3CallMappedGuest(
        cpuState,
        guestMemory.data(),
        guestTarget,
        hostFence);

    CHECK(XeO3DispatchTestInvocationCount == 1);
    CHECK(XeO3DispatchTestObservedCpuState == cpuState);
    CHECK(XeO3DispatchTestObservedGuestMemory == guestMemory.data());
    CHECK(XeO3DispatchTestObservedGuestTarget == guestTarget);
    CHECK(XeO3DispatchTestObservedHostFence == hostFence);

    constexpr std::uint32_t directGuestTarget = 0x8005EEC8U;
    XeO3CallHostGuest(
        cpuState,
        guestMemory.data(),
        directGuestTarget,
        reinterpret_cast<void*>(&XeO3DispatchTestTarget),
        hostFence);

    CHECK(XeO3DispatchTestInvocationCount == 2);
    CHECK(XeO3DispatchTestObservedCpuState == cpuState);
    CHECK(XeO3DispatchTestObservedGuestMemory == guestMemory.data());
    CHECK(XeO3DispatchTestObservedGuestTarget == directGuestTarget);
    CHECK(XeO3DispatchTestObservedHostFence == hostFence);
    return 0;
}

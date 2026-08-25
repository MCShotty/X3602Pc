#include "xeo3_contract/xeo3_contract.h"
#include "ac6_probe_mappings.inc"

#include <Windows.h>

#include <array>
#include <iterator>

extern "C" IMAGE_DOS_HEADER __ImageBase;

extern "C"
{
__declspec(dllexport) std::uintptr_t PrecompiledPointers[xeo3::kHostPointerCount]{};
__declspec(dllexport) std::uint32_t PrecompiledImportTable[
    (std::size(xeo3::generated::kImportGuestAddresses) + 1) * 2]{};

__declspec(dllexport) volatile std::uint64_t ProbeInvocationCount = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeLastCpuState = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeLastGuestMemory = 0;
__declspec(dllexport) volatile std::uint32_t ProbeLastGuestIar = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeHostInterface = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeHostValue0 = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeHostValue1 = 0;
__declspec(dllexport) volatile std::uintptr_t ProbeHostValue2 = 0;
}

namespace
{
std::array<
    xeo3::MappingEntry,
    std::size(xeo3::generated::kProbeGuestAddresses)>
    g_mappings{};

using QueryHostValues = void (*)(
    void* self,
    std::uintptr_t* value0,
    std::uintptr_t* value1,
    std::uintptr_t* value2);

std::uint32_t NegotiateVersion(const std::uint32_t requestedVersion)
{
    if (requestedVersion >= xeo3::kMinimumAbiVersion &&
        requestedVersion <= xeo3::kCurrentAbiVersion)
    {
        return requestedVersion;
    }

    return xeo3::kCurrentAbiVersion;
}

void QueryObservedHostValues(void* hostInterface)
{
    if (hostInterface == nullptr)
    {
        return;
    }

    auto*** object = static_cast<void***>(hostInterface);
    if (*object == nullptr)
    {
        return;
    }

    constexpr std::size_t queryHostValuesIndex = 0x28 / sizeof(void*);
    auto query = reinterpret_cast<QueryHostValues>((*object)[queryHostValuesIndex]);
    if (query == nullptr)
    {
        return;
    }

    std::uintptr_t value0 = 0;
    std::uintptr_t value1 = 0;
    std::uintptr_t value2 = 0;
    query(hostInterface, &value0, &value1, &value2);

    ProbeHostValue0 = value0;
    ProbeHostValue1 = value1;
    ProbeHostValue2 = value2;
}
}

extern "C" std::uint32_t InitPrecompiledDll(
    xeo3::MappingEntry** mappings,
    std::uint64_t* mappingCount,
    void* hostInterface,
    std::uint32_t options[2])
{
    const auto moduleBase = reinterpret_cast<std::uintptr_t>(&__ImageBase);
    const auto thunkAddress = reinterpret_cast<std::uintptr_t>(&XeO3ProbeThunk);
    const auto thunkRva = thunkAddress - moduleBase;

    if (thunkRva > UINT32_MAX)
    {
        return 0;
    }

    for (std::size_t index = 0; index < g_mappings.size(); ++index)
    {
        g_mappings[index] = {
            .guestAddress = xeo3::generated::kProbeGuestAddresses[index],
            .hostRva = static_cast<std::uint32_t>(thunkRva),
        };
    }

    for (std::size_t index = 0;
         index < std::size(xeo3::generated::kImportGuestAddresses);
         ++index)
    {
        PrecompiledImportTable[index * 2] =
            xeo3::generated::kImportGuestAddresses[index];
        PrecompiledImportTable[index * 2 + 1] = 0;
    }

    if (mappings != nullptr)
    {
        *mappings = g_mappings.data();
    }

    if (mappingCount != nullptr)
    {
        *mappingCount = g_mappings.size();
    }

    ProbeHostInterface = reinterpret_cast<std::uintptr_t>(hostInterface);
    QueryObservedHostValues(hostInterface);

    const std::uint32_t requestedVersion =
        options == nullptr ? xeo3::kCurrentAbiVersion : options[1];

    if (options != nullptr)
    {
        options[0] = 1;
        options[1] = xeo3::kCurrentAbiVersion;
    }

    OutputDebugStringA("AC6 XeO3 contract probe initialized.\n");
    return NegotiateVersion(requestedVersion);
}

extern "C" void CleanupPrecompiledDll()
{
    for (std::size_t index = 0;
         index < std::size(xeo3::generated::kImportGuestAddresses);
         ++index)
    {
        PrecompiledImportTable[index * 2 + 1] = 0;
    }

    ProbeHostInterface = 0;
    ProbeHostValue0 = 0;
    ProbeHostValue1 = 0;
    ProbeHostValue2 = 0;
    OutputDebugStringA("AC6 XeO3 contract probe cleaned up.\n");
}

#pragma once

#include <cstddef>
#include <cstdint>

#if defined(XEO3_CONTRACT_BUILD)
#define XEO3_API __declspec(dllexport)
#else
#define XEO3_API __declspec(dllimport)
#endif

namespace xeo3
{
constexpr std::uint32_t kMinimumAbiVersion = 0x21;
constexpr std::uint32_t kCurrentAbiVersion = 0x2E;
constexpr std::uint32_t kAc6EntryPoint = 0x821F5ED0;
constexpr std::uint32_t kAc6EntryPointRva = 0x001F5ED0;
constexpr std::uint32_t kDispatcherStopIar = 0x6BF0F910;
constexpr std::size_t kHostPointerCount = 16;

struct MappingEntry
{
    std::uint32_t guestAddress;
    std::uint32_t hostRva;
};

static_assert(sizeof(MappingEntry) == 8);
static_assert(offsetof(MappingEntry, hostRva) == 4);
}

extern "C"
{
XEO3_API std::uint32_t InitPrecompiledDll(
    xeo3::MappingEntry** mappings,
    std::uint64_t* mappingCount,
    void* hostInterface,
    std::uint32_t options[2]);

XEO3_API void CleanupPrecompiledDll();

XEO3_API extern std::uintptr_t PrecompiledPointers[xeo3::kHostPointerCount];
XEO3_API extern std::uint32_t PrecompiledImportTable[];

XEO3_API extern volatile std::uint64_t ProbeInvocationCount;
XEO3_API extern volatile std::uintptr_t ProbeLastCpuState;
XEO3_API extern volatile std::uintptr_t ProbeLastGuestMemory;
XEO3_API extern volatile std::uint32_t ProbeLastGuestIar;
XEO3_API extern volatile std::uintptr_t ProbeHostInterface;
XEO3_API extern volatile std::uintptr_t ProbeHostValue0;
XEO3_API extern volatile std::uintptr_t ProbeHostValue1;
XEO3_API extern volatile std::uintptr_t ProbeHostValue2;

void XeO3ProbeThunk();
}

#undef XEO3_API

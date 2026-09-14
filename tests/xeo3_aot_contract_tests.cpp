#include "xeo3_contract/xeo3_contract.h"

#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <iterator>

#define CHECK(expression)        \
    do                           \
    {                            \
        if (!(expression))       \
        {                        \
            std::fprintf(        \
                stderr,          \
                "check failed at line %d: %s\n", \
                __LINE__,        \
                #expression);    \
            return __LINE__;     \
        }                        \
    } while (false)

namespace
{
using InitFunction = std::uint32_t (*)(
    xeo3::MappingEntry**,
    std::uint64_t*,
    void*,
    std::uint32_t[2]);
using CleanupFunction = void (*)();

void ExecuteIar(void*, std::uint64_t)
{
}

struct FakeHostInterface
{
    void** vtable;
};
}

int wmain(const int argumentCount, wchar_t** arguments)
{
    if (argumentCount != 2)
    {
        std::fwprintf(stderr, L"expected one DLL argument, argc=%d\n", argumentCount);
        for (int index = 0; index < argumentCount; ++index)
        {
            std::fwprintf(stderr, L"argv[%d]=%ls\n", index, arguments[index]);
        }
        return __LINE__;
    }
    const auto module = LoadLibraryW(arguments[1]);
    CHECK(module != nullptr);

    const auto init = reinterpret_cast<InitFunction>(
        GetProcAddress(module, "InitPrecompiledDll"));
    const auto cleanup = reinterpret_cast<CleanupFunction>(
        GetProcAddress(module, "CleanupPrecompiledDll"));
    auto* const imports = reinterpret_cast<std::uint32_t*>(
        GetProcAddress(module, "PrecompiledImportTable"));
    auto* const pointers = reinterpret_cast<std::uintptr_t*>(
        GetProcAddress(module, "PrecompiledPointers"));
    auto* const synchronousQueueEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeSynchronousQueueEnabled"));
    CHECK(init != nullptr);
    CHECK(cleanup != nullptr);
    CHECK(imports != nullptr);
    CHECK(pointers != nullptr);
    CHECK(synchronousQueueEnabled != nullptr);
    constexpr std::array<const char*, 81> runtimeExports{
        "BridgeVgpuShadowRestartShaderCount",
        "BridgeVgpuAircraftRestartShaderCount",
        "BridgeVgpuPipelineStateLastFailureResult",
        "BridgeVgpuPipelineStateLastFailureCreateSequence",
        "BridgeImportCountersEnabled",
        "BridgeImportCounterCount",
        "BridgeImportCounts",
        "BridgeFunctionLookupEntryCount",
        "BridgeIntegerImportSyncEnabled",
        "BridgeIntegerImportSyncAttemptCount",
        "BridgeIntegerImportSyncHitCount",
        "BridgeIntegerImportSyncFallbackCount",
        "BridgeIntegerImportSyncLastThunk",
        "BridgeGuestIarTelemetryEnabled",
        "BridgeContinuousStatePublicationMode",
        "BridgeSynchronousArchiveSignalCount",
        "BridgeSynchronousArchiveSignalFailureCount",
        "BridgeSynchronousArchiveReadLastEvent",
        "BridgeSynchronousArchiveReadLastApcRoutine",
        "BridgeSynchronousArchiveReadLastApcContext",
        "BridgeSynchronousArchiveSignalLastStatus",
        "BridgePostIntroCallbackCount",
        "BridgePostIntroCallbackReturnCount",
        "BridgeThreadImportStates",
        "BridgeFastSynchronizationTelemetryEnabled",
        "BridgeFastCriticalSectionsEnabled",
        "BridgeFastCriticalSectionAttemptCount",
        "BridgeFastCriticalSectionHitCount",
        "BridgeFastCriticalSectionFallbackCount",
        "BridgeFastCriticalSectionSignalCount",
        "BridgeFastSpinLocksEnabled",
        "BridgeFastSpinLockAttemptCount",
        "BridgeFastSpinLockHitCount",
        "BridgeFastSpinLockFallbackCount",
        "BridgeFastSpinLockContentionCount",
        "BridgeFastSpinLockLastThunk",
        "BridgeFastSpinLockLastAddress",
        "BridgeFastIrqlEnabled",
        "BridgeFastIrqlTelemetryEnabled",
        "BridgeFastIrqlAttemptCount",
        "BridgeFastIrqlHitCount",
        "BridgeFastIrqlNativeFallbackCount",
        "BridgeFastIrqlInvalidFallbackCount",
        "BridgeFastIrqlLastThunk",
        "BridgeFastIrqlLastOldIrql",
        "BridgeFastIrqlLastNewIrql",
        "BridgeFastIrqlLastPendingIrql",
        "BridgeRaiseIrqlOldPassiveCount",
        "BridgeRaiseIrqlOldApcCount",
        "BridgeRaiseIrqlOldDispatchCount",
        "BridgeRaiseIrqlUnexpectedCount",
        "BridgeIndirectTelemetryEnabled",
        "BridgeIndirectStateSyncEnabled",
        "BridgeSynchronousQueueEnabled",
        "BridgeSynchronousQueueBypassCount",
        "BridgeVgpuXenosTranslateHookInstalled",
        "BridgeVgpuXenosTranslateHookFailure",
        "BridgeVgpuXenosTranslateCount",
        "BridgeVgpuXenosUcodeCaptureCount",
        "BridgeVgpuXenosUcodeCaptureFailure",
        "BridgeVgpuXenosShaderMapFailure",
        "BridgeVgpuXenosLastStage",
        "BridgeVgpuXenosLastUcodeSize",
        "BridgeVgpuXenosLastUcodeHash0",
        "BridgeVgpuXenosLastUcodeHash1",
        "BridgeVgpuXenosLastUcodeHash2",
        "BridgeVgpuXenosLastUcodeHash3",
        "BridgeVgpuGroundFixEnabled",
        "BridgeVgpuReciprocalFixEnabled",
        "BridgeVgpuWaveBallotFingerprintMatchCount",
        "BridgeVgpuWaveBallotFixShaderCount",
        "BridgeVgpuWaveBallotFixSiteCount",
        "BridgeVgpuWaveBallotFixFailure",
        "BridgeVgpuWaveBallotFixEnabled",
        "BridgeVgpuPso535CullFixEnabled",
        "BridgeVgpuPso535CullCandidateCount",
        "BridgeVgpuPso535CullFingerprintMatchCount",
        "BridgeVgpuPso535CullFixPipelineCount",
        "BridgeVgpuPso535CullFixFailure",
        "BridgeVgpuPso535CullLastOriginalMode",
        "BridgeVgpuPso535CullLastReplacementMode",
    };
    for (const auto* const exportName : runtimeExports)
    {
        CHECK(GetProcAddress(module, exportName) != nullptr);
    }
    auto* const importCountersEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeImportCountersEnabled"));
    auto* const importCounterCount =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeImportCounterCount"));
    auto* const importCounts =
        reinterpret_cast<volatile std::uint64_t*>(
            GetProcAddress(module, "BridgeImportCounts"));
    auto* const functionLookupEntryCount =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeFunctionLookupEntryCount"));
    auto* const integerImportSyncEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeIntegerImportSyncEnabled"));
    auto* const guestIarTelemetryEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeGuestIarTelemetryEnabled"));
    auto* const continuousStatePublicationMode =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeContinuousStatePublicationMode"));
    auto* const fastIrqlEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeFastIrqlEnabled"));
    auto* const fastIrqlTelemetryEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeFastIrqlTelemetryEnabled"));
    auto* const fastSynchronizationTelemetryEnabled =
        reinterpret_cast<volatile std::uint32_t*>(GetProcAddress(
            module,
            "BridgeFastSynchronizationTelemetryEnabled"));
    auto* const indirectTelemetryEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeIndirectTelemetryEnabled"));
    auto* const indirectStateSyncEnabled =
        reinterpret_cast<volatile std::uint32_t*>(
            GetProcAddress(module, "BridgeIndirectStateSyncEnabled"));
    CHECK(importCountersEnabled != nullptr);
    CHECK(importCounterCount != nullptr);
    CHECK(importCounts != nullptr);
    CHECK(functionLookupEntryCount != nullptr);
    CHECK(integerImportSyncEnabled != nullptr);
    CHECK(guestIarTelemetryEnabled != nullptr);
    CHECK(continuousStatePublicationMode != nullptr);
    CHECK(fastIrqlEnabled != nullptr);
    CHECK(fastIrqlTelemetryEnabled != nullptr);
    CHECK(fastSynchronizationTelemetryEnabled != nullptr);
    CHECK(indirectTelemetryEnabled != nullptr);
    CHECK(indirectStateSyncEnabled != nullptr);
    CHECK(*fastSynchronizationTelemetryEnabled == 0);
    CHECK(*indirectTelemetryEnabled == 0);
    CHECK(*indirectStateSyncEnabled == 0);
    CHECK(*importCounterCount == 229);

    std::array<void*, 34> vtable{};
    vtable[0] = reinterpret_cast<void*>(&ExecuteIar);
    FakeHostInterface host{vtable.data()};

    xeo3::MappingEntry* mappings = nullptr;
    std::uint64_t mappingCount = 0;
    std::uint32_t options[2]{0, xeo3::kCurrentAbiVersion};
    CHECK(init(&mappings, &mappingCount, &host, options) ==
          xeo3::kCurrentAbiVersion);
    CHECK(options[0] == 1);
    CHECK(options[1] == xeo3::kCurrentAbiVersion);
    CHECK(*synchronousQueueEnabled == 1);
    CHECK(*importCountersEnabled == 0);
    CHECK(*integerImportSyncEnabled == 0);
    CHECK(*guestIarTelemetryEnabled == 0);
    CHECK(*continuousStatePublicationMode == 0);
    CHECK(*fastIrqlEnabled == 0);
    CHECK(*fastIrqlTelemetryEnabled == 0);
    CHECK(*functionLookupEntryCount == 0x002A8000);
    for (std::size_t index = 0; index < *importCounterCount; ++index)
    {
        CHECK(importCounts[index] == 0);
    }
    CHECK(mappings != nullptr);
    std::fprintf(
        stderr,
        "mapping_count=%llu\n",
        static_cast<unsigned long long>(mappingCount));
    CHECK(mappingCount == 18842);

    constexpr std::array<std::uint32_t, 105> requiredMappingRvas{
        xeo3::kAc6EntryPointRva,
        0x002526AC,
        0x0025277C,
        0x002527C0,
        0x00252814,
        0x00252868,
        0x002529E8,
        0x00252A38,
        0x00252AB0,
        0x00284970,
        0x00284DA4,
        0x00272658,
        0x00272668,
        0x002728A0,
        0x002728B8,
        0x002728D0,
        0x002728E8,
        0x00272900,
        0x00272918,
        0x00272930,
        0x00272948,
        0x00272960,
        0x00272978,
        0x00272990,
        0x002729A8,
        0x002729C0,
        0x002729D8,
        0x00272C08,
        0x00272C20,
        0x00272C38,
        0x00272C50,
        0x00272C68,
        0x00272C80,
        0x00272C98,
        0x00272CB0,
        0x00272CC8,
        0x00272CE0,
        0x00272CF8,
        0x00288BC8,
        0x0028AFE0,
        0x002AF840,
        0x002B09E8,
        0x0028B2F0,
        0x0028B2F8,
        0x0028BDA8,
        0x0028BDB0,
        0x0028BDB8,
        0x0028BDC0,
        0x0028BDC8,
        0x0028BC30,
        0x0028BC50,
        0x0028BC70,
        0x002AACA0,
        0x002B0FA0,
        0x002B0FB0,
        0x002B0FC0,
        0x002B95F0,
        0x002C1638,
        0x002C1660,
        0x002C1880,
        0x002D3DA0,
        0x002DD838,
        0x002E0EB8,
        0x002E35B0,
        0x002E4FC8,
        0x002EFA00,
        0x002EFA18,
        0x002EFA30,
        0x002EFA48,
        0x002EFD00,
        0x002EFE08,
        0x002EFE78,
        0x002F0BD8,
        0x002F26E0,
        0x002F2BB8,
        0x002F2C58,
        0x002F2C90,
        0x002F2CC8,
        0x002F2D00,
        0x002F2D38,
        0x002F2D70,
        0x002F3030,
        0x002F4330,
        0x002F5230,
        0x002F7120,
        0x002F8988,
        0x003038B8,
        0x00319198,
        0x00321578,
        0x00321628,
        0x00321660,
        0x003216F0,
        0x00321770,
        0x003217E0,
        0x00321838,
        0x00321898,
        0x003218F0,
        0x00321940,
        0x003219D0,
        0x003222F0,
        0x00322300,
        0x00322310,
        0x00322320,
        0x00322330,
        0x00322340,
    };
    std::array<bool, requiredMappingRvas.size()> foundMappings{};
    for (std::uint64_t index = 0; index < mappingCount; ++index)
    {
        CHECK(mappings[index].hostRva != 0);
        if (index != 0)
        {
            CHECK(mappings[index - 1].guestAddress <
                  mappings[index].guestAddress);
            CHECK(mappings[index - 1].hostRva ==
                  mappings[index].hostRva);
        }
        for (std::size_t requiredIndex = 0;
             requiredIndex < requiredMappingRvas.size();
             ++requiredIndex)
        {
            if (mappings[index].guestAddress ==
                requiredMappingRvas[requiredIndex])
            {
                foundMappings[requiredIndex] = true;
            }
        }
    }
    for (const bool found : foundMappings)
    {
        CHECK(found);
    }

    std::size_t importCount = 0;
    while (imports[importCount * 2] != 0)
    {
        CHECK(imports[importCount * 2 + 1] == 0);
        ++importCount;
        CHECK(importCount <= 229);
    }
    CHECK(importCount == 229);
    CHECK(imports[0] == 0x823CFE1C);
    CHECK(imports[(importCount - 1) * 2] == 0x823D0C5C);

    imports[1] = 0x80050000;
    pointers[8] = 0x11111111;
    cleanup();
    CHECK(imports[0] == 0x823CFE1C);
    CHECK(imports[1] == 0);
    CHECK(pointers[8] == 0);
    CHECK(FreeLibrary(module) != 0);
    return 0;
}

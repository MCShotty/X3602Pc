#include "xeo3_bridge/xeo3_host_unmapped_observer.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <limits>

extern "C" void HostUnmappedIarObserverThunk();

extern "C"
{
__declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarObserverInstalled = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarObserverFailure = 0;
__declspec(dllexport) volatile std::uint64_t
    BridgeHostUnmappedIarCount = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarLastIar = 0;
__declspec(dllexport) volatile std::uint32_t
    BridgeHostUnmappedIarLastThreadId = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastCpuObject = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastCpuStateAnchor = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastGuestMemory = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastDispatchBase = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastSlot = 0;
__declspec(dllexport) volatile std::uintptr_t
    BridgeHostUnmappedIarLastSlotTarget = 0;

std::uintptr_t HostUnmappedFatalFormatTarget = 0;
std::uintptr_t HostUnmappedFatalMessage = 0;
std::uintptr_t HostUnmappedFatalTrapTarget = 0;
}

namespace
{
constexpr DWORD kExpectedTimestamp = 0x6A585A77;
constexpr DWORD kExpectedImageSize = 0x00D97000;
constexpr std::uintptr_t kFatalPathRva = 0x0005FE78;
constexpr std::uintptr_t kFatalFormatRva = 0x0003B780;
constexpr std::uintptr_t kFatalMessageRva = 0x000FE468;
constexpr std::uintptr_t kFatalTrapRva = 0x0005FE88;
constexpr std::ptrdiff_t kCpuStateAnchorOffset = 0x80;
constexpr std::ptrdiff_t kDispatchBaseFromCpuObjectOffset = 0x70;
constexpr std::array<std::uint8_t, 32> kExpectedSha256{
    0xD1, 0x57, 0x8E, 0x07, 0xB5, 0x33, 0xE3, 0x91,
    0xD8, 0xA8, 0x1C, 0x33, 0x0D, 0x54, 0x93, 0xBA,
    0x2D, 0x45, 0xB2, 0x52, 0x49, 0x0A, 0x81, 0x8E,
    0xC1, 0x48, 0xDA, 0xBE, 0x1B, 0xA2, 0x4D, 0x06,
};
constexpr std::array<
    std::uint8_t,
    xeo3::host::kUnmappedFatalDetourSize> kExpectedFatalPath{
    0x48, 0x8B, 0x57, 0x78,
    0x48, 0x8D, 0x0D, 0xE5, 0xE5, 0x09, 0x00,
    0xE8, 0xF8, 0xB8, 0xFD, 0xFF,
};

SRWLOCK g_patchLock = SRWLOCK_INIT;
SRWLOCK g_logLock = SRWLOCK_INIT;
std::uint8_t* g_detourTarget = nullptr;
std::array<std::uint8_t, xeo3::host::kUnmappedFatalDetourSize>
    g_installedDetour{};

void SetFailure(const xeo3::host::ObserverFailure failure) noexcept
{
    BridgeHostUnmappedIarObserverFailure =
        static_cast<std::uint32_t>(failure);
}

bool IsReadableProtection(const DWORD protection) noexcept
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0)
    {
        return false;
    }

    switch (protection & 0xFFU)
    {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return true;
    default:
        return false;
    }
}

bool IsReadableRange(
    const void* const address,
    const std::size_t byteCount) noexcept
{
    if (address == nullptr || byteCount == 0)
    {
        return false;
    }

    auto cursor = reinterpret_cast<std::uintptr_t>(address);
    if (cursor >
        (std::numeric_limits<std::uintptr_t>::max)() - byteCount)
    {
        return false;
    }
    const auto end = cursor + byteCount;
    while (cursor < end)
    {
        MEMORY_BASIC_INFORMATION information{};
        if (VirtualQuery(
                reinterpret_cast<const void*>(cursor),
                &information,
                sizeof(information)) != sizeof(information) ||
            information.State != MEM_COMMIT ||
            !IsReadableProtection(information.Protect))
        {
            return false;
        }

        const auto regionEnd =
            reinterpret_cast<std::uintptr_t>(information.BaseAddress) +
            information.RegionSize;
        if (regionEnd <= cursor)
        {
            return false;
        }
        cursor = (std::min)(regionEnd, end);
    }
    return true;
}

std::uintptr_t CalculateSlot(
    const std::uintptr_t dispatchBase,
    const std::uint32_t guestIar) noexcept
{
    const auto offset = static_cast<std::uintptr_t>(guestIar) * 2U;
    if (dispatchBase == 0 ||
        dispatchBase >
            (std::numeric_limits<std::uintptr_t>::max)() - offset)
    {
        return 0;
    }

    const auto slot = dispatchBase + offset;
    return (slot & (alignof(void*) - 1U)) == 0 ? slot : 0;
}

void AppendObservationLog(
    const xeo3::host::Observation& observation,
    const std::uint64_t count,
    const std::uint32_t threadId) noexcept
{
    std::array<char, 768> record{};
    const auto length = std::snprintf(
        record.data(),
        record.size(),
        "{\"xeo3_ac6\":\"host_unmapped_iar\","
        "\"count\":%" PRIu64 ",\"tick\":%" PRIu64
        ",\"tid\":%u,\"iar\":\"0x%08X\","
        "\"cpu_object\":\"%p\",\"cpu_anchor\":\"%p\","
        "\"guest_memory\":\"%p\",\"dispatch\":\"%p\","
        "\"slot\":\"%p\",\"slot_target\":\"%p\"}\r\n",
        count,
        static_cast<std::uint64_t>(GetTickCount64()),
        threadId,
        observation.guestIar,
        reinterpret_cast<void*>(observation.cpuObject),
        reinterpret_cast<void*>(observation.cpuStateAnchor),
        reinterpret_cast<void*>(observation.guestMemory),
        reinterpret_cast<void*>(observation.dispatchBase),
        reinterpret_cast<void*>(observation.slot),
        reinterpret_cast<void*>(observation.slotTarget));
    if (length <= 0)
    {
        return;
    }

    const auto byteCount = static_cast<DWORD>((std::min)(
        static_cast<std::size_t>(length),
        record.size() - 1U));
    OutputDebugStringA(record.data());

    HMODULE ownModule = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&AppendObservationLog),
            &ownModule) ||
        ownModule == nullptr)
    {
        return;
    }

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        ownModule,
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        return;
    }

    auto* const extension = std::wcsrchr(path.data(), L'.');
    constexpr wchar_t kLogSuffix[] = L".unmapped-iar.log";
    if (extension == nullptr ||
        static_cast<std::size_t>(extension - path.data()) +
                std::size(kLogSuffix) >
            path.size())
    {
        return;
    }
    std::wmemcpy(extension, kLogSuffix, std::size(kLogSuffix));

    AcquireSRWLockExclusive(&g_logLock);
    const auto file = CreateFileW(
        path.data(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (file != INVALID_HANDLE_VALUE)
    {
        DWORD bytesWritten = 0;
        static_cast<void>(WriteFile(
            file,
            record.data(),
            byteCount,
            &bytesWritten,
            nullptr));
        static_cast<void>(FlushFileBuffers(file));
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}

bool ValidatePinnedHost(
    const xeo3::host::HashFileSha256 hashFile,
    HMODULE& module) noexcept
{
    module = nullptr;
    if (hashFile == nullptr)
    {
        SetFailure(xeo3::host::ObserverFailure::missingHashCallback);
        return false;
    }

    module = GetModuleHandleW(nullptr);
    if (module == nullptr)
    {
        SetFailure(xeo3::host::ObserverFailure::moduleMissing);
        return false;
    }

    const auto* const base =
        reinterpret_cast<const std::uint8_t*>(module);
    if (!IsReadableRange(base, sizeof(IMAGE_DOS_HEADER)))
    {
        SetFailure(xeo3::host::ObserverFailure::invalidPe);
        return false;
    }
    const auto* const dos =
        reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew < static_cast<LONG>(sizeof(*dos)) ||
        dos->e_lfanew > 0x1000)
    {
        SetFailure(xeo3::host::ObserverFailure::invalidPe);
        return false;
    }

    const auto* const nt =
        reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base + dos->e_lfanew);
    if (!IsReadableRange(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE)
    {
        SetFailure(xeo3::host::ObserverFailure::invalidPe);
        return false;
    }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->FileHeader.TimeDateStamp != kExpectedTimestamp ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
    {
        SetFailure(xeo3::host::ObserverFailure::identityMismatch);
        return false;
    }

    std::array<wchar_t, 32768> path{};
    const auto pathLength = GetModuleFileNameW(
        module,
        path.data(),
        static_cast<DWORD>(path.size()));
    if (pathLength == 0 || pathLength >= path.size())
    {
        SetFailure(xeo3::host::ObserverFailure::pathFailure);
        return false;
    }

    std::array<std::uint8_t, 32> digest{};
    if (!hashFile(path.data(), digest) || digest != kExpectedSha256)
    {
        SetFailure(xeo3::host::ObserverFailure::hashMismatch);
        return false;
    }
    return true;
}
}

namespace xeo3::host::detail
{
std::array<std::uint8_t, kUnmappedFatalDetourSize>
EncodeAbsoluteJump(const void* const target) noexcept
{
    std::array<std::uint8_t, kUnmappedFatalDetourSize> jump{};
    jump.fill(0x90);
    jump[0] = 0xFF;
    jump[1] = 0x25;
    jump[2] = 0;
    jump[3] = 0;
    jump[4] = 0;
    jump[5] = 0;
    std::memcpy(
        jump.data() + 6,
        &target,
        sizeof(target));
    return jump;
}

bool HasExpectedFatalPath(
    const std::uint8_t* const bytes,
    const std::size_t byteCount) noexcept
{
    return bytes != nullptr &&
        byteCount == kExpectedFatalPath.size() &&
        std::equal(
            kExpectedFatalPath.begin(),
            kExpectedFatalPath.end(),
            bytes);
}

Observation CaptureObservation(
    const std::uint32_t guestIar,
    void* const cpuObject,
    std::uint8_t* const guestMemory) noexcept
{
    Observation observation{};
    observation.guestIar = guestIar;
    observation.cpuObject =
        reinterpret_cast<std::uintptr_t>(cpuObject);
    observation.guestMemory =
        reinterpret_cast<std::uintptr_t>(guestMemory);
    if (cpuObject == nullptr)
    {
        return observation;
    }

    auto* const cpuBytes =
        static_cast<std::uint8_t*>(cpuObject);
    observation.cpuStateAnchor =
        reinterpret_cast<std::uintptr_t>(
            cpuBytes + kCpuStateAnchorOffset);
    const auto* const dispatchPointer =
        cpuBytes + kDispatchBaseFromCpuObjectOffset;
    if (!IsReadableRange(dispatchPointer, sizeof(std::uintptr_t)))
    {
        return observation;
    }
    std::memcpy(
        &observation.dispatchBase,
        dispatchPointer,
        sizeof(observation.dispatchBase));

    observation.slot =
        CalculateSlot(observation.dispatchBase, guestIar);
    if (observation.slot != 0 &&
        IsReadableRange(
            reinterpret_cast<const void*>(observation.slot),
            sizeof(std::uintptr_t)))
    {
        std::memcpy(
            &observation.slotTarget,
            reinterpret_cast<const void*>(observation.slot),
            sizeof(observation.slotTarget));
    }
    return observation;
}

bool InstallDetour(
    std::uint8_t* const target,
    const void* const thunk) noexcept
{
    AcquireSRWLockExclusive(&g_patchLock);
    if (g_detourTarget != nullptr)
    {
        const auto sameTarget = g_detourTarget == target;
        const auto unchanged =
            sameTarget &&
            std::equal(
                g_installedDetour.begin(),
                g_installedDetour.end(),
                target);
        ReleaseSRWLockExclusive(&g_patchLock);
        if (!sameTarget)
        {
            SetFailure(ObserverFailure::differentTargetInstalled);
            return false;
        }
        if (!unchanged)
        {
            BridgeHostUnmappedIarObserverInstalled = 0;
            SetFailure(ObserverFailure::detourChanged);
            return false;
        }
        SetFailure(ObserverFailure::none);
        return true;
    }

    if (target == nullptr ||
        thunk == nullptr ||
        !HasExpectedFatalPath(target, kUnmappedFatalDetourSize))
    {
        ReleaseSRWLockExclusive(&g_patchLock);
        SetFailure(ObserverFailure::fatalPathMismatch);
        return false;
    }

    const auto detour = EncodeAbsoluteJump(thunk);
    DWORD oldProtection = 0;
    if (!VirtualProtect(
            target,
            kUnmappedFatalDetourSize,
            PAGE_EXECUTE_READWRITE,
            &oldProtection))
    {
        ReleaseSRWLockExclusive(&g_patchLock);
        SetFailure(ObserverFailure::targetProtectionFailure);
        return false;
    }

    std::memcpy(target, detour.data(), detour.size());
    FlushInstructionCache(
        GetCurrentProcess(),
        target,
        detour.size());

    DWORD ignoredProtection = 0;
    if (!VirtualProtect(
            target,
            kUnmappedFatalDetourSize,
            oldProtection,
            &ignoredProtection))
    {
        std::memcpy(
            target,
            kExpectedFatalPath.data(),
            kExpectedFatalPath.size());
        FlushInstructionCache(
            GetCurrentProcess(),
            target,
            kExpectedFatalPath.size());
        static_cast<void>(VirtualProtect(
            target,
            kUnmappedFatalDetourSize,
            oldProtection,
            &ignoredProtection));
        ReleaseSRWLockExclusive(&g_patchLock);
        SetFailure(ObserverFailure::targetProtectionRestoreFailure);
        return false;
    }

    g_detourTarget = target;
    g_installedDetour = detour;
    BridgeHostUnmappedIarObserverInstalled = 1;
    SetFailure(ObserverFailure::none);
    ReleaseSRWLockExclusive(&g_patchLock);
    return true;
}

bool RemoveDetour() noexcept
{
    AcquireSRWLockExclusive(&g_patchLock);
    auto* const target = g_detourTarget;
    if (target == nullptr)
    {
        BridgeHostUnmappedIarObserverInstalled = 0;
        SetFailure(ObserverFailure::none);
        ReleaseSRWLockExclusive(&g_patchLock);
        return true;
    }

    if (!std::equal(
            g_installedDetour.begin(),
            g_installedDetour.end(),
            target))
    {
        BridgeHostUnmappedIarObserverInstalled = 0;
        SetFailure(ObserverFailure::detourChanged);
        ReleaseSRWLockExclusive(&g_patchLock);
        return false;
    }

    DWORD oldProtection = 0;
    if (!VirtualProtect(
            target,
            kUnmappedFatalDetourSize,
            PAGE_EXECUTE_READWRITE,
            &oldProtection))
    {
        SetFailure(ObserverFailure::targetProtectionFailure);
        ReleaseSRWLockExclusive(&g_patchLock);
        return false;
    }

    std::memcpy(
        target,
        kExpectedFatalPath.data(),
        kExpectedFatalPath.size());
    FlushInstructionCache(
        GetCurrentProcess(),
        target,
        kExpectedFatalPath.size());
    DWORD ignoredProtection = 0;
    const auto restoredProtection = VirtualProtect(
        target,
        kUnmappedFatalDetourSize,
        oldProtection,
        &ignoredProtection);

    g_detourTarget = nullptr;
    g_installedDetour = {};
    BridgeHostUnmappedIarObserverInstalled = 0;
    ReleaseSRWLockExclusive(&g_patchLock);
    if (!restoredProtection)
    {
        SetFailure(ObserverFailure::targetProtectionRestoreFailure);
        return false;
    }

    SetFailure(ObserverFailure::none);
    return true;
}
}

namespace xeo3::host
{
bool InstallPinnedUnmappedIarObserver(
    const HashFileSha256 hashFile) noexcept
{
    HMODULE module = nullptr;
    if (!ValidatePinnedHost(hashFile, module))
    {
        return false;
    }

    auto* const moduleBase =
        reinterpret_cast<std::uint8_t*>(module);
    HostUnmappedFatalFormatTarget =
        reinterpret_cast<std::uintptr_t>(
            moduleBase + kFatalFormatRva);
    HostUnmappedFatalMessage =
        reinterpret_cast<std::uintptr_t>(
            moduleBase + kFatalMessageRva);
    HostUnmappedFatalTrapTarget =
        reinterpret_cast<std::uintptr_t>(
            moduleBase + kFatalTrapRva);

    if (!detail::InstallDetour(
            moduleBase + kFatalPathRva,
            reinterpret_cast<const void*>(
                &HostUnmappedIarObserverThunk)))
    {
        HostUnmappedFatalFormatTarget = 0;
        HostUnmappedFatalMessage = 0;
        HostUnmappedFatalTrapTarget = 0;
        return false;
    }
    return true;
}

bool RemovePinnedUnmappedIarObserver() noexcept
{
    const auto removed = detail::RemoveDetour();
    if (removed)
    {
        HostUnmappedFatalFormatTarget = 0;
        HostUnmappedFatalMessage = 0;
        HostUnmappedFatalTrapTarget = 0;
    }
    return removed;
}
}

extern "C" void RecordHostUnmappedIar(
    const std::uint32_t guestIar,
    void* const cpuObject,
    std::uint8_t* const guestMemory) noexcept
{
    const auto observation =
        xeo3::host::detail::CaptureObservation(
            guestIar,
            cpuObject,
            guestMemory);
    const auto count = static_cast<std::uint64_t>(
        InterlockedIncrement64(
            reinterpret_cast<volatile LONG64*>(
                &BridgeHostUnmappedIarCount)));
    const auto threadId = GetCurrentThreadId();

    BridgeHostUnmappedIarLastIar = observation.guestIar;
    BridgeHostUnmappedIarLastThreadId = threadId;
    BridgeHostUnmappedIarLastCpuObject = observation.cpuObject;
    BridgeHostUnmappedIarLastCpuStateAnchor =
        observation.cpuStateAnchor;
    BridgeHostUnmappedIarLastGuestMemory =
        observation.guestMemory;
    BridgeHostUnmappedIarLastDispatchBase =
        observation.dispatchBase;
    BridgeHostUnmappedIarLastSlot = observation.slot;
    BridgeHostUnmappedIarLastSlotTarget =
        observation.slotTarget;

    AppendObservationLog(observation, count, threadId);
}

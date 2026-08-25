#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <limits>

namespace
{
struct MappingEntry
{
    std::uint32_t guestAddress;
    std::uint32_t hostRva;
};

using InitFunction = std::uint32_t (*)(
    MappingEntry** mappings,
    std::uint64_t* mappingCount,
    void* hostInterface,
    std::uint32_t options[2]);
using CleanupFunction = void (*)();

std::uint64_t HostStub(
    void*,
    std::uint64_t,
    std::uint64_t,
    std::uint64_t) noexcept
{
    return 0;
}

struct FakeHostInterface
{
    void** vtable;
};

constexpr std::array<std::uint32_t, 12> kTargets{
    0x0001EEC4U,
    0x0001EEC8U,
    0x00019C50U,
    0x000392F4U,
    0x0005F0B4U,
    0x0005F0B8U,
    0x8005EEC4U,
    0x8005EEC8U,
    0x8005F0B4U,
    0x8005F0B8U,
    0x80059C50U,
    0x800792F4U,
};
}

int wmain(const int argumentCount, wchar_t** arguments)
{
    if (argumentCount != 2)
    {
        std::fwprintf(stderr, L"usage: xeo3_mapping_dump <AOT DLL>\n");
        return 2;
    }

    std::array<void*, 64> vtable{};
    std::fill(
        vtable.begin(),
        vtable.end(),
        reinterpret_cast<void*>(&HostStub));
    FakeHostInterface host{vtable.data()};

    const auto module = LoadLibraryW(arguments[1]);
    if (module == nullptr)
    {
        std::fwprintf(
            stderr,
            L"LoadLibraryW failed for %ls: %lu\n",
            arguments[1],
            GetLastError());
        return 3;
    }

    const auto init = reinterpret_cast<InitFunction>(
        GetProcAddress(module, "InitPrecompiledDll"));
    const auto cleanup = reinterpret_cast<CleanupFunction>(
        GetProcAddress(module, "CleanupPrecompiledDll"));
    if (init == nullptr || cleanup == nullptr)
    {
        std::fprintf(stderr, "required exports are missing\n");
        FreeLibrary(module);
        return 4;
    }

    MappingEntry* mappings = nullptr;
    std::uint64_t mappingCount = 0;
    std::uint32_t options[2]{0, 0x2EU};
    const auto version =
        init(&mappings, &mappingCount, &host, options);
    std::printf(
        "version=0x%X options={0x%X,0x%X} mappings=%p count=%llu\n",
        version,
        options[0],
        options[1],
        static_cast<void*>(mappings),
        static_cast<unsigned long long>(mappingCount));

    if (mappings == nullptr ||
        mappingCount >
            static_cast<std::uint64_t>(
                (std::numeric_limits<std::size_t>::max)()))
    {
        cleanup();
        FreeLibrary(module);
        return 5;
    }

    for (const auto target : kTargets)
    {
        const auto begin = mappings;
        const auto end =
            mappings + static_cast<std::size_t>(mappingCount);
        const auto found = std::lower_bound(
            begin,
            end,
            target,
            [](const MappingEntry& entry, const std::uint32_t value)
            {
                return entry.guestAddress < value;
            });
        std::printf("target=%08X", target);
        if (found != end && found->guestAddress == target)
        {
            std::printf(
                " exact host_rva=%08X",
                found->hostRva);
        }
        if (found != begin)
        {
            const auto& previous = found[-1];
            std::printf(
                " previous=%08X:%08X",
                previous.guestAddress,
                previous.hostRva);
        }
        if (found != end)
        {
            std::printf(
                " next=%08X:%08X",
                found->guestAddress,
                found->hostRva);
        }
        std::putchar('\n');
    }

    cleanup();
    FreeLibrary(module);
    return 0;
}

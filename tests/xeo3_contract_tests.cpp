#include "xeo3_contract/xeo3_contract.h"

#include <array>
#include <cstdint>

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
constexpr std::uintptr_t kHostValue0 = 0x1111111122222222;
constexpr std::uintptr_t kHostValue1 = 0x3333333344444444;
constexpr std::uintptr_t kHostValue2 = 0x5555555566666666;

void QueryHostValues(
    void*,
    std::uintptr_t* value0,
    std::uintptr_t* value1,
    std::uintptr_t* value2)
{
    *value0 = kHostValue0;
    *value1 = kHostValue1;
    *value2 = kHostValue2;
}

struct FakeHostInterface
{
    void** vtable;
};
}

int main()
{
    std::array<void*, 6> vtable{};
    vtable[5] = reinterpret_cast<void*>(&QueryHostValues);
    FakeHostInterface host{vtable.data()};

    xeo3::MappingEntry* mappings = nullptr;
    std::uint64_t mappingCount = 0;
    std::uint32_t options[2]{0xFFFFFFFF, xeo3::kCurrentAbiVersion};

    const auto version =
        InitPrecompiledDll(&mappings, &mappingCount, &host, options);

    CHECK(version == xeo3::kCurrentAbiVersion);
    CHECK(options[0] == 1);
    CHECK(options[1] == xeo3::kCurrentAbiVersion);
    CHECK(mappingCount >= 1);
    CHECK(mappings != nullptr);
    bool foundEntryPoint = false;
    for (std::uint64_t index = 0; index < mappingCount; ++index)
    {
        CHECK(mappings[index].hostRva != 0);
        if (mappings[index].guestAddress == xeo3::kAc6EntryPointRva)
        {
            foundEntryPoint = true;
        }
    }
    CHECK(foundEntryPoint);

    std::size_t importCount = 0;
    bool foundFirstObservedImport = false;
    while (PrecompiledImportTable[importCount * 2] != 0)
    {
        const auto guestAddress = PrecompiledImportTable[importCount * 2];
        CHECK(PrecompiledImportTable[importCount * 2 + 1] == 0);
        if (guestAddress == 0x823CFE1C)
        {
            foundFirstObservedImport = true;
        }

        ++importCount;
        CHECK(importCount < mappingCount);
    }
    CHECK(importCount > 0);
    CHECK(foundFirstObservedImport);

    CHECK(ProbeHostInterface == reinterpret_cast<std::uintptr_t>(&host));
    CHECK(ProbeHostValue0 == kHostValue0);
    CHECK(ProbeHostValue1 == kHostValue1);
    CHECK(ProbeHostValue2 == kHostValue2);

    options[1] = xeo3::kMinimumAbiVersion;
    CHECK(InitPrecompiledDll(&mappings, &mappingCount, nullptr, options) ==
          xeo3::kMinimumAbiVersion);

    options[1] = 0;
    CHECK(InitPrecompiledDll(&mappings, &mappingCount, nullptr, options) ==
          xeo3::kCurrentAbiVersion);

    CleanupPrecompiledDll();
    CHECK(ProbeHostInterface == 0);
    CHECK(ProbeHostValue0 == 0);
    CHECK(ProbeHostValue1 == 0);
    CHECK(ProbeHostValue2 == 0);
    return 0;
}

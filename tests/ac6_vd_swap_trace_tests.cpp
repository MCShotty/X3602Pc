#include "xeo3_bridge/ac6_vd_swap_trace.h"
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdint>

#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

int main() {
    std::array<std::uint8_t, 512> memory{};
    memory[32] = 0x12; memory[33] = 0x34; memory[34] = 0x56; memory[35] = 0x78;
    memory[36] = 0xff; memory[37] = 0xff; memory[38] = 0xff; memory[39] = 0xfe;
    const auto original = memory;
    std::array<std::uint32_t, 2> words{};
    CHECK(xeo3::video::detail::ReadGuestWords(memory.data(), 32, words.data(), 2));
    CHECK(words[0] == 0x12345678 && words[1] == 0xfffffffe);
    CHECK(memory == original);
    CHECK(!xeo3::video::detail::ReadGuestWords(nullptr, 32, words.data(), 2));
    CHECK(words[0] == 0 && words[1] == 0);
    CHECK(!xeo3::video::detail::ReadGuestWords(memory.data(), 0, words.data(), 2));
    CHECK(!xeo3::video::detail::ReadGuestWords(memory.data(), 0xfffffffc, words.data(), 2));
    CHECK(!xeo3::video::detail::ReadGuestWords(memory.data(), 32, nullptr, 2));
    CHECK(!xeo3::video::detail::ReadGuestWords(memory.data(), 32, words.data(), 0));
    CHECK(!xeo3::video::detail::ReadGuestWords(memory.data(), 32, words.data(), 65));
    void* inaccessible = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
    CHECK(inaccessible != nullptr);
    const bool readInaccessible = xeo3::video::detail::ReadGuestWords(
        static_cast<std::uint8_t*>(inaccessible), 32, words.data(), 2);
    CHECK(VirtualFree(inaccessible, 0, MEM_RELEASE));
    CHECK(!readInaccessible && words[0] == 0 && words[1] == 0);
    xeo3::video::ResetSwapTrace();
    CHECK(BridgeVdSwapTraceEnabled == 0 && BridgeVdSwapTraceCount == 0);
    CHECK(xeo3::video::BeginSwapTrace({}, memory.data()) == 0);
    xeo3::video::EndSwapTrace(0, memory.data(), 0, 0);
    xeo3::video::EndSwapTrace(1, memory.data(), 0, 0);
    xeo3::video::EndSwapTrace(xeo3::video::kMaximumSwapTraces + 1, memory.data(), 0, 0);
    CHECK(BridgeVdSwapTraceCount == 0 && BridgeVdSwapTraceFailure == 0);
    return 0;
}

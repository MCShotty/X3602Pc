#include "xeo3_bridge/ac6_vd_swap_trace.h"

#include <Windows.h>
#include <intrin.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>

extern "C" {
// Exported from the DLL by xeo3_aot.def; the same source is linked into tests.
volatile std::uint32_t BridgeVdSwapTraceEnabled = 0;
volatile std::uint32_t BridgeVdSwapTraceCount = 0;
volatile std::uint32_t BridgeVdSwapTraceFailure = 0;
}

namespace xeo3::video {
namespace {
std::array<SwapArguments, kMaximumSwapTraces> g_arguments{};
SRWLOCK g_fileLock = SRWLOCK_INIT;

void Record(const std::uint32_t sequence, const char* phase,
            const std::uint8_t* guestMemory, const std::uint32_t result,
            const std::uint32_t lr) noexcept {
    const auto& arguments = g_arguments[sequence - 1];
    std::array<std::uint32_t, 64> ring{};
    std::array<std::uint32_t, 6> fetch{};
    std::array<std::uint32_t, 32> stack{};
    std::array<std::uint32_t, 3> pointed{};
    const bool ringReadable = detail::ReadGuestWords(
        guestMemory, arguments.registers[0], ring.data(), ring.size());
    const bool fetchReadable = detail::ReadGuestWords(
        guestMemory, arguments.registers[1], fetch.data(), fetch.size());
    const bool stackReadable = detail::ReadGuestWords(
        guestMemory, arguments.stack, stack.data(), stack.size());
    bool pointedReadable = true;
    for (std::size_t index = 0; index < pointed.size(); ++index) {
        pointedReadable = detail::ReadGuestWords(
            guestMemory, arguments.registers[index + 5], &pointed[index], 1) &&
            pointedReadable;
    }
    if (!ringReadable || !fetchReadable || !stackReadable || !pointedReadable) {
        BridgeVdSwapTraceFailure = ERROR_NOACCESS;
    }

    char line[8192]{};
    const int header = std::snprintf(
        line, std::size(line),
        "{\"sequence\":%u,\"phase\":\"%s\",\"pid\":%lu,\"tid\":%lu,"
        "\"tick_ms\":%llu,\"caller_lr\":%u,\"stack_address\":%u,"
        "\"result_r3\":%u,\"return_lr\":%u,\"ring_readable\":%s,"
        "\"fetch_readable\":%s,\"stack_readable\":%s,\"pointed_readable\":%s",
        sequence, phase, GetCurrentProcessId(), GetCurrentThreadId(),
        static_cast<unsigned long long>(GetTickCount64()), arguments.callerLr,
        arguments.stack, result, lr, ringReadable ? "true" : "false",
        fetchReadable ? "true" : "false", stackReadable ? "true" : "false",
        pointedReadable ? "true" : "false");
    if (header < 0 || static_cast<std::size_t>(header) >= std::size(line)) {
        BridgeVdSwapTraceFailure = ERROR_INSUFFICIENT_BUFFER;
        return;
    }
    std::size_t used = static_cast<std::size_t>(header);
    const auto appendWords = [&](const char* name, const auto& words) {
        int written = std::snprintf(line + used, std::size(line) - used,
                                    ",\"%s\":[", name);
        if (written < 0 || static_cast<std::size_t>(written) >= std::size(line) - used)
            return false;
        used += static_cast<std::size_t>(written);
        for (std::size_t index = 0; index < words.size(); ++index) {
            written = std::snprintf(line + used, std::size(line) - used,
                                    "%s%u", index == 0 ? "" : ",", words[index]);
            if (written < 0 || static_cast<std::size_t>(written) >= std::size(line) - used)
                return false;
            used += static_cast<std::size_t>(written);
        }
        if (used + 1 >= std::size(line)) return false;
        line[used++] = ']';
        line[used] = '\0';
        return true;
    };
    if (!appendWords("gpr3_to_10", arguments.registers) ||
        !appendWords("pointed_frontbuffer_format_colorspace", pointed) ||
        !appendWords("fetch_words", fetch) || !appendWords("ring_words", ring) ||
        !appendWords("stack_words", stack) || used + 2 >= std::size(line)) {
        BridgeVdSwapTraceFailure = ERROR_INSUFFICIENT_BUFFER;
        return;
    }
    line[used++] = '}';
    line[used++] = '\n';
    char path[512]{};
    const int pathLength = std::snprintf(
        path, std::size(path),
        "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\ac6-vdswap-%lu.jsonl",
        GetCurrentProcessId());
    if (pathLength < 0 || static_cast<std::size_t>(pathLength) >= std::size(path)) {
        BridgeVdSwapTraceFailure = ERROR_INSUFFICIENT_BUFFER;
        return;
    }
    AcquireSRWLockExclusive(&g_fileLock);
    const HANDLE file = CreateFileA(path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        BridgeVdSwapTraceFailure = GetLastError();
    } else {
        DWORD written = 0;
        if (!WriteFile(file, line, static_cast<DWORD>(used), &written, nullptr) ||
            written != used) {
            const DWORD error = GetLastError();
            BridgeVdSwapTraceFailure = error == 0 ? ERROR_WRITE_FAULT : error;
        }
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&g_fileLock);
}
}

bool detail::ReadGuestWords(const std::uint8_t* const guestMemory,
                           const std::uint32_t address,
                           std::uint32_t* const output,
                           const std::size_t wordCount) noexcept {
    if (output == nullptr || wordCount == 0 || wordCount > 64) return false;
    const auto size = wordCount * sizeof(std::uint32_t);
    std::memset(output, 0, size);
    if (guestMemory == nullptr || address == 0 ||
        static_cast<std::uint64_t>(address) + size > 0x100000000ULL) return false;
    const auto base = reinterpret_cast<std::uintptr_t>(guestMemory);
    if (base > (std::numeric_limits<std::uintptr_t>::max)() - address - size)
        return false;
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
            reinterpret_cast<const void*>(base + address), output, size, &read) ||
        read != size) {
        std::memset(output, 0, size);
        return false;
    }
    for (std::size_t index = 0; index < wordCount; ++index)
        output[index] = _byteswap_ulong(output[index]);
    return true;
}

void ResetSwapTrace() noexcept {
    BridgeVdSwapTraceEnabled = 0;
    BridgeVdSwapTraceCount = 0;
    BridgeVdSwapTraceFailure = 0;
    g_arguments = {};
}

std::uint32_t BeginSwapTrace(const SwapArguments& arguments,
                             const std::uint8_t* const guestMemory) noexcept {
    if (BridgeVdSwapTraceEnabled == 0) return 0;
    auto* const counter = reinterpret_cast<volatile LONG*>(&BridgeVdSwapTraceCount);
    LONG current = InterlockedCompareExchange(counter, 0, 0);
    while (current >= 0 && static_cast<std::uint32_t>(current) < kMaximumSwapTraces) {
        const LONG observed = InterlockedCompareExchange(counter, current + 1, current);
        if (observed == current) {
            const auto sequence = static_cast<std::uint32_t>(current) + 1;
            g_arguments[sequence - 1] = arguments;
            Record(sequence, "before", guestMemory, arguments.registers[0], arguments.callerLr);
            return sequence;
        }
        current = observed;
    }
    return 0;
}

void EndSwapTrace(const std::uint32_t sequence, const std::uint8_t* const guestMemory,
                  const std::uint32_t result, const std::uint32_t lr) noexcept {
    if (sequence == 0 || sequence > kMaximumSwapTraces || sequence > BridgeVdSwapTraceCount) return;
    Record(sequence, "after", guestMemory, result, lr);
}
}

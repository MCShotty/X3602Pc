#include <Windows.h>

#include <charconv>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

bool ParseUnsigned(std::string_view text, std::uint32_t& value) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() ||
        parsed > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    value = static_cast<std::uint32_t>(parsed);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "usage: debug_break_after <pid> <delay-ms>\n";
        return 2;
    }

    std::uint32_t processId = 0;
    std::uint32_t delayMilliseconds = 0;
    if (!ParseUnsigned(argv[1], processId) || processId == 0 ||
        !ParseUnsigned(argv[2], delayMilliseconds)) {
        std::cerr << "pid and delay-ms must be unsigned 32-bit integers\n";
        return 2;
    }

    Sleep(delayMilliseconds);

    HANDLE process = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (process == nullptr) {
        std::cerr << "OpenProcess failed: " << GetLastError() << '\n';
        return 3;
    }

    if (!DebugBreakProcess(process)) {
        const DWORD error = GetLastError();
        CloseHandle(process);
        std::cerr << "DebugBreakProcess failed: " << error << '\n';
        return 4;
    }

    CloseHandle(process);
    std::cout << "debug break requested for PID " << processId << " after "
              << delayMilliseconds << " ms\n";
    return 0;
}

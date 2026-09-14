#include "xeo3_bridge/ac6_g2h_trace.h"

#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <iterator>

extern "C" {
volatile std::uint32_t BridgeVgpuG2HTraceEnabled = 0;
volatile std::uint32_t BridgeVgpuG2HTraceCount = 0;
volatile std::uint32_t BridgeVgpuG2HTraceFailure = 0;
}

namespace xeo3::vgpu {
namespace {
constexpr LONG kTraceLimit = 1024;
SRWLOCK g_fileLock = SRWLOCK_INIT;
bool Read(const void* source, void* output, std::size_t bytes) noexcept {
  SIZE_T read = 0;
  return source != nullptr && ReadProcessMemory(GetCurrentProcess(), source,
      output, bytes, &read) && read == bytes;
}
}

bool detail::CaptureG2HTraceSnapshot(
    const std::uint8_t* moduleBase, const void* returnStack,
    const void* original, const void* applied, const std::size_t size,
    G2HTraceSnapshot& output) noexcept {
  output = {};
  if (moduleBase == nullptr || returnStack == nullptr || size != 0x80) return false;
  G2HTraceSnapshot candidate{};
  if (!Read(original, candidate.original.data(), sizeof(candidate.original)) ||
      !Read(applied, candidate.applied.data(), sizeof(candidate.applied))) return false;
  const auto& cb = candidate.original;
  // Endian-2 frame-texture transfers only; final native presentation is endian 0.
  if (cb != std::array<std::uint32_t, 6>{0, 2, 4, 1, 6, 14400}) return false;
  for (std::size_t index = 0; index < cb.size(); ++index)
    if (index != 1 && cb[index] != candidate.applied[index]) return false;
  if (candidate.applied[1] > 3) return false;

  std::array<std::uintptr_t, 7> stack{};
  if (!Read(returnStack, stack.data(), sizeof(stack)) ||
      stack[0] != reinterpret_cast<std::uintptr_t>(moduleBase + 0x92AA) ||
      stack[6] != reinterpret_cast<std::uintptr_t>(moduleBase + 0xCC44)) return false;
  constexpr std::array<std::uint8_t, 10> kUploadCallerPrologue{
      0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20};
  constexpr std::array<std::uint8_t, 5> kUploadCall{0xE8, 0x3A, 0xE9, 0x03, 0x00};
  constexpr std::array<std::uint8_t, 5> kG2HCall{0xE8, 0x3C, 0xC6, 0xFF, 0xFF};
  std::array<std::uint8_t, 10> prologue{};
  std::array<std::uint8_t, 5> uploadCall{}, g2hCall{};
  if (!Read(moduleBase + 0x9280, prologue.data(), prologue.size()) ||
      !Read(moduleBase + 0x92A5, uploadCall.data(), uploadCall.size()) ||
      !Read(moduleBase + 0xCC3F, g2hCall.data(), g2hCall.size()) ||
      prologue != kUploadCallerPrologue || uploadCall != kUploadCall ||
      g2hCall != kG2HCall) return false;

  // Native 0x9280 pushes its incoming RDI, then subtracts 0x20 and calls
  // 0x47BE4. Relative to that call's return address, saved RDI is +0x28 and
  // 0x9280's caller is +0x30. Current RDI instead holds GPU state.
  candidate.metadataAddress = stack[5];
  if (!Read(reinterpret_cast<const void*>(candidate.metadataAddress),
            candidate.metadata.data(), sizeof(candidate.metadata))) return false;
  const auto& metadata = candidate.metadata;
  if (metadata[1] >= 0x20000000U || metadata[18] != 1280 || metadata[19] != 720 ||
      (metadata[5] & 63) != cb[4] || ((metadata[5] >> 6) & 3) != cb[1] ||
      ((metadata[5] >> 8) & 1) != cb[3]) return false;
  output = candidate;
  return true;
}

void ResetG2HTrace() noexcept {
  BridgeVgpuG2HTraceEnabled = 0;
  BridgeVgpuG2HTraceCount = 0;
  BridgeVgpuG2HTraceFailure = 0;
}

void RecordG2HTrace(const std::uint8_t* moduleBase, const void* returnStack,
                    const std::uint64_t context, const void* original,
                    const void* applied, const std::size_t size) noexcept {
  if (BridgeVgpuG2HTraceEnabled == 0 || BridgeVgpuG2HTraceCount >= kTraceLimit) return;
  G2HTraceSnapshot snapshot{};
  if (!detail::CaptureG2HTraceSnapshot(moduleBase, returnStack, original, applied, size, snapshot)) return;
  auto* count = reinterpret_cast<volatile LONG*>(&BridgeVgpuG2HTraceCount);
  LONG previous = InterlockedCompareExchange(count, 0, 0);
  for (;;) {
    if (previous >= kTraceLimit) return;
    const LONG observed = InterlockedCompareExchange(count, previous + 1, previous);
    if (observed == previous) break;
    previous = observed;
  }
  char line[2048]{};
  int length = std::snprintf(line, std::size(line),
      "{\"sequence\":%ld,\"pid\":%lu,\"tid\":%lu,\"tick_ms\":%llu,"
      "\"context\":%llu,\"metadata_address\":%llu,\"source_physical\":%u,"
      "\"original_endian\":%u,\"applied_endian\":%u,\"metadata_words\":[",
      previous + 1, GetCurrentProcessId(), GetCurrentThreadId(),
      static_cast<unsigned long long>(GetTickCount64()),
      static_cast<unsigned long long>(context),
      static_cast<unsigned long long>(snapshot.metadataAddress), snapshot.metadata[1],
      snapshot.original[1], snapshot.applied[1]);
  if (length < 0 || static_cast<std::size_t>(length) >= std::size(line)) return;
  auto used = static_cast<std::size_t>(length);
  for (std::size_t index = 0; index < snapshot.metadata.size(); ++index) {
    length = std::snprintf(line + used, std::size(line) - used, "%s%u",
                           index == 0 ? "" : ",", snapshot.metadata[index]);
    if (length < 0 || static_cast<std::size_t>(length) >= std::size(line) - used) {
      BridgeVgpuG2HTraceFailure = ERROR_INSUFFICIENT_BUFFER;
      return;
    }
    used += static_cast<std::size_t>(length);
  }
  if (used + 3 >= std::size(line)) return;
  std::memcpy(line + used, "]}\n", 3);
  used += 3;
  char path[512]{};
  length = std::snprintf(path, std::size(path),
      "D:\\Games\\AC6 shit\\XeO3-AC6-lab\\ProbeLogs\\ac6-g2h-context-%lu.jsonl",
      GetCurrentProcessId());
  if (length < 0 || static_cast<std::size_t>(length) >= std::size(path)) return;
  AcquireSRWLockExclusive(&g_fileLock);
  const HANDLE file = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
      nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    BridgeVgpuG2HTraceFailure = GetLastError();
  } else {
    DWORD written = 0;
    if (!WriteFile(file, line, static_cast<DWORD>(used), &written, nullptr) || written != used)
      BridgeVgpuG2HTraceFailure = ERROR_WRITE_FAULT;
    CloseHandle(file);
  }
  ReleaseSRWLockExclusive(&g_fileLock);
}
}

// Generate local diagnostic HLSL with the production bridge's patch functions.
// No emulator, device, or runtime hook is initialized by this utility.
#include "xeo3_bridge/xeo3_vgpu_patch.h"

#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <string>

int wmain(int argc, wchar_t **argv) {
  if ((argc != 4 && argc != 5) || (std::wcscmp(argv[1], L"index") != 0 &&
                    std::wcscmp(argv[1], L"restart") != 0)) {
    std::puts("usage: patch-xeo3-shader index|restart input.hlsl output.hlsl "
              "[restart-scan-limit:1..256]");
    return 2;
  }
  std::uint32_t scanLimit = 64;
  if (argc == 5) {
    wchar_t *end = nullptr;
    const auto parsed = std::wcstoul(argv[4], &end, 10);
    if (end == argv[4] || *end != L'\0' || parsed == 0 || parsed > 256) {
      std::puts("Invalid restart scan limit.");
      return 2;
    }
    scanLimit = static_cast<std::uint32_t>(parsed);
  }
  const std::filesystem::path inputPath(argv[2]);
  const std::filesystem::path outputPath(argv[3]);
  if (std::filesystem::exists(outputPath)) {
    std::puts("Output already exists; use a new path to preserve evidence.");
    return 2;
  }
  std::ifstream input(inputPath, std::ios::binary | std::ios::ate);
  if (!input || input.tellg() <= 0 || input.tellg() > 64 * 1024 * 1024) {
    std::puts("Invalid or oversized input.");
    return 2;
  }
  std::string source(static_cast<std::size_t>(input.tellg()), '\0');
  input.seekg(0);
  input.read(source.data(), static_cast<std::streamsize>(source.size()));
  if (!input) {
    return 2;
  }
  std::string patched;
  std::uint32_t sites = 0;
  if (!xeo3::vgpu::detail::PatchXenosIndexBufferSemantics(
          source.data(), source.size(), patched, sites,
          std::wcscmp(argv[1], L"restart") == 0, scanLimit) || sites == 0) {
    std::puts("Patch rejected or already applied; no output written.");
    return 3;
  }
  std::ofstream output(outputPath, std::ios::binary);
  output.write(patched.data(), static_cast<std::streamsize>(patched.size()));
  output.close();
  if (!output) {
    return 4;
  }
  std::printf("{\"input_bytes\":%zu,\"output_bytes\":%zu,\"patched_sites\":%u}\n",
              source.size(), patched.size(), sites);
  return 0;
}

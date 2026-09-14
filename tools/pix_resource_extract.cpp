#include <windows.h>
#include <compressapi.h>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string_view>
#include <vector>

#pragma comment(lib, "Cabinet.lib")

namespace {

bool ParseUnsigned(const char *text, std::uint64_t &value) {
  const std::string_view input(text);
  const auto result =
      std::from_chars(input.data(), input.data() + input.size(), value);
  return result.ec == std::errc{} && result.ptr == input.data() + input.size();
}

struct ScopedDecompressor {
  DECOMPRESSOR_HANDLE handle = nullptr;

  ~ScopedDecompressor() {
    if (handle != nullptr) {
      CloseDecompressor(handle);
    }
  }
};

} // namespace

int main(int argc, char **argv) {
  if (argc < 7 || (argc - 4) % 3 != 0) {
    std::cerr << "usage: pix_resource_extract <resources.bin> "
                 "<compressed-offset> <compressed-size> "
                 "<slice-offset> <slice-size> <output> [...]\n";
    return 2;
  }

  std::uint64_t compressedOffset = 0;
  std::uint64_t compressedSize64 = 0;
  if (!ParseUnsigned(argv[2], compressedOffset) ||
      !ParseUnsigned(argv[3], compressedSize64) ||
      compressedSize64 > (std::numeric_limits<std::size_t>::max)()) {
    std::cerr << "invalid compressed offset or size\n";
    return 2;
  }

  std::ifstream input(argv[1], std::ios::binary);
  if (!input) {
    std::cerr << "unable to open input\n";
    return 3;
  }
  input.seekg(static_cast<std::streamoff>(compressedOffset));
  std::vector<std::uint8_t> compressed(
      static_cast<std::size_t>(compressedSize64));
  input.read(reinterpret_cast<char *>(compressed.data()),
             static_cast<std::streamsize>(compressed.size()));
  if (input.gcount() != static_cast<std::streamsize>(compressed.size())) {
    std::cerr << "short compressed read\n";
    return 3;
  }

  ScopedDecompressor decompressor;
  if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, nullptr,
                          &decompressor.handle)) {
    std::cerr << "CreateDecompressor failed: " << GetLastError() << '\n';
    return 4;
  }

  SIZE_T decompressedSize = 0;
  if (Decompress(decompressor.handle, compressed.data(), compressed.size(),
                 nullptr, 0, &decompressedSize) ||
      GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    std::cerr << "decompressed-size query failed: " << GetLastError() << '\n';
    return 4;
  }

  std::vector<std::uint8_t> decompressed(decompressedSize);
  SIZE_T actualSize = 0;
  if (!Decompress(decompressor.handle, compressed.data(), compressed.size(),
                  decompressed.data(), decompressed.size(), &actualSize)) {
    std::cerr << "decompression failed: " << GetLastError() << '\n';
    return 4;
  }
  decompressed.resize(actualSize);

  for (int argument = 4; argument < argc; argument += 3) {
    std::uint64_t sliceOffset = 0;
    std::uint64_t sliceSize = 0;
    if (!ParseUnsigned(argv[argument], sliceOffset) ||
        !ParseUnsigned(argv[argument + 1], sliceSize) ||
        sliceOffset > decompressed.size() ||
        sliceSize > decompressed.size() - sliceOffset) {
      std::cerr << "invalid slice at argument " << argument << '\n';
      return 5;
    }

    const std::filesystem::path outputPath(argv[argument + 2]);
    if (outputPath.has_parent_path()) {
      std::filesystem::create_directories(outputPath.parent_path());
    }
    std::ofstream output(outputPath, std::ios::binary);
    output.write(
        reinterpret_cast<const char *>(decompressed.data() + sliceOffset),
        static_cast<std::streamsize>(sliceSize));
    if (!output) {
      std::cerr << "unable to write " << outputPath << '\n';
      return 5;
    }
    std::cout << outputPath.string() << '\t' << sliceOffset << '\t'
              << sliceSize << '\n';
  }

  return 0;
}

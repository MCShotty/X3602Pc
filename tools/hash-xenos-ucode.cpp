#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#define XXH_INLINE_ALL
#include "xxhash.h"

namespace {

std::vector<std::uint8_t> ReadFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("unable to open input");
  }
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

std::uint64_t Hash(const std::vector<std::uint8_t>& bytes) {
  return XXH3_64bits(bytes.data(), bytes.size());
}

void SwapDwords(std::vector<std::uint8_t>& bytes) {
  const std::size_t aligned_size = bytes.size() & ~std::size_t{3};
  for (std::size_t offset = 0; offset < aligned_size; offset += 4) {
    std::swap(bytes[offset], bytes[offset + 3]);
    std::swap(bytes[offset + 1], bytes[offset + 2]);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: hash-xenos-ucode <ucode.bin> [...]\n";
    return 2;
  }

  std::cout << "path\tsize\txxh3_raw\txxh3_dword_swapped\n";
  bool failed = false;
  for (int index = 1; index < argc; ++index) {
    const std::filesystem::path path(argv[index]);
    try {
      auto bytes = ReadFile(path);
      const std::uint64_t raw_hash = Hash(bytes);
      SwapDwords(bytes);
      const std::uint64_t swapped_hash = Hash(bytes);
      std::cout << path.string() << '\t' << bytes.size() << '\t'
                << std::uppercase << std::hex << std::setw(16)
                << std::setfill('0') << raw_hash << '\t' << std::setw(16)
                << swapped_hash << std::dec << '\n';
    } catch (const std::exception& exception) {
      failed = true;
      std::cerr << path.string() << ": " << exception.what() << '\n';
    }
  }
  return failed ? 1 : 0;
}

// Tiled-address functions adapted from Xenia's
// src/xenia/gpu/shaders/texture_address.xesli at
// 95a5c3ee250f80c3b9d139658649d9ffb6db3eec.
// Copyright 2022 Ben Vanik. All rights reserved.
// Released under the BSD license; see licenses/Xenia.txt and
// THIRD_PARTY_NOTICES.md in this repository.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::uint32_t ParseU32(const char* text) {
  const auto value = std::stoull(text, nullptr, 0);
  if (value > UINT32_MAX) {
    throw std::out_of_range("integer is larger than uint32");
  }
  return static_cast<std::uint32_t>(value);
}

std::uint32_t XenosTextureTiledAddressCombine(
    const std::int32_t outerInnerBytes, const std::int32_t bank,
    const std::int32_t pipe, const std::int32_t yLsb) {
  return static_cast<std::uint32_t>(
      (yLsb << 4) | (pipe << 6) | (bank << 11) |
      (outerInnerBytes & 0xF) | (((outerInnerBytes >> 4) & 1) << 5) |
      (((outerInnerBytes >> 5) & 7) << 8) |
      ((outerInnerBytes >> 8) << 12));
}

std::uint32_t XenosTextureTiledAddress2D(
    const std::int32_t x, const std::int32_t y,
    const std::uint32_t pitchMacroTiles,
    const std::uint32_t bytesPerPixelLog2) {
  const auto outerBlocks =
      (((y >> 5) * static_cast<std::int32_t>(pitchMacroTiles)) + (x >> 5))
      << 6;
  const auto innerBlocks = (((y >> 1) & 7) << 3) | (x & 7);
  const auto outerInnerBytes =
      (outerBlocks | innerBlocks) << bytesPerPixelLog2;
  const auto bank = (y >> 4) & 1;
  const auto pipe = ((x >> 3) & 3) ^ (((y >> 3) & 1) << 1);
  return XenosTextureTiledAddressCombine(outerInnerBytes, bank, pipe, y & 1);
}

std::uint32_t ByteSwap32(const std::uint32_t value) {
  return ((value & 0x000000FFu) << 24) |
         ((value & 0x0000FF00u) << 8) |
         ((value & 0x00FF0000u) >> 8) |
         ((value & 0xFF000000u) >> 24);
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 6) {
      std::cerr << "Usage: decode-xenia-tiled-rgba8 <input.bin> <output.rgba> "
                   "<width> <height> <pitch>\n";
      return 2;
    }

    const std::filesystem::path inputPath(argv[1]);
    const std::filesystem::path outputPath(argv[2]);
    const auto width = ParseU32(argv[3]);
    const auto height = ParseU32(argv[4]);
    const auto pitch = ParseU32(argv[5]);
    if (width == 0 || height == 0 || pitch < width || (pitch & 31u) != 0) {
      throw std::invalid_argument("invalid texture dimensions or pitch");
    }

    std::ifstream input(inputPath, std::ios::binary | std::ios::ate);
    if (!input) {
      throw std::runtime_error("unable to open input");
    }
    const auto inputSize = static_cast<std::size_t>(input.tellg());
    input.seekg(0);
    std::vector<std::uint8_t> source(inputSize);
    input.read(reinterpret_cast<char*>(source.data()),
               static_cast<std::streamsize>(source.size()));
    if (!input) {
      throw std::runtime_error("unable to read input");
    }

    std::vector<std::uint8_t> output(
        static_cast<std::size_t>(width) * height * 4u);
    const auto pitchMacroTiles = pitch >> 5;
    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        const auto sourceOffset = XenosTextureTiledAddress2D(
            static_cast<std::int32_t>(x), static_cast<std::int32_t>(y),
            pitchMacroTiles, 2);
        if (sourceOffset > source.size() || source.size() - sourceOffset < 4) {
          throw std::runtime_error("tiled address lies outside input");
        }
        std::uint32_t packed = 0;
        std::copy_n(source.data() + sourceOffset, 4,
                    reinterpret_cast<std::uint8_t*>(&packed));
        packed = ByteSwap32(packed);  // RB_COPY_DEST_ENDIAN = 8-in-32.
        const auto outputOffset =
            (static_cast<std::size_t>(y) * width + x) * 4u;
        output[outputOffset + 0] = static_cast<std::uint8_t>(packed);
        output[outputOffset + 1] = static_cast<std::uint8_t>(packed >> 8);
        output[outputOffset + 2] = static_cast<std::uint8_t>(packed >> 16);
        output[outputOffset + 3] = static_cast<std::uint8_t>(packed >> 24);
      }
    }

    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream outputFile(outputPath, std::ios::binary | std::ios::trunc);
    outputFile.write(reinterpret_cast<const char*>(output.data()),
                     static_cast<std::streamsize>(output.size()));
    if (!outputFile) {
      throw std::runtime_error("unable to write output");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "decode-xenia-tiled-rgba8: " << error.what() << '\n';
    return 1;
  }
}

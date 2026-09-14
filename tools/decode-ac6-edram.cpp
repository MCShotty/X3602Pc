#include <array>
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

std::uint32_t EdramWordAddress(const std::uint32_t x,
                               const std::uint32_t y,
                               const std::uint32_t sample,
                               const std::uint32_t pitch_tiles,
                               const std::uint32_t base_tiles) {
  const std::uint32_t even_x = x & ~1u;
  const std::uint32_t doubled_x_parity =
      (2u * (x - even_x)) & 0x01FFFFFEu;
  const std::uint32_t doubled_y = y << 1u;
  std::uint32_t sample_x = doubled_x_parity + (sample >> 1u);
  std::uint32_t sample_y = (doubled_y & 2u) | (sample & 1u);
  if ((sample_x - 1u) < 2u) {
    sample_x ^= 3u;
  }
  if ((sample_y - 1u) < 2u) {
    sample_y ^= 3u;
  }

  const std::uint32_t tile_x = x / 40u;
  const std::uint32_t tile_y = y >> 3u;
  const std::uint32_t within_x = even_x % 40u;
  const std::uint32_t within_y =
      (sample_y & 3u) | (doubled_y & 12u);
  const std::uint32_t tile =
      (tile_y * pitch_tiles + base_tiles + tile_x) & 2047u;
  return tile * 1280u + 2u * (within_y * 40u + within_x) + sample_x;
}

#pragma pack(push, 1)
struct BitmapFileHeader {
  std::uint16_t type = 0x4D42;
  std::uint32_t size = 0;
  std::uint16_t reserved_1 = 0;
  std::uint16_t reserved_2 = 0;
  std::uint32_t pixel_offset = 54;
};

struct BitmapInfoHeader {
  std::uint32_t size = 40;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::uint16_t planes = 1;
  std::uint16_t bits_per_pixel = 32;
  std::uint32_t compression = 0;
  std::uint32_t image_size = 0;
  std::int32_t x_pixels_per_meter = 2835;
  std::int32_t y_pixels_per_meter = 2835;
  std::uint32_t colors_used = 0;
  std::uint32_t important_colors = 0;
};
#pragma pack(pop)

static_assert(sizeof(BitmapFileHeader) == 14);
static_assert(sizeof(BitmapInfoHeader) == 40);

void WriteBitmap(const std::filesystem::path& path,
                 const std::uint32_t width,
                 const std::uint32_t height,
                 const std::vector<std::uint8_t>& rgba) {
  const auto image_size = static_cast<std::uint32_t>(
      static_cast<std::uint64_t>(width) * height * 4u);
  BitmapFileHeader file_header;
  file_header.size = file_header.pixel_offset + image_size;
  BitmapInfoHeader info_header;
  info_header.width = static_cast<std::int32_t>(width);
  info_header.height = -static_cast<std::int32_t>(height);
  info_header.image_size = image_size;

  std::vector<std::uint8_t> bgra(image_size);
  for (std::size_t i = 0; i < rgba.size(); i += 4) {
    bgra[i + 0] = rgba[i + 2];
    bgra[i + 1] = rgba[i + 1];
    bgra[i + 2] = rgba[i + 0];
    bgra[i + 3] = rgba[i + 3];
  }

  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
  output.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));
  output.write(reinterpret_cast<const char*>(bgra.data()),
               static_cast<std::streamsize>(bgra.size()));
  if (!output) {
    throw std::runtime_error("unable to write bitmap");
  }
}

std::array<std::uint8_t, 4> Unpack(const std::uint32_t packed) {
  return {
      static_cast<std::uint8_t>(packed),
      static_cast<std::uint8_t>(packed >> 8u),
      static_cast<std::uint8_t>(packed >> 16u),
      static_cast<std::uint8_t>(packed >> 24u),
  };
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc != 7) {
      std::cerr << "Usage: decode-ac6-edram <input.bin> <output-prefix> "
                   "<width> <height> <pitch-tiles> <base-tiles>\n";
      return 2;
    }

    const std::filesystem::path input_path(argv[1]);
    const std::filesystem::path output_prefix(argv[2]);
    const auto width = ParseU32(argv[3]);
    const auto height = ParseU32(argv[4]);
    const auto pitch_tiles = ParseU32(argv[5]);
    const auto base_tiles = ParseU32(argv[6]);
    if (width == 0 || height == 0 || pitch_tiles == 0 || base_tiles >= 2048) {
      throw std::invalid_argument("invalid dimensions or tile geometry");
    }

    std::ifstream input(input_path, std::ios::binary | std::ios::ate);
    if (!input) {
      throw std::runtime_error("unable to open input");
    }
    const auto byte_count = static_cast<std::size_t>(input.tellg());
    if ((byte_count & 3u) != 0) {
      throw std::runtime_error("EDRAM dump length is not dword-aligned");
    }
    input.seekg(0);
    std::vector<std::uint32_t> edram(byte_count / 4u);
    input.read(reinterpret_cast<char*>(edram.data()),
               static_cast<std::streamsize>(byte_count));
    if (!input) {
      throw std::runtime_error("unable to read input");
    }

    const auto pixel_count = static_cast<std::size_t>(width) * height;
    std::array<std::vector<std::uint8_t>, 4> samples;
    for (auto& sample : samples) {
      sample.resize(pixel_count * 4u);
    }
    std::vector<std::uint8_t> average(pixel_count * 4u);

    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        const auto pixel = static_cast<std::size_t>(y) * width + x;
        std::array<std::uint32_t, 4> sums = {};
        for (std::uint32_t sample = 0; sample < 4; ++sample) {
          const auto address =
              EdramWordAddress(x, y, sample, pitch_tiles, base_tiles);
          if (address >= edram.size()) {
            throw std::runtime_error("EDRAM address lies outside input");
          }
          const auto channels = Unpack(edram[address]);
          for (std::size_t channel = 0; channel < channels.size(); ++channel) {
            samples[sample][pixel * 4u + channel] = channels[channel];
            sums[channel] += channels[channel];
          }
        }
        for (std::size_t channel = 0; channel < sums.size(); ++channel) {
          average[pixel * 4u + channel] =
              static_cast<std::uint8_t>((sums[channel] + 2u) / 4u);
        }
      }
    }

    for (std::uint32_t sample = 0; sample < 4; ++sample) {
      WriteBitmap(
          output_prefix.string() + "-sample" + std::to_string(sample) + ".bmp",
          width, height, samples[sample]);
    }
    WriteBitmap(output_prefix.string() + "-average.bmp", width, height, average);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "decode-ac6-edram: " << error.what() << '\n';
    return 1;
  }
}

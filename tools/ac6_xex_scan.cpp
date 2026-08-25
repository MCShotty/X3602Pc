#include <file.h>
#include <image.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <string_view>

namespace
{
struct Pattern
{
    std::string_view name;
    std::span<const std::uint8_t> bytes;
};

constexpr std::array<std::uint8_t, 4> kRestGpr14 = { 0xE9, 0xC1, 0xFF, 0x68 };
constexpr std::array<std::uint8_t, 4> kSaveGpr14 = { 0xF9, 0xC1, 0xFF, 0x68 };
constexpr std::array<std::uint8_t, 4> kRestFpr14 = { 0xC9, 0xCC, 0xFF, 0x70 };
constexpr std::array<std::uint8_t, 4> kSaveFpr14 = { 0xD9, 0xCC, 0xFF, 0x70 };
constexpr std::array<std::uint8_t, 8> kRestVmx14 = {
    0x39, 0x60, 0xFE, 0xE0, 0x7D, 0xCB, 0x60, 0xCE
};
constexpr std::array<std::uint8_t, 8> kSaveVmx14 = {
    0x39, 0x60, 0xFE, 0xE0, 0x7D, 0xCB, 0x61, 0xCE
};
constexpr std::array<std::uint8_t, 8> kRestVmx64 = {
    0x39, 0x60, 0xFC, 0x00, 0x10, 0x0B, 0x60, 0xCB
};
constexpr std::array<std::uint8_t, 8> kSaveVmx64 = {
    0x39, 0x60, 0xFC, 0x00, 0x10, 0x0B, 0x61, 0xCB
};

constexpr std::array<Pattern, 8> kPatterns = {
    Pattern{ "restgprlr_14_address", kRestGpr14 },
    Pattern{ "savegprlr_14_address", kSaveGpr14 },
    Pattern{ "restfpr_14_address", kRestFpr14 },
    Pattern{ "savefpr_14_address", kSaveFpr14 },
    Pattern{ "restvmx_14_address", kRestVmx14 },
    Pattern{ "savevmx_14_address", kSaveVmx14 },
    Pattern{ "restvmx_64_address", kRestVmx64 },
    Pattern{ "savevmx_64_address", kSaveVmx64 },
};

std::size_t CountMatches(
    const std::span<const std::uint8_t> image,
    const std::span<const std::uint8_t> pattern)
{
    if (pattern.empty() || pattern.size() > image.size())
    {
        return 0;
    }

    std::size_t count = 0;
    for (std::size_t offset = 0; offset <= image.size() - pattern.size(); ++offset)
    {
        if (std::equal(pattern.begin(), pattern.end(), image.begin() + offset))
        {
            ++count;
        }
    }
    return count;
}
}

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        std::cerr << "Usage: ac6_xex_scan <input.xex> <output.tsv> [code-image.bin]\n";
        return EXIT_FAILURE;
    }

    const auto file = LoadFile(argv[1]);
    if (file.empty())
    {
        std::cerr << "Unable to read XEX input.\n";
        return EXIT_FAILURE;
    }

    auto image = Image::ParseImage(file.data(), file.size());
    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!output)
    {
        std::cerr << "Unable to open scan output.\n";
        return EXIT_FAILURE;
    }

    output << "name\taddress\toffset\tsection\tmatch_count\n";
    bool unique = true;
    for (const auto& pattern : kPatterns)
    {
        std::size_t matchCount = 0;
        for (const auto& section : image.sections)
        {
            if ((section.flags & SectionFlags_Code) == 0)
            {
                continue;
            }
            matchCount += CountMatches(
                std::span<const std::uint8_t>(section.data, section.size),
                pattern.bytes);
        }
        unique = unique && matchCount == 1;

        if (matchCount == 0)
        {
            output << pattern.name << "\tMISSING\tMISSING\tMISSING\t0\n";
            continue;
        }

        for (const auto& section : image.sections)
        {
            if ((section.flags & SectionFlags_Code) == 0)
            {
                continue;
            }

            const std::span<const std::uint8_t> sectionBytes(section.data, section.size);
            if (pattern.bytes.size() > sectionBytes.size())
            {
                continue;
            }

            for (std::size_t offset = 0;
                 offset <= sectionBytes.size() - pattern.bytes.size();
                 ++offset)
            {
                if (!std::equal(
                        pattern.bytes.begin(),
                        pattern.bytes.end(),
                        sectionBytes.begin() + offset))
                {
                    continue;
                }

                const auto address = section.base + offset;
                output << pattern.name << "\t0x"
                       << std::uppercase << std::hex << std::setfill('0') << std::setw(8)
                       << address
                       << "\t0x" << std::setw(8) << (address - image.base)
                       << "\t" << section.name
                       << "\t" << std::dec << matchCount << "\n";
            }
        }
    }

    if (argc == 4)
    {
        std::ofstream codeImage(argv[3], std::ios::binary | std::ios::trunc);
        if (!codeImage)
        {
            std::cerr << "Unable to open code-image output.\n";
            return EXIT_FAILURE;
        }

        codeImage.seekp(static_cast<std::streamoff>(image.size - 1));
        codeImage.put('\0');
        for (const auto& section : image.sections)
        {
            if ((section.flags & SectionFlags_Code) == 0)
            {
                continue;
            }

            const auto offset = section.base - image.base;
            if (offset > image.size || section.size > image.size - offset)
            {
                std::cerr << "Executable section is outside the image: " << section.name << "\n";
                return EXIT_FAILURE;
            }

            codeImage.seekp(static_cast<std::streamoff>(offset));
            codeImage.write(
                reinterpret_cast<const char*>(section.data),
                static_cast<std::streamsize>(section.size));
        }

        if (!codeImage)
        {
            std::cerr << "Unable to write code-image output.\n";
            return EXIT_FAILURE;
        }
    }

    std::cout << "image_base=0x" << std::uppercase << std::hex << image.base
              << " image_size=0x" << image.size
              << " helper_signatures=" << std::dec << kPatterns.size()
              << " all_unique=" << (unique ? "true" : "false") << "\n";
    return unique ? EXIT_SUCCESS : EXIT_FAILURE;
}

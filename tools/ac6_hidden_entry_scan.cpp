#include <cstdint>
#include <malloc.h>

#include <byteswap.h>
#include <file.h>
#include <image.h>
#include <ppc.h>
#include <xbox.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <string>

namespace
{
std::size_t ReadableImageSize(const Image& image)
{
    if (image.data == nullptr)
    {
        return 0;
    }

    const auto allocationSize = _msize(image.data.get());
    return std::min<std::size_t>(image.size, allocationSize);
}

bool IsImageAddress(const Image& image, const std::uint32_t address)
{
    const auto upper = image.sections.upper_bound(address);
    if (upper == image.sections.begin())
    {
        return false;
    }

    const auto section = std::prev(upper);
    return address >= section->base &&
        address < section->base + section->size;
}

std::uint32_t ReadableSectionSize(
    const Image& image,
    const Section& section)
{
    if (section.base < image.base)
    {
        return 0;
    }

    const auto offset = section.base - image.base;
    const auto imageSize = ReadableImageSize(image);
    if (offset >= imageSize)
    {
        return 0;
    }

    return static_cast<std::uint32_t>(
        std::min<std::size_t>(section.size, imageSize - offset));
}

const std::uint8_t* ReadableSectionData(
    const Image& image,
    const Section& section)
{
    if (ReadableSectionSize(image, section) == 0)
    {
        return nullptr;
    }

    return image.data.get() + (section.base - image.base);
}

bool IsCodeAddress(const Image& image, const std::uint32_t address)
{
    const auto upper = image.sections.upper_bound(address);
    if (upper == image.sections.begin())
    {
        return false;
    }

    const auto section = std::prev(upper);
    return address >= section->base &&
        address < section->base + section->size &&
        (section->flags & SectionFlags_Code) != 0;
}

std::set<std::uint32_t> LoadMappedFunctions(const char* path)
{
    std::ifstream input(path);
    std::set<std::uint32_t> mappings;
    std::string line;
    while (std::getline(input, line))
    {
        const auto prefix = line.find("0x");
        if (prefix == std::string::npos)
        {
            continue;
        }

        std::uint32_t address = 0;
        const auto begin = line.data() + prefix + 2;
        const auto end = begin + std::min<std::size_t>(8, line.data() + line.size() - begin);
        const auto result = std::from_chars(begin, end, address, 16);
        if (result.ec == std::errc{} && result.ptr == end)
        {
            mappings.emplace(address);
        }
    }
    return mappings;
}

constexpr bool IsTerminal(const std::uint32_t instruction)
{
    if (instruction == 0 ||
        instruction == 0x4E800020 ||
        instruction == 0x4E800420)
    {
        return true;
    }

    return PPC_OP(instruction) == PPC_OP_B && !PPC_BL(instruction);
}

static_assert(IsTerminal(0x4E800020));
static_assert(IsTerminal(0x4E800420));
static_assert(!IsTerminal(0x4E800421));

std::uint32_t EstimateLinearSize(
    const std::uint8_t* data,
    const std::uint32_t remainingSize)
{
    for (std::uint32_t offset = 0;
         offset + 4 <= remainingSize;
         offset += 4)
    {
        const auto instruction = ByteSwap(
            *reinterpret_cast<const std::uint32_t*>(data + offset));
        if (IsTerminal(instruction))
        {
            return offset + 4;
        }
    }
    return remainingSize;
}

std::set<std::uint32_t> FindConstructedAddresses(const Image& image)
{
    std::set<std::uint32_t> addresses;
    for (const auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
        {
            continue;
        }

        const auto readableSize = ReadableSectionSize(image, section);
        const auto* const sectionData = ReadableSectionData(image, section);
        for (std::uint32_t offset = 0;
             offset + 8 <= readableSize;
             offset += 4)
        {
            const auto upperInstruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    sectionData + offset));
            const auto upperOpcode = upperInstruction >> 26;
            const auto upperRegister = (upperInstruction >> 21) & 31;
            const auto upperBaseRegister = (upperInstruction >> 16) & 31;
            if (upperOpcode != 15 || upperBaseRegister != 0)
            {
                continue;
            }

            const auto upper =
                static_cast<std::uint32_t>(
                    static_cast<std::uint16_t>(upperInstruction)) << 16;
            for (std::uint32_t lookahead = 4;
                 lookahead <= 32 &&
                     offset + lookahead + 4 <= readableSize;
                 lookahead += 4)
            {
                const auto lowerInstruction = ByteSwap(
                    *reinterpret_cast<const std::uint32_t*>(
                        sectionData + offset + lookahead));
                const auto lowerOpcode = lowerInstruction >> 26;
                const auto sourceRegister = (lowerInstruction >> 16) & 31;
                if (lowerOpcode == 14 && sourceRegister == upperRegister)
                {
                    const auto lower = static_cast<std::int16_t>(
                        lowerInstruction & 0xFFFF);
                    addresses.emplace(
                        upper + static_cast<std::uint32_t>(lower));
                }
                else if (
                    lowerOpcode == 24 &&
                    ((lowerInstruction >> 21) & 31) == upperRegister)
                {
                    addresses.emplace(
                        upper | (lowerInstruction & 0xFFFF));
                }
            }
        }
    }
    return addresses;
}

std::map<std::uint32_t, std::size_t> FindDataPointers(const Image& image)
{
    std::map<std::uint32_t, std::size_t> pointers;
    const auto imageSize = ReadableImageSize(image);
    for (std::uint32_t offset = 0;
         offset + 4 <= imageSize;
         offset += 4)
    {
        const auto sourceAddress =
            static_cast<std::uint32_t>(image.base + offset);
        if (IsCodeAddress(image, sourceAddress))
        {
            continue;
        }

        const auto address = ByteSwap(
            *reinterpret_cast<const std::uint32_t*>(
                image.data.get() + offset));
        if (IsImageAddress(image, address))
        {
            ++pointers[address];
        }
    }
    return pointers;
}

std::map<std::uint32_t, std::size_t> FindTableDataPointers(
    const Image& image)
{
    std::map<std::uint32_t, std::size_t> pointers;
    const auto imageSize = ReadableImageSize(image);
    for (std::uint32_t offset = 0;
         offset + 4 <= imageSize;
         offset += 4)
    {
        const auto sourceAddress =
            static_cast<std::uint32_t>(image.base + offset);
        if (IsCodeAddress(image, sourceAddress))
        {
            continue;
        }

        const auto address = ByteSwap(
            *reinterpret_cast<const std::uint32_t*>(
                image.data.get() + offset));
        if (!IsCodeAddress(image, address))
        {
            continue;
        }

        bool hasAdjacentCodePointer = false;
        for (const std::int32_t delta : {-4, 4})
        {
            const auto adjacentOffset =
                static_cast<std::int64_t>(offset) + delta;
            if (adjacentOffset < 0 ||
                adjacentOffset + 4 >
                    static_cast<std::int64_t>(imageSize))
            {
                continue;
            }

            const auto adjacentSourceAddress =
                static_cast<std::uint32_t>(
                    image.base + adjacentOffset);
            if (IsCodeAddress(image, adjacentSourceAddress))
            {
                continue;
            }

            const auto adjacentAddress = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    image.data.get() + adjacentOffset));
            if (IsCodeAddress(image, adjacentAddress))
            {
                hasAdjacentCodePointer = true;
                break;
            }
        }

        if (hasAdjacentCodePointer)
        {
            ++pointers[address];
        }
    }
    return pointers;
}

std::set<std::uint32_t> FindCodeBranchTargets(const Image& image)
{
    std::set<std::uint32_t> targets;
    for (const auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
        {
            continue;
        }

        const auto readableSize = ReadableSectionSize(image, section);
        const auto* const sectionData = ReadableSectionData(image, section);
        for (std::uint32_t offset = 0;
             offset + 4 <= readableSize;
             offset += 4)
        {
            const auto instruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    sectionData + offset));
            const auto address =
                static_cast<std::uint32_t>(section.base + offset);
            std::uint32_t target = 0;
            if (PPC_OP(instruction) == PPC_OP_B && !PPC_BL(instruction))
            {
                target = static_cast<std::uint32_t>(
                    address + PPC_BI(instruction));
            }
            else if (
                PPC_OP(instruction) == PPC_OP_BC &&
                !PPC_BL(instruction))
            {
                target = static_cast<std::uint32_t>(
                    address + PPC_BD(instruction));
            }

            if (IsImageAddress(image, target))
            {
                targets.emplace(target);
            }
        }
    }
    return targets;
}

void WriteCandidate(
    std::ostream& output,
    const std::uint32_t pdataAddress,
    const std::uint32_t pdataSize,
    const std::uint32_t candidateAddress,
    const std::uint32_t candidateSize,
    const std::uint32_t precedingInstruction,
    const bool branchTarget,
    const bool constructedAddress,
    const std::size_t dataPointerCount,
    const std::size_t tablePointerCount)
{
    output
        << "0x" << std::uppercase << std::hex
        << std::setfill('0') << std::setw(8) << pdataAddress
        << "\t0x" << std::setw(8) << pdataSize
        << "\t0x" << std::setw(8) << candidateAddress
        << "\t0x" << std::setw(8) << candidateSize
        << "\t0x" << std::setw(8) << precedingInstruction
        << "\t" << (branchTarget ? "true" : "false")
        << "\t" << (constructedAddress ? "true" : "false")
        << "\t" << std::dec << dataPointerCount
        << "\t" << tablePointerCount
        << "\n";
}
}

int main(int argc, char** argv)
{
    if (argc != 4)
    {
        std::cerr
            << "Usage: ac6_hidden_entry_scan <input.xex> <ppc_func_mapping.cpp> <output.tsv>\n";
        return EXIT_FAILURE;
    }

    const auto file = LoadFile(argv[1]);
    if (file.empty())
    {
        std::cerr << "Unable to read XEX input.\n";
        return EXIT_FAILURE;
    }

    const auto mappings = LoadMappedFunctions(argv[2]);
    if (mappings.empty())
    {
        std::cerr << "Unable to read mapped function addresses.\n";
        return EXIT_FAILURE;
    }

    const auto image = Image::ParseImage(file.data(), file.size());
    const auto constructedAddresses = FindConstructedAddresses(image);
    const auto dataPointers = FindDataPointers(image);
    const auto tableDataPointers = FindTableDataPointers(image);
    const auto codeBranchTargets = FindCodeBranchTargets(image);
    const auto* const pdata = image.Find(".pdata");
    if (pdata == nullptr)
    {
        std::cerr << "XEX image has no .pdata section.\n";
        return EXIT_FAILURE;
    }

    std::ofstream output(argv[3], std::ios::binary | std::ios::trunc);
    if (!output)
    {
        std::cerr << "Unable to open hidden-entry output.\n";
        return EXIT_FAILURE;
    }
    output
        << "pdata_address\tpdata_size\tcandidate_address\tcandidate_size"
           "\tpreceding_instruction\tlocal_branch_target"
           "\tconstructed_address\tdata_pointer_count"
           "\ttable_pointer_count\n";

    std::size_t candidateCount = 0;
    std::size_t constructedCandidateCount = 0;
    std::size_t dataPointerCandidateCount = 0;
    std::size_t gapCandidateCount = 0;
    std::size_t tablePointerCandidateCount = 0;
    std::set<std::uint32_t> emittedAddresses;
    const auto pdataCount = pdata->size / sizeof(IMAGE_CE_RUNTIME_FUNCTION);
    const auto* const runtimeFunctions =
        reinterpret_cast<const IMAGE_CE_RUNTIME_FUNCTION*>(pdata->data);
    for (std::size_t index = 0; index < pdataCount; ++index)
    {
        auto runtimeFunction = runtimeFunctions[index];
        runtimeFunction.BeginAddress = ByteSwap(runtimeFunction.BeginAddress);
        runtimeFunction.Data = ByteSwap(runtimeFunction.Data);

        const auto pdataAddress =
            static_cast<std::uint32_t>(runtimeFunction.BeginAddress);
        const auto pdataSize =
            static_cast<std::uint32_t>(runtimeFunction.FunctionLength * 4);
        if (pdataSize < 8)
        {
            continue;
        }

        const auto* const functionData =
            static_cast<const std::uint8_t*>(image.Find(pdataAddress));
        if (functionData == nullptr)
        {
            continue;
        }

        std::set<std::uint32_t> localBranchTargets;
        for (std::uint32_t offset = 0;
             offset + 4 <= pdataSize;
             offset += 4)
        {
            const auto instruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    functionData + offset));
            const auto address = pdataAddress + offset;
            std::uint32_t target = 0;
            if (PPC_OP(instruction) == PPC_OP_B && !PPC_BL(instruction))
            {
                target = static_cast<std::uint32_t>(
                    address + PPC_BI(instruction));
            }
            else if (
                PPC_OP(instruction) == PPC_OP_BC &&
                !PPC_BL(instruction))
            {
                target = static_cast<std::uint32_t>(
                    address + PPC_BD(instruction));
            }
            if (target >= pdataAddress &&
                target < pdataAddress + pdataSize)
            {
                localBranchTargets.emplace(target);
            }
        }

        for (std::uint32_t cursor = 4;
             cursor + 4 <= pdataSize;
             cursor += 4)
        {
            const auto candidateInstruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    functionData + cursor));
            if (candidateInstruction == 0)
            {
                continue;
            }

            const auto candidateAddress = pdataAddress + cursor;
            const auto precedingInstruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    functionData + cursor - 4));

            if (!mappings.contains(candidateAddress) &&
                IsTerminal(precedingInstruction))
            {
                const auto candidateSize = EstimateLinearSize(
                    functionData + cursor,
                    pdataSize - cursor);
                const auto dataPointer = dataPointers.find(candidateAddress);
                const auto dataPointerCount =
                    dataPointer == dataPointers.end()
                        ? 0
                        : dataPointer->second;
                const auto tablePointer =
                    tableDataPointers.find(candidateAddress);
                const auto tablePointerCount =
                    tablePointer == tableDataPointers.end()
                        ? 0
                        : tablePointer->second;
                WriteCandidate(
                    output,
                    pdataAddress,
                    pdataSize,
                    candidateAddress,
                    candidateSize,
                    precedingInstruction,
                    localBranchTargets.contains(candidateAddress),
                    constructedAddresses.contains(candidateAddress),
                    dataPointerCount,
                    tablePointerCount);
                emittedAddresses.emplace(candidateAddress);
                ++candidateCount;
                if (constructedAddresses.contains(candidateAddress))
                {
                    ++constructedCandidateCount;
                }
                if (dataPointerCount != 0)
                {
                    ++dataPointerCandidateCount;
                }
                if (tablePointerCount != 0)
                {
                    ++tablePointerCandidateCount;
                }
            }
        }
    }

    for (const auto& section : image.sections)
    {
        if ((section.flags & SectionFlags_Code) == 0)
        {
            continue;
        }

        const auto readableSize = ReadableSectionSize(image, section);
        const auto* const sectionData = ReadableSectionData(image, section);
        for (std::uint32_t cursor = 4;
             cursor + 4 <= readableSize;
             cursor += 4)
        {
            const auto candidateInstruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    sectionData + cursor));
            if (candidateInstruction == 0)
            {
                continue;
            }

            const auto candidateAddress =
                static_cast<std::uint32_t>(section.base + cursor);
            const auto precedingInstruction = ByteSwap(
                *reinterpret_cast<const std::uint32_t*>(
                    sectionData + cursor - 4));
            if (mappings.contains(candidateAddress) ||
                emittedAddresses.contains(candidateAddress) ||
                !IsTerminal(precedingInstruction))
            {
                continue;
            }

            const auto candidateSize = EstimateLinearSize(
                sectionData + cursor,
                readableSize - cursor);
            const auto dataPointer = dataPointers.find(candidateAddress);
            const auto dataPointerCount =
                dataPointer == dataPointers.end()
                    ? 0
                    : dataPointer->second;
            const auto tablePointer =
                tableDataPointers.find(candidateAddress);
            const auto tablePointerCount =
                tablePointer == tableDataPointers.end()
                    ? 0
                    : tablePointer->second;
            WriteCandidate(
                output,
                0,
                0,
                candidateAddress,
                candidateSize,
                precedingInstruction,
                codeBranchTargets.contains(candidateAddress),
                constructedAddresses.contains(candidateAddress),
                dataPointerCount,
                tablePointerCount);
            emittedAddresses.emplace(candidateAddress);
            ++candidateCount;
            ++gapCandidateCount;
            if (constructedAddresses.contains(candidateAddress))
            {
                ++constructedCandidateCount;
            }
            if (dataPointerCount != 0)
            {
                ++dataPointerCandidateCount;
            }
            if (tablePointerCount != 0)
            {
                ++tablePointerCandidateCount;
            }
        }
    }

    std::cout << "pdata_entries=" << std::dec << pdataCount
              << " mapped_functions=" << mappings.size()
              << " hidden_entry_candidates=" << candidateCount
              << " gap_candidates=" << gapCandidateCount
              << " constructed_candidates=" << constructedCandidateCount
              << " data_pointer_candidates=" << dataPointerCandidateCount
              << " table_pointer_candidates=" << tablePointerCandidateCount
              << "\n";
    return EXIT_SUCCESS;
}

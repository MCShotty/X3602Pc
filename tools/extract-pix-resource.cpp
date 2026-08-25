#define NOMINMAX

#include <compressapi.h>
#include <windows.h>

#include <charconv>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

class Handle {
public:
    explicit Handle(HANDLE value = INVALID_HANDLE_VALUE) noexcept : value_(value) {}
    ~Handle() {
        if (value_ != INVALID_HANDLE_VALUE) {
            CloseHandle(value_);
        }
    }

    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;

    [[nodiscard]] HANDLE get() const noexcept { return value_; }

private:
    HANDLE value_;
};

class Decompressor {
public:
    Decompressor() {
        if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS, nullptr, &value_)) {
            throw std::runtime_error("CreateDecompressor failed");
        }
    }

    ~Decompressor() {
        if (value_ != nullptr) {
            CloseDecompressor(value_);
        }
    }

    Decompressor(const Decompressor&) = delete;
    Decompressor& operator=(const Decompressor&) = delete;

    [[nodiscard]] DECOMPRESSOR_HANDLE get() const noexcept { return value_; }

private:
    DECOMPRESSOR_HANDLE value_ = nullptr;
};

[[nodiscard]] std::uint64_t ParseUnsigned(std::string_view text, const char* name) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value, 10);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
        throw std::runtime_error(std::string("Invalid ") + name + ": " + std::string(text));
    }
    return value;
}

[[nodiscard]] std::vector<std::byte> ReadCompressedBlob(
    const std::filesystem::path& path,
    std::uint64_t offset,
    std::uint64_t size) {
    if (size == 0 || size > std::numeric_limits<DWORD>::max()) {
        throw std::runtime_error("Compressed blob size is outside the supported DWORD range");
    }

    Handle file(CreateFileW(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr));
    if (file.get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("CreateFileW failed");
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(file.get(), position, nullptr, FILE_BEGIN)) {
        throw std::runtime_error("SetFilePointerEx failed");
    }

    std::vector<std::byte> compressed(static_cast<std::size_t>(size));
    std::size_t totalRead = 0;
    while (totalRead < compressed.size()) {
        const auto remaining = compressed.size() - totalRead;
        const auto chunk = static_cast<DWORD>(
            (std::min)(remaining, static_cast<std::size_t>(std::numeric_limits<DWORD>::max())));
        DWORD bytesRead = 0;
        if (!ReadFile(file.get(), compressed.data() + totalRead, chunk, &bytesRead, nullptr)) {
            throw std::runtime_error("ReadFile failed");
        }
        if (bytesRead == 0) {
            throw std::runtime_error("Unexpected end of resources.bin");
        }
        totalRead += bytesRead;
    }
    return compressed;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        if (argc != 7) {
            std::wcerr
                << L"Usage: extract-pix-resource.exe <resources.bin> <compressed-offset> "
                   L"<compressed-size> <decompressed-offset> <length> <output>\n";
            return 2;
        }

        const std::filesystem::path inputPath(argv[1]);
        const auto compressedOffset = ParseUnsigned(
            std::filesystem::path(argv[2]).string(), "compressed offset");
        const auto compressedSize = ParseUnsigned(
            std::filesystem::path(argv[3]).string(), "compressed size");
        const auto decompressedOffset = ParseUnsigned(
            std::filesystem::path(argv[4]).string(), "decompressed offset");
        const auto requestedLength = ParseUnsigned(
            std::filesystem::path(argv[5]).string(), "requested length");
        const std::filesystem::path outputPath(argv[6]);

        auto compressed = ReadCompressedBlob(inputPath, compressedOffset, compressedSize);
        Decompressor decompressor;

        SIZE_T requiredSize = 0;
        if (Decompress(
                decompressor.get(),
                compressed.data(),
                compressed.size(),
                nullptr,
                0,
                &requiredSize) ||
            GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            const auto error = GetLastError();
            throw std::runtime_error(
                "Unable to query decompressed resource size (Win32 " +
                std::to_string(error) + ")");
        }

        if (decompressedOffset > requiredSize ||
            requestedLength > requiredSize - decompressedOffset) {
            throw std::runtime_error("Requested range is outside the decompressed resource");
        }

        std::vector<std::byte> decompressed(requiredSize);
        SIZE_T decompressedSize = 0;
        if (!Decompress(
                decompressor.get(),
                compressed.data(),
                compressed.size(),
                decompressed.data(),
                decompressed.size(),
                &decompressedSize)) {
            const auto error = GetLastError();
            throw std::runtime_error(
                "XPRESS decompression failed (Win32 " + std::to_string(error) +
                ", required " + std::to_string(requiredSize) + ")");
        }
        if (decompressedSize != requiredSize) {
            throw std::runtime_error("XPRESS decompression returned an unexpected size");
        }

        std::filesystem::create_directories(outputPath.parent_path());
        std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            throw std::runtime_error("Unable to create output file");
        }
        output.write(
            reinterpret_cast<const char*>(decompressed.data() + decompressedOffset),
            static_cast<std::streamsize>(requestedLength));
        if (!output) {
            throw std::runtime_error("Unable to write the requested resource range");
        }

        std::wcout << L"decompressed_size=" << decompressedSize
                   << L" extracted_offset=" << decompressedOffset
                   << L" extracted_length=" << requestedLength << L'\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "extract-pix-resource: " << error.what() << '\n';
        return 1;
    }
}

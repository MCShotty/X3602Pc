#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cwchar>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace {
constexpr std::size_t kShaderContainerSize = 9 * sizeof(std::uint32_t);
constexpr std::uint64_t kMaximumContainerSize = 64ULL * 1024ULL * 1024ULL;
constexpr std::size_t kProcessScanChunkSize = 8ULL * 1024ULL * 1024ULL;
constexpr std::uintptr_t kGuestMemoryBase = 0x100000000ULL;
constexpr std::uintptr_t kGuestMemorySize = 0x100000000ULL;

struct MappedFile {
  HANDLE file = INVALID_HANDLE_VALUE;
  HANDLE mapping = nullptr;
  const std::uint8_t *data = nullptr;
  std::uint64_t size = 0;

  ~MappedFile() {
    if (data != nullptr) {
      UnmapViewOfFile(data);
    }
    if (mapping != nullptr) {
      CloseHandle(mapping);
    }
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
    }
  }

  MappedFile(const MappedFile &) = delete;
  MappedFile &operator=(const MappedFile &) = delete;
  MappedFile() = default;
};

struct ContainerInfo {
  std::uint32_t flags = 0;
  std::uint32_t virtualSize = 0;
  std::uint32_t physicalSize = 0;
  std::uint32_t constantTableOffset = 0;
  std::uint32_t definitionTableOffset = 0;
  std::uint32_t shaderOffset = 0;
  std::uint32_t shaderPhysicalOffset = 0;
  std::uint32_t shaderSize = 0;
  std::uint64_t totalSize = 0;
};

std::uint32_t ReadBigEndian32(const std::uint8_t *const bytes) noexcept {
  return (std::uint32_t{bytes[0]} << 24U) |
         (std::uint32_t{bytes[1]} << 16U) |
         (std::uint32_t{bytes[2]} << 8U) | std::uint32_t{bytes[3]};
}

bool OpenMappedFile(const std::filesystem::path &path,
                    MappedFile &mapped) noexcept {
  mapped.file = CreateFileW(path.c_str(), GENERIC_READ,
                            FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
                            nullptr);
  if (mapped.file == INVALID_HANDLE_VALUE) {
    return false;
  }

  LARGE_INTEGER size{};
  if (!GetFileSizeEx(mapped.file, &size) || size.QuadPart <= 0) {
    return false;
  }
  mapped.size = static_cast<std::uint64_t>(size.QuadPart);
  mapped.mapping =
      CreateFileMappingW(mapped.file, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (mapped.mapping == nullptr) {
    return false;
  }
  mapped.data = static_cast<const std::uint8_t *>(
      MapViewOfFile(mapped.mapping, FILE_MAP_READ, 0, 0, 0));
  return mapped.data != nullptr;
}

bool ParseContainerHeader(const std::uint8_t *const bytes,
                          ContainerInfo &info) noexcept {
  info = {};
  if (bytes == nullptr || bytes[0] != 0x10U || bytes[1] != 0x2AU ||
      bytes[2] != 0x11U) {
    return false;
  }

  info.flags = ReadBigEndian32(bytes);
  if ((info.flags & 0xFFFFFF00U) != 0x102A1100U) {
    return false;
  }
  info.virtualSize = ReadBigEndian32(bytes + 4);
  info.physicalSize = ReadBigEndian32(bytes + 8);
  info.constantTableOffset = ReadBigEndian32(bytes + 16);
  info.definitionTableOffset = ReadBigEndian32(bytes + 20);
  info.shaderOffset = ReadBigEndian32(bytes + 24);
  const auto field1C = ReadBigEndian32(bytes + 28);
  const auto field20 = ReadBigEndian32(bytes + 32);
  info.totalSize = std::uint64_t{info.virtualSize} + info.physicalSize;

  if (field1C != 0 || field20 != 0 ||
      info.virtualSize < kShaderContainerSize || info.physicalSize == 0 ||
      info.totalSize > kMaximumContainerSize ||
      info.constantTableOffset == 0 ||
      info.constantTableOffset + 32ULL > info.virtualSize ||
      info.shaderOffset == 0 || info.shaderOffset + 24ULL > info.virtualSize ||
      (info.definitionTableOffset != 0 &&
       info.definitionTableOffset + 24ULL > info.virtualSize)) {
    return false;
  }

  return true;
}

bool ValidateContainer(const std::uint8_t *const bytes,
                       const std::uint64_t remaining,
                       ContainerInfo &info) noexcept {
  if (remaining < kShaderContainerSize ||
      !ParseContainerHeader(bytes, info) || info.totalSize > remaining) {
    return false;
  }

  const auto *const shader = bytes + info.shaderOffset;
  info.shaderPhysicalOffset = ReadBigEndian32(shader);
  info.shaderSize = ReadBigEndian32(shader + 4);
  return info.shaderPhysicalOffset < info.physicalSize && info.shaderSize != 0 &&
         info.shaderSize <= info.physicalSize - info.shaderPhysicalOffset;
}

std::uint32_t Crc32c(const std::span<const std::uint8_t> bytes,
                     std::uint32_t crc = 0) noexcept {
  constexpr std::uint32_t kReflectedCastagnoliPolynomial = 0x82F63B78U;
  for (const auto byte : bytes) {
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit) {
      const auto mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (kReflectedCastagnoliPolynomial & mask);
    }
  }
  return crc;
}

std::uint32_t Crc32cDwordSwapped(
    const std::span<const std::uint8_t> bytes,
    std::uint32_t crc = 0) noexcept {
  const auto alignedSize = bytes.size() & ~std::size_t{3};
  for (std::size_t offset = 0; offset < alignedSize; offset += 4) {
    const std::array<std::uint8_t, 4> swapped{
        bytes[offset + 3], bytes[offset + 2], bytes[offset + 1],
        bytes[offset]};
    crc = Crc32c(swapped, crc);
  }
  return Crc32c(bytes.subspan(alignedSize), crc);
}

std::string Hex32(const std::uint32_t value) {
  std::ostringstream output;
  output << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
         << value;
  return output.str();
}

bool HashSha256(const std::span<const std::uint8_t> bytes,
                std::array<std::uint8_t, 32> &digest) noexcept {
  digest.fill(0);
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  std::vector<std::uint8_t> object;
  DWORD objectSize = 0;
  DWORD resultSize = 0;

  auto status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM,
                                            nullptr, 0);
  if (status >= 0) {
    status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH,
                               reinterpret_cast<PUCHAR>(&objectSize),
                               sizeof(objectSize), &resultSize, 0);
  }
  if (status >= 0) {
    object.resize(objectSize);
    status = BCryptCreateHash(algorithm, &hash, object.data(), objectSize,
                              nullptr, 0, 0);
  }
  if (status >= 0) {
    if (bytes.size() > ULONG_MAX) {
      status = STATUS_INVALID_PARAMETER;
    } else {
      status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()),
                              static_cast<ULONG>(bytes.size()), 0);
    }
  }
  if (status >= 0) {
    status = BCryptFinishHash(hash, digest.data(),
                              static_cast<ULONG>(digest.size()), 0);
  }
  if (hash != nullptr) {
    BCryptDestroyHash(hash);
  }
  if (algorithm != nullptr) {
    BCryptCloseAlgorithmProvider(algorithm, 0);
  }
  return status >= 0;
}

bool HashSha256DwordSwapped(
    const std::span<const std::uint8_t> bytes,
    std::array<std::uint8_t, 32> &digest) noexcept {
  try {
    std::vector<std::uint8_t> swapped(bytes.begin(), bytes.end());
    const auto alignedSize = swapped.size() & ~std::size_t{3};
    for (std::size_t offset = 0; offset < alignedSize; offset += 4) {
      std::swap(swapped[offset], swapped[offset + 3]);
      std::swap(swapped[offset + 1], swapped[offset + 2]);
    }
    return HashSha256(swapped, digest);
  } catch (...) {
    digest.fill(0);
    return false;
  }
}

std::string HexDigest(const std::array<std::uint8_t, 32> &digest) {
  std::ostringstream output;
  output << std::uppercase << std::hex << std::setfill('0');
  for (const auto byte : digest) {
    output << std::setw(2) << static_cast<unsigned>(byte);
  }
  return output.str();
}

std::string Utf8(const std::wstring &value) {
  if (value.empty()) {
    return {};
  }
  const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                        static_cast<int>(value.size()), nullptr,
                                        0, nullptr, nullptr);
  std::string output(static_cast<std::size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      output.data(), size, nullptr, nullptr);
  return output;
}

std::string EscapeJson(const std::string &value) {
  std::string output;
  output.reserve(value.size() + 16);
  for (const auto character : value) {
    switch (character) {
    case '\\':
      output += "\\\\";
      break;
    case '"':
      output += "\\\"";
      break;
    case '\n':
      output += "\\n";
      break;
    case '\r':
      output += "\\r";
      break;
    case '\t':
      output += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) >= 0x20U) {
        output += character;
      }
      break;
    }
  }
  return output;
}

bool WriteContainer(const std::filesystem::path &path,
                    const std::span<const std::uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return output.good();
}

bool RecordContainer(const std::filesystem::path &outputDirectory,
                     std::ofstream &manifest,
                     std::unordered_set<std::string> &extractedDigests,
                     std::uint64_t &containerCount,
                     std::uint64_t &uniqueCount,
                     const std::string &source,
                     const std::uint64_t offset,
                     const std::span<const std::uint8_t> container,
                     const ContainerInfo &info) {
  std::array<std::uint8_t, 32> digest{};
  if (!HashSha256(container, digest)) {
    std::cerr << "SHA-256 failed for candidate in " << source << "\n";
    return true;
  }

  const auto hash = HexDigest(digest);
  const auto stage = (info.flags & 1U) != 0 ? "vertex" : "pixel";
  const auto virtualBytes =
      container.first(static_cast<std::size_t>(info.virtualSize));
  const auto physicalBytes = container.subspan(
      static_cast<std::size_t>(info.virtualSize),
      static_cast<std::size_t>(info.physicalSize));
  const auto ucodeBytes = physicalBytes.subspan(
      static_cast<std::size_t>(info.shaderPhysicalOffset),
      static_cast<std::size_t>(info.shaderSize));
  std::array<std::uint8_t, 32> ucodeDigest{};
  std::array<std::uint8_t, 32> ucodeDwordSwapDigest{};
  if (!HashSha256(ucodeBytes, ucodeDigest) ||
      !HashSha256DwordSwapped(ucodeBytes, ucodeDwordSwapDigest)) {
    std::cerr << "SHA-256 failed for shader ucode in " << source << "\n";
    return true;
  }
  const bool unique = extractedDigests.insert(hash).second;
  const auto artifactName =
      std::string("xenos-") + hash + "-" + stage + ".bin";
  if (unique && !WriteContainer(outputDirectory / artifactName, container)) {
    std::cerr << "Unable to write extracted container.\n";
    return false;
  }

  ++containerCount;
  uniqueCount += unique ? 1U : 0U;
  manifest << "{\"source\":\"" << EscapeJson(source) << "\",\"offset\":"
           << offset << ",\"flags\":\"0x" << std::uppercase << std::hex
           << std::setw(8) << std::setfill('0') << info.flags << std::dec
           << "\",\"virtual_size\":" << info.virtualSize
           << ",\"physical_size\":" << info.physicalSize
           << ",\"ucode_offset\":" << info.shaderPhysicalOffset
           << ",\"ucode_size\":" << info.shaderSize
           << ",\"container_size\":" << info.totalSize
           << ",\"stage\":\"" << stage << "\",\"sha256\":\"" << hash
           << "\",\"crc32c_container\":\"" << Hex32(Crc32c(container))
           << "\",\"crc32c_virtual\":\"" << Hex32(Crc32c(virtualBytes))
           << "\",\"crc32c_physical\":\"" << Hex32(Crc32c(physicalBytes))
           << "\",\"crc32c_ucode\":\"" << Hex32(Crc32c(ucodeBytes))
           << "\",\"crc32c_ucode_dword_swap\":\""
           << Hex32(Crc32cDwordSwapped(ucodeBytes))
           << "\",\"ucode_sha256\":\"" << HexDigest(ucodeDigest)
           << "\",\"ucode_dword_swap_sha256\":\""
           << HexDigest(ucodeDwordSwapDigest)
           << "\",\"crc32c_ucode_seed_ffffffff\":\""
           << Hex32(Crc32c(ucodeBytes, 0xFFFFFFFFU))
           << "\",\"artifact\":\"" << artifactName
           << "\",\"duplicate\":" << (unique ? "false" : "true")
           << "}\n";
  return manifest.good();
}

bool IsReadableMemory(const MEMORY_BASIC_INFORMATION &memory) noexcept {
  if (memory.State != MEM_COMMIT ||
      (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
    return false;
  }
  switch (memory.Protect & 0xFFU) {
  case PAGE_READONLY:
  case PAGE_READWRITE:
  case PAGE_WRITECOPY:
  case PAGE_EXECUTE_READ:
  case PAGE_EXECUTE_READWRITE:
  case PAGE_EXECUTE_WRITECOPY:
    return true;
  default:
    return false;
  }
}

bool ReadRemoteExact(HANDLE process, const std::uintptr_t address,
                     void *const destination, const std::size_t size) noexcept {
  SIZE_T bytesRead = 0;
  return ReadProcessMemory(process, reinterpret_cast<const void *>(address),
                           destination, size, &bytesRead) != FALSE &&
         bytesRead == size;
}

bool ScanProcessRegion(
    HANDLE process, const std::uint32_t processId,
    const std::uintptr_t regionBegin, const std::uintptr_t regionEnd,
    const std::filesystem::path &outputDirectory, std::ofstream &manifest,
    std::unordered_set<std::string> &extractedDigests,
    std::unordered_set<std::uintptr_t> &visitedAddresses,
    std::uint64_t &containerCount, std::uint64_t &uniqueCount) {
  std::vector<std::uint8_t> buffer(kProcessScanChunkSize);
  const auto source = std::string("pid:") + std::to_string(processId);
  auto position = regionBegin;
  while (position < regionEnd) {
    const auto remaining = regionEnd - position;
    const auto requested = static_cast<std::size_t>((std::min)(
        static_cast<std::uintptr_t>(buffer.size()), remaining));
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(process, reinterpret_cast<const void *>(position),
                           buffer.data(), requested, &bytesRead) ||
        bytesRead < kShaderContainerSize) {
      break;
    }

    const auto *cursor = buffer.data();
    const auto *const end = buffer.data() + bytesRead;
    while (cursor + kShaderContainerSize <= end) {
      const auto *const hit = static_cast<const std::uint8_t *>(
          std::memchr(cursor, 0x10, static_cast<std::size_t>(end - cursor)));
      if (hit == nullptr || hit + kShaderContainerSize > end) {
        break;
      }
      cursor = hit + 1;
      const auto address =
          position + static_cast<std::uintptr_t>(hit - buffer.data());
      if (!visitedAddresses.insert(address).second) {
        continue;
      }

      ContainerInfo header{};
      if (!ParseContainerHeader(hit, header)) {
        continue;
      }
      std::vector<std::uint8_t> container(
          static_cast<std::size_t>(header.totalSize));
      if (!ReadRemoteExact(process, address, container.data(),
                           container.size())) {
        continue;
      }
      ContainerInfo validated{};
      if (!ValidateContainer(container.data(), container.size(), validated)) {
        continue;
      }
      if (!RecordContainer(outputDirectory, manifest, extractedDigests,
                           containerCount, uniqueCount, source, address,
                           container, validated)) {
        return false;
      }
    }

    if (bytesRead <= kShaderContainerSize) {
      break;
    }
    position += bytesRead - (kShaderContainerSize - 1U);
  }
  return true;
}

bool ScanGuestProcessMemory(
    const std::uint32_t processId,
    const std::filesystem::path &outputDirectory, std::ofstream &manifest,
    std::unordered_set<std::string> &extractedDigests,
    std::uint64_t &containerCount, std::uint64_t &uniqueCount) {
  const auto process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                                   FALSE, processId);
  if (process == nullptr) {
    std::wcerr << L"Unable to open process " << processId << L": "
               << GetLastError() << L"\n";
    return false;
  }

  std::unordered_set<std::uintptr_t> visitedAddresses;
  const auto scanEnd = kGuestMemoryBase + kGuestMemorySize;
  auto address = kGuestMemoryBase;
  bool success = true;
  while (address < scanEnd) {
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQueryEx(process, reinterpret_cast<const void *>(address),
                       &memory, sizeof(memory)) == 0) {
      break;
    }
    const auto queriedBegin =
        reinterpret_cast<std::uintptr_t>(memory.BaseAddress);
    const auto queriedEnd = queriedBegin + memory.RegionSize;
    const auto regionBegin = (std::max)(address, queriedBegin);
    const auto regionEnd = (std::min)(scanEnd, queriedEnd);
    if (regionEnd <= regionBegin) {
      break;
    }
    if (IsReadableMemory(memory) &&
        !ScanProcessRegion(process, processId, regionBegin, regionEnd,
                           outputDirectory, manifest, extractedDigests,
                           visitedAddresses, containerCount, uniqueCount)) {
      success = false;
      break;
    }
    address = regionEnd;
  }
  CloseHandle(process);
  return success;
}

std::vector<std::filesystem::path>
CollectInputFiles(const std::filesystem::path &input) {
  std::vector<std::filesystem::path> files;
  if (std::filesystem::is_regular_file(input)) {
    files.push_back(input);
    return files;
  }
  for (const auto &entry : std::filesystem::recursive_directory_iterator(
           input, std::filesystem::directory_options::skip_permission_denied)) {
    if (entry.is_regular_file()) {
      files.push_back(entry.path());
    }
  }
  return files;
}
} // namespace

int wmain(const int argc, wchar_t **const argv) {
  const bool processMode = argc >= 2 && std::wstring_view(argv[1]) == L"--pid";
  if ((!processMode && argc != 3 && argc != 4) ||
      (processMode && argc != 4 && argc != 5)) {
    std::wcerr
        << L"Usage: ac6_xenos_shader_scan <input> <output-directory> "
           L"[manifest.jsonl]\n"
        << L"       ac6_xenos_shader_scan --pid <process-id> "
           L"<output-directory> [manifest.jsonl]\n";
    return EXIT_FAILURE;
  }

  const std::filesystem::path input = processMode ? L"" : argv[1];
  const std::filesystem::path outputDirectory =
      processMode ? std::filesystem::path(argv[3])
                  : std::filesystem::path(argv[2]);
  const std::filesystem::path manifestPath =
      argc == (processMode ? 5 : 4)
          ? std::filesystem::path(argv[processMode ? 4 : 3])
          : outputDirectory / L"manifest.jsonl";
  if (!processMode && !std::filesystem::exists(input)) {
    std::wcerr << L"Input does not exist: " << input << L"\n";
    return EXIT_FAILURE;
  }
  std::filesystem::create_directories(outputDirectory);
  std::ofstream manifest(manifestPath, std::ios::binary | std::ios::trunc);
  if (!manifest) {
    std::wcerr << L"Unable to create manifest: " << manifestPath << L"\n";
    return EXIT_FAILURE;
  }

  std::unordered_set<std::string> extractedDigests;
  std::uint64_t containerCount = 0;
  std::uint64_t uniqueCount = 0;
  if (processMode) {
    wchar_t *parseEnd = nullptr;
    const auto parsedProcessId = wcstoul(argv[2], &parseEnd, 10);
    if (parseEnd == argv[2] || *parseEnd != L'\0' || parsedProcessId == 0 ||
        parsedProcessId > MAXDWORD) {
      std::wcerr << L"Invalid process id: " << argv[2] << L"\n";
      return EXIT_FAILURE;
    }
    const auto processId = static_cast<std::uint32_t>(parsedProcessId);
    if (!ScanGuestProcessMemory(processId, outputDirectory, manifest,
                                extractedDigests, containerCount,
                                uniqueCount)) {
      return EXIT_FAILURE;
    }
    std::cout << "pid=" << processId << " containers=" << containerCount
              << " unique=" << uniqueCount
              << " manifest=" << Utf8(manifestPath.wstring()) << "\n";
    return containerCount == 0 ? 2 : EXIT_SUCCESS;
  }

  const auto files = CollectInputFiles(input);
  for (const auto &path : files) {
    MappedFile mapped;
    if (!OpenMappedFile(path, mapped)) {
      std::wcerr << L"Skipping unreadable file: " << path << L"\n";
      continue;
    }

    const auto *cursor = mapped.data;
    const auto *const end = mapped.data + mapped.size;
    while (cursor + kShaderContainerSize <= end) {
      const auto *const hit = static_cast<const std::uint8_t *>(
          std::memchr(cursor, 0x10, static_cast<std::size_t>(end - cursor)));
      if (hit == nullptr || hit + kShaderContainerSize > end) {
        break;
      }
      cursor = hit + 1;
      ContainerInfo info{};
      if (!ValidateContainer(hit, static_cast<std::uint64_t>(end - hit),
                             info)) {
        continue;
      }

      const std::span<const std::uint8_t> container(
          hit, static_cast<std::size_t>(info.totalSize));
      if (!RecordContainer(
              outputDirectory, manifest, extractedDigests, containerCount,
              uniqueCount, Utf8(path.wstring()),
              static_cast<std::uint64_t>(hit - mapped.data), container,
              info)) {
        return EXIT_FAILURE;
      }
      cursor = hit + info.totalSize;
    }
  }

  std::cout << "files=" << files.size() << " containers=" << containerCount
            << " unique=" << uniqueCount << " manifest="
            << Utf8(manifestPath.wstring()) << "\n";
  return containerCount == 0 ? 2 : EXIT_SUCCESS;
}

#include "xeo3_bridge/ac6_sync_dvd_io.h"

#include <Windows.h>
#include <intrin.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
constexpr std::uint32_t kArchiveCount = 2;

bool GuestRangeIsValid(
    const std::uint32_t address,
    const std::uint64_t size) noexcept
{
    return address != 0 &&
        static_cast<std::uint64_t>(address) + size <=
            0x1'0000'0000ULL;
}

std::uint32_t ReadGuestU32(
    const std::uint8_t* const guestMemory,
    const std::uint32_t address) noexcept
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, guestMemory + address, sizeof(raw));
    return _byteswap_ulong(raw);
}

std::uint64_t ReadGuestU64(
    const std::uint8_t* const guestMemory,
    const std::uint32_t address) noexcept
{
    return
        (static_cast<std::uint64_t>(
            ReadGuestU32(guestMemory, address)) << 32) |
        ReadGuestU32(guestMemory, address + sizeof(std::uint32_t));
}

void WriteGuestU32(
    std::uint8_t* const guestMemory,
    const std::uint32_t address,
    const std::uint32_t value) noexcept
{
    const auto raw = _byteswap_ulong(value);
    std::memcpy(guestMemory + address, &raw, sizeof(raw));
}

bool FindArchiveIndex(
    const std::uint8_t* const guestMemory,
    const std::uint32_t archiveHandleTable,
    const std::uint32_t fileHandle,
    std::uint32_t& archiveIndex) noexcept
{
    if (!GuestRangeIsValid(
            archiveHandleTable,
            kArchiveCount * sizeof(std::uint32_t)))
    {
        return false;
    }

    for (std::uint32_t index = 0; index < kArchiveCount; ++index)
    {
        if (ReadGuestU32(
                guestMemory,
                archiveHandleTable +
                    index * sizeof(std::uint32_t)) == fileHandle)
        {
            archiveIndex = index;
            return true;
        }
    }
    return false;
}
}

namespace xeo3::ac6_io
{
ArchiveReadResult TryReadDataArchiveSynchronously(
    std::uint8_t* const guestMemory,
    const std::uint32_t archiveHandleTable,
    const std::uint32_t fileHandle,
    const std::uint32_t ioStatusBlock,
    const std::uint32_t buffer,
    const std::uint32_t length,
    const std::uint32_t byteOffsetPointer,
    const wchar_t* const dvdRoot) noexcept
{
    ArchiveReadResult result{};
    if (guestMemory == nullptr || dvdRoot == nullptr ||
        !GuestRangeIsValid(ioStatusBlock, 8) ||
        !GuestRangeIsValid(byteOffsetPointer, 8) ||
        !GuestRangeIsValid(buffer, length))
    {
        return result;
    }

    if (!FindArchiveIndex(
            guestMemory,
            archiveHandleTable,
            fileHandle,
            result.archiveIndex))
    {
        return result;
    }

    result.byteOffset =
        ReadGuestU64(guestMemory, byteOffsetPointer);
    result.requestedBytes = length;

    std::array<wchar_t, 1024> path{};
    const auto pathLength = std::swprintf(
        path.data(),
        path.size(),
        L"%ls\\DATA%02u.PAC",
        dvdRoot,
        result.archiveIndex);
    if (pathLength < 0 ||
        static_cast<std::size_t>(pathLength) >= path.size())
    {
        result.disposition = ArchiveReadDisposition::Failed;
        result.win32Error = ERROR_FILENAME_EXCED_RANGE;
        return result;
    }

    const auto file = CreateFileW(
        path.data(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        result.disposition = ArchiveReadDisposition::Failed;
        result.win32Error = GetLastError();
        return result;
    }

    LARGE_INTEGER size{};
    LARGE_INTEGER offset{};
    offset.QuadPart = static_cast<LONGLONG>(result.byteOffset);
    if (!GetFileSizeEx(file, &size) ||
        result.byteOffset >
            static_cast<std::uint64_t>(size.QuadPart) ||
        !SetFilePointerEx(file, offset, nullptr, FILE_BEGIN))
    {
        result.disposition = ArchiveReadDisposition::Failed;
        result.win32Error = GetLastError();
        CloseHandle(file);
        return result;
    }

    DWORD bytesRead = 0;
    auto readSucceeded = ReadFile(
        file,
        guestMemory + buffer,
        length,
        &bytesRead,
        nullptr);
    auto readError = readSucceeded ? ERROR_SUCCESS : GetLastError();

    // XeO3's guest view is backed by a mapped section.  Most reads accept the
    // view directly, but some Windows I/O paths reject that destination with
    // ERROR_NOACCESS even when every guest page is committed and writable.
    // Retry those requests through ordinary host memory, then copy the bytes
    // into the guest view with the CPU-side mapping.
    if (!readSucceeded && readError == ERROR_NOACCESS && length != 0)
    {
        std::vector<std::uint8_t> hostBuffer(length);
        LARGE_INTEGER retryOffset{};
        retryOffset.QuadPart = static_cast<LONGLONG>(result.byteOffset);
        DWORD hostBytesRead = 0;
        const auto repositioned = SetFilePointerEx(
            file,
            retryOffset,
            nullptr,
            FILE_BEGIN);
        const auto retrySucceeded = repositioned && ReadFile(
            file,
            hostBuffer.data(),
            length,
            &hostBytesRead,
            nullptr);
        const auto retryError =
            retrySucceeded ? ERROR_SUCCESS : GetLastError();
        if (retrySucceeded)
        {
            std::memcpy(
                guestMemory + buffer,
                hostBuffer.data(),
                hostBytesRead);
            bytesRead = hostBytesRead;
            readSucceeded = true;
            readError = ERROR_SUCCESS;
        }
        else
        {
            readError = repositioned ? retryError : GetLastError();
        }
    }
    CloseHandle(file);

    if (!readSucceeded)
    {
        result.disposition = ArchiveReadDisposition::Failed;
        result.win32Error = readError;
        return result;
    }

    WriteGuestU32(guestMemory, ioStatusBlock, 0);
    WriteGuestU32(
        guestMemory,
        ioStatusBlock + sizeof(std::uint32_t),
        bytesRead);
    result.disposition = ArchiveReadDisposition::Completed;
    result.completedBytes = bytesRead;
    return result;
}
}

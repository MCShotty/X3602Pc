#pragma once

#include <cstdint>

namespace xeo3::ac6_io
{
enum class ArchiveReadDisposition : std::uint32_t
{
    NotApplicable,
    Completed,
    Failed,
};

struct ArchiveReadResult
{
    ArchiveReadDisposition disposition =
        ArchiveReadDisposition::NotApplicable;
    std::uint32_t archiveIndex = 0;
    std::uint64_t byteOffset = 0;
    std::uint32_t requestedBytes = 0;
    std::uint32_t completedBytes = 0;
    std::uint32_t win32Error = 0;
};

ArchiveReadResult TryReadDataArchiveSynchronously(
    std::uint8_t* guestMemory,
    std::uint32_t archiveHandleTable,
    std::uint32_t fileHandle,
    std::uint32_t ioStatusBlock,
    std::uint32_t buffer,
    std::uint32_t length,
    std::uint32_t byteOffsetPointer,
    const wchar_t* dvdRoot) noexcept;
}

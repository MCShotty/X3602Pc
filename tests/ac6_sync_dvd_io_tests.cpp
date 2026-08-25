#include "xeo3_bridge/ac6_sync_dvd_io.h"

#include <Windows.h>
#include <intrin.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace
{
void WriteGuestU32(
    std::vector<std::uint8_t>& memory,
    const std::uint32_t address,
    const std::uint32_t value)
{
    const auto raw = _byteswap_ulong(value);
    std::memcpy(memory.data() + address, &raw, sizeof(raw));
}

std::uint32_t ReadGuestU32(
    const std::vector<std::uint8_t>& memory,
    const std::uint32_t address)
{
    std::uint32_t raw = 0;
    std::memcpy(&raw, memory.data() + address, sizeof(raw));
    return _byteswap_ulong(raw);
}

bool WriteFixture(
    const wchar_t* const path,
    const std::array<std::uint8_t, 16>& bytes)
{
    const auto file = CreateFileW(
        path,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD written = 0;
    const auto succeeded = WriteFile(
        file,
        bytes.data(),
        static_cast<DWORD>(bytes.size()),
        &written,
        nullptr);
    CloseHandle(file);
    return succeeded && written == bytes.size();
}
}

int main()
{
    std::array<wchar_t, MAX_PATH> tempRoot{};
    if (GetTempPathW(
            static_cast<DWORD>(tempRoot.size()),
            tempRoot.data()) == 0)
    {
        return 1;
    }

    std::array<wchar_t, MAX_PATH> directory{};
    if (std::swprintf(
            directory.data(),
            directory.size(),
            L"%lsxeo3-ac6-io-%lu-%llu",
            tempRoot.data(),
            GetCurrentProcessId(),
            GetTickCount64()) < 0 ||
        !CreateDirectoryW(directory.data(), nullptr))
    {
        return 2;
    }

    std::array<wchar_t, MAX_PATH> data00{};
    std::array<wchar_t, MAX_PATH> data01{};
    std::swprintf(
        data00.data(),
        data00.size(),
        L"%ls\\DATA00.PAC",
        directory.data());
    std::swprintf(
        data01.data(),
        data01.size(),
        L"%ls\\DATA01.PAC",
        directory.data());

    constexpr std::array<std::uint8_t, 16> fixture0{
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    };
    constexpr std::array<std::uint8_t, 16> fixture1{
        0xF0, 0xE1, 0xD2, 0xC3, 0xB4, 0xA5, 0x96, 0x87,
        0x78, 0x69, 0x5A, 0x4B, 0x3C, 0x2D, 0x1E, 0x0F,
    };
    if (!WriteFixture(data00.data(), fixture0) ||
        !WriteFixture(data01.data(), fixture1))
    {
        DeleteFileW(data00.data());
        DeleteFileW(data01.data());
        RemoveDirectoryW(directory.data());
        return 3;
    }

    std::vector<std::uint8_t> guestMemory(0x10000);
    constexpr std::uint32_t handleTable = 0x1000;
    constexpr std::uint32_t ioStatusBlock = 0x2000;
    constexpr std::uint32_t buffer = 0x3000;
    constexpr std::uint32_t byteOffset = 0x4000;
    constexpr std::uint32_t handle0 = 0xF80000B0;
    constexpr std::uint32_t handle1 = 0xF80000B4;
    WriteGuestU32(guestMemory, handleTable, handle0);
    WriteGuestU32(guestMemory, handleTable + 4, handle1);
    WriteGuestU32(guestMemory, byteOffset, 0);
    WriteGuestU32(guestMemory, byteOffset + 4, 5);

    const auto completed =
        xeo3::ac6_io::TryReadDataArchiveSynchronously(
            guestMemory.data(),
            handleTable,
            handle1,
            ioStatusBlock,
            buffer,
            7,
            byteOffset,
            directory.data());
    const auto matched =
        completed.disposition ==
            xeo3::ac6_io::ArchiveReadDisposition::Completed &&
        completed.archiveIndex == 1 &&
        completed.byteOffset == 5 &&
        completed.completedBytes == 7 &&
        ReadGuestU32(guestMemory, ioStatusBlock) == 0 &&
        ReadGuestU32(guestMemory, ioStatusBlock + 4) == 7 &&
        std::memcmp(
            guestMemory.data() + buffer,
            fixture1.data() + 5,
            7) == 0;

    const auto ignored =
        xeo3::ac6_io::TryReadDataArchiveSynchronously(
            guestMemory.data(),
            handleTable,
            0xF80000CC,
            ioStatusBlock,
            buffer,
            4,
            byteOffset,
            directory.data());

    DeleteFileW(data00.data());
    DeleteFileW(data01.data());
    RemoveDirectoryW(directory.data());

    if (!matched ||
        ignored.disposition !=
            xeo3::ac6_io::ArchiveReadDisposition::NotApplicable)
    {
        return 4;
    }
    return 0;
}

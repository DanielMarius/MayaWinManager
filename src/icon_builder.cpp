#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#pragma pack(push, 1)
struct IconDirectory { WORD reserved = 0, type = 1, count = 1; };
struct IconEntry { BYTE width = 0, height = 0, colors = 0, reserved = 0; WORD planes = 1, bits = 32; DWORD bytes = 0, offset = 22; };
#pragma pack(pop)

uint32_t ReadBigEndian(const BYTE *bytes)
{
    return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

bool WritePart(HANDLE file, const void *data, DWORD size)
{
    DWORD written = 0;
    return WriteFile(file, data, size, &written, nullptr) && written == size;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc != 3) return 1;
    std::ifstream input(std::filesystem::path(argv[2]), std::ios::binary);
    if (!input) return 2;
    std::vector<BYTE> png((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    constexpr BYTE signature[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    if (png.size() < 24 || png.size() > 4 * 1024 * 1024 || !std::equal(std::begin(signature), std::end(signature), png.begin()))
        return 3;
    const uint32_t width = ReadBigEndian(png.data() + 16), height = ReadBigEndian(png.data() + 20);
    if (!width || !height || width > 256 || height > 256) return 4;
    IconDirectory directory;
    IconEntry entry;
    entry.width = width == 256 ? 0 : (BYTE)width;
    entry.height = height == 256 ? 0 : (BYTE)height;
    entry.bytes = (DWORD)png.size();
    HANDLE file = CreateFileW(argv[1], GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return 5;
    const bool ok = WritePart(file, &directory, sizeof(directory)) && WritePart(file, &entry, sizeof(entry)) &&
                    WritePart(file, png.data(), (DWORD)png.size());
    CloseHandle(file);
    return ok ? 0 : 6;
}

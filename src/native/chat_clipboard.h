#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

inline HBITMAP MayaBitmapFromDib(const void *data, size_t size)
{
    if (!data || size < sizeof(BITMAPINFOHEADER)) return nullptr;
    const auto *header = static_cast<const BITMAPINFOHEADER *>(data);
    if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biSize > size || header->biWidth <= 0 ||
        header->biHeight == 0 || header->biWidth > 10000 || header->biHeight > 10000 || header->biHeight < -10000 ||
        uint64_t(header->biWidth) * uint64_t(header->biHeight < 0 ? -int64_t(header->biHeight) : header->biHeight) > 16000000 ||
        (header->biCompression != BI_RGB && header->biCompression != BI_BITFIELDS && header->biCompression != 6))
        return nullptr;
    size_t offset = header->biSize;
    if (header->biSize == sizeof(BITMAPINFOHEADER) &&
        (header->biCompression == BI_BITFIELDS || header->biCompression == 6))
        offset += (header->biCompression == 6 ? 4 : 3) * sizeof(DWORD);
    const DWORD colors = header->biClrUsed ? header->biClrUsed : header->biBitCount <= 8 ? 1u << header->biBitCount : 0;
    if (colors > 256 || offset > size || size - offset < size_t(colors) * sizeof(RGBQUAD)) return nullptr;
    offset += size_t(colors) * sizeof(RGBQUAD);
    if (offset >= size) return nullptr;
    HDC dc = GetDC(nullptr);
    if (!dc) return nullptr;
    HBITMAP bitmap = CreateDIBitmap(dc, header, CBM_INIT, static_cast<const BYTE *>(data) + offset,
                                    reinterpret_cast<const BITMAPINFO *>(data), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    return bitmap;
}

inline HBITMAP MayaClipboardBitmap(HWND owner)
{
    if (!OpenClipboard(owner)) return nullptr;
    HBITMAP bitmap = nullptr;
    for (UINT format : {CF_DIBV5, CF_DIB})
    {
        if (!IsClipboardFormatAvailable(format)) continue;
        HGLOBAL dib = static_cast<HGLOBAL>(GetClipboardData(format));
        if (!dib) continue;
        const auto size = GlobalSize(dib);
        const void *data = GlobalLock(dib);
        if (data) bitmap = MayaBitmapFromDib(data, size);
        if (data) GlobalUnlock(dib);
        if (bitmap) break;
    }
    if (!bitmap && IsClipboardFormatAvailable(CF_BITMAP))
    {
        auto source = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        if (source) bitmap = static_cast<HBITMAP>(CopyImage(source, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    }
    CloseClipboard();
    return bitmap;
}

inline bool MayaSaveClipboardPng(HBITMAP bitmap, const std::filesystem::path &path)
{
    if (!bitmap) return false;
    Gdiplus::Bitmap image(bitmap, nullptr);
    if (image.GetLastStatus() != Gdiplus::Ok) return false;
    UINT count = 0, bytes = 0;
    if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok || !bytes) return false;
    std::vector<BYTE> storage(bytes);
    auto *encoders = reinterpret_cast<Gdiplus::ImageCodecInfo *>(storage.data());
    if (Gdiplus::GetImageEncoders(count, bytes, encoders) != Gdiplus::Ok) return false;
    const auto png = std::find_if(encoders, encoders + count, [](const auto &e) {
        return e.MimeType && wcscmp(e.MimeType, L"image/png") == 0;
    });
    if (png == encoders + count) return false;
    IStream *stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK) return false;
    bool saved = false;
    if (image.Save(stream, &png->Clsid, nullptr) == Gdiplus::Ok)
    {
        STATSTG stat{};
        LARGE_INTEGER begin{};
        if (stream->Stat(&stat, STATFLAG_NONAME) == S_OK && stat.cbSize.QuadPart <= 20000000ULL &&
            stream->Seek(begin, STREAM_SEEK_SET, nullptr) == S_OK)
        {
            std::ofstream file(path, std::ios::binary | std::ios::out | std::ios::trunc);
            std::vector<char> buffer(65536);
            saved = (bool)file;
            while (saved)
            {
                ULONG read = 0;
                const HRESULT result = stream->Read(buffer.data(), (ULONG)buffer.size(), &read);
                if (read) file.write(buffer.data(), read);
                if (!file) saved = false;
                if (result == S_FALSE || read == 0) break;
                if (FAILED(result)) saved = false;
            }
        }
    }
    stream->Release();
    return saved;
}

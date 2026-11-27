#include "chat_clipboard.h"
#include "image_runtime.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <objbase.h>

int wmain(int argc, wchar_t **argv)
{
    if (argc != 2)
    {
        std::wcerr << L"Pass a new PNG output path.\n";
        return 2;
    }
    if (GetModuleHandleW(L"gdiplus.dll"))
    {
        std::wcerr << L"GDI+ loaded before the image path was used.\n";
        return 1;
    }
    const HRESULT ole = OleInitialize(nullptr);
    if (FAILED(ole))
    {
        std::wcerr << L"OleInitialize failed.\n";
        return 1;
    }

    ULONG_PTR token = 0;
    HBITMAP dib = nullptr;
    int result = 1;
    do
    {
        if (!MayaEnsureGdiplus(token) || !token || !GetModuleHandleW(L"gdiplus.dll"))
        {
            std::wcerr << L"Lazy GDI+ startup failed.\n";
            break;
        }
        const ULONG_PTR firstToken = token;
        if (!MayaEnsureGdiplus(token) || token != firstToken)
        {
            std::wcerr << L"GDI+ startup was not idempotent.\n";
            break;
        }

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = 4;
        info.bmiHeader.biHeight = -4;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void *pixels = nullptr;
        dib = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!dib || !pixels)
        {
            std::wcerr << L"CreateDIBSection failed.\n";
            break;
        }
        auto *pixel = static_cast<uint32_t *>(pixels);
        for (uint32_t i = 0; i < 16; ++i)
            pixel[i] = 0xff000000u | ((i * 47u & 255u) << 16) | ((i * 83u & 255u) << 8) | (i * 29u & 255u);

        const std::filesystem::path output(argv[1]);
        std::error_code ec;
        if (std::filesystem::exists(output, ec) || ec || !MayaSaveClipboardPng(dib, output))
        {
            std::wcerr << L"Clipboard bitmap to PNG encoding failed or output already exists.\n";
            break;
        }
        DeleteObject(dib);
        dib = nullptr;

        std::ifstream file(output, std::ios::binary);
        unsigned char signature[8]{};
        static constexpr unsigned char pngSignature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
        if (!file.read(reinterpret_cast<char *>(signature), sizeof(signature)) ||
            std::memcmp(signature, pngSignature, sizeof(signature)) != 0)
        {
            std::wcerr << L"Output does not have a PNG signature.\n";
            break;
        }
        file.close();

        Gdiplus::Bitmap decoded(output.c_str(), FALSE);
        if (decoded.GetLastStatus() != Gdiplus::Ok || decoded.GetWidth() != 4 || decoded.GetHeight() != 4)
        {
            std::wcerr << L"PNG decode or dimensions check failed.\n";
            break;
        }
        std::unique_ptr<Gdiplus::Image> thumbnail(decoded.GetThumbnailImage(2, 2, nullptr, nullptr));
        if (!thumbnail || thumbnail->GetLastStatus() != Gdiplus::Ok)
        {
            std::wcerr << L"Thumbnail creation failed.\n";
            break;
        }
        Gdiplus::Bitmap rendered(8, 8, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&rendered);
        if (rendered.GetLastStatus() != Gdiplus::Ok || graphics.GetLastStatus() != Gdiplus::Ok ||
            graphics.DrawImage(thumbnail.get(), 0, 0, 8, 8) != Gdiplus::Ok)
        {
            std::wcerr << L"Offscreen image rendering failed.\n";
            break;
        }
        result = 0;
        std::wcout << L"Lazy GDI+, clipboard PNG encode, image decode, thumbnail and draw passed: " << output << L'\n';
    } while (false);

    if (dib) DeleteObject(dib);
    if (token) Gdiplus::GdiplusShutdown(token);
    OleUninitialize();
    return result;
}

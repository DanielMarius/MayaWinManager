#pragma once
#include <windows.h>
#include <gdiplus.h>

inline bool MayaEnsureGdiplus(ULONG_PTR &token) noexcept
{
    if (token) return true;
    Gdiplus::GdiplusStartupInput input;
    return Gdiplus::GdiplusStartup(&token, &input, nullptr) == Gdiplus::Ok;
}

#pragma once

#include <windows.h>

inline int NativeComboLayoutHeight(HWND, int closedHeight) noexcept { return closedHeight; }

inline BOOL CALLBACK NativeFindOpenComboBox(HWND child, LPARAM state)
{
    wchar_t className[32]{};
    if (GetClassNameW(child, className, 32) && lstrcmpiW(className, L"ComboBox") == 0 &&
        SendMessageW(child, CB_GETDROPPEDSTATE, 0, 0))
    {
        *reinterpret_cast<bool *>(state) = true;
        return FALSE;
    }
    return TRUE;
}

inline bool NativeComboPopupOpen(HWND parent)
{
    bool open = false;
    if (parent) EnumChildWindows(parent, NativeFindOpenComboBox, (LPARAM)&open);
    return open;
}

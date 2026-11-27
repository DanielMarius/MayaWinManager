#pragma once

#include <windows.h>

// The app is an overlapped window, so walk its child Z-order instead of dialog-only APIs.
inline bool MayaIsTabStop(HWND parent, HWND control) noexcept
{
    return control && GetParent(control) == parent &&
           (GetWindowLongPtrW(control, GWL_STYLE) & WS_TABSTOP) &&
           IsWindowVisible(control) && IsWindowEnabled(control);
}

inline HWND MayaNextTabStop(HWND parent, HWND focused, BOOL backwards) noexcept
{
    HWND top = GetWindow(parent, GW_CHILD);
    if (!top) return nullptr;
    HWND bottom = top;
    while (HWND next = GetWindow(bottom, GW_HWNDNEXT)) bottom = next;

    const auto step = [backwards](HWND control) noexcept {
        return GetWindow(control, backwards ? GW_HWNDPREV : GW_HWNDNEXT);
    };
    const bool focusedCanTab = MayaIsTabStop(parent, focused);
    HWND candidate = focusedCanTab ? step(focused) : (backwards ? bottom : top);
    for (int pass = 0; pass < 2; ++pass)
    {
        for (HWND control = candidate; control; control = step(control))
            if (control != focused && MayaIsTabStop(parent, control)) return control;
        candidate = backwards ? bottom : top;
    }
    return focusedCanTab ? focused : nullptr;
}

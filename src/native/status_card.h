#pragma once

constexpr int NativeStatusCardId = 274;
constexpr int NativeChatOptionsId = 278;
constexpr int NativeSavingsSidebarId = 287;

inline bool NativeStatusCardClicked(int controlId, int notification) noexcept
{
    return controlId == NativeStatusCardId && notification == 0;
}

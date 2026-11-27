#pragma once

struct NativeSupportLinkPlacement
{
    bool about = false, update = false;
};

inline NativeSupportLinkPlacement NativeSupportLinkPlacementFor(bool, bool) noexcept
{
    return {true, false};
}

constexpr wchar_t NativeSupportMessage[] =
    L"I build Maya because I believe everyone should have access to great local AI. If you enjoy what I'm building, "
    L"consider supporting me. Every little bit helps keep the project alive and growing. ♥";
inline constexpr wchar_t NativeSupportUrl[] = L"https://github.com/sponsors/mw00";

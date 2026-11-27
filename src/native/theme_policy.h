#pragma once

#include <cstdint>

constexpr bool NativeSystemThemeChanged(bool previousLight, bool currentLight) noexcept
{
    return previousLight != currentLight;
}

constexpr bool NativeTitlebarUsesDarkMode(bool light) noexcept
{
    return !light;
}

struct NativeThemeColors
{
    std::uint32_t background, sidebar, card, fill, surface3, line, text, softText, mutedText, open, warningText,
        accent, accentText, info, infoText, ok, okText, danger, dangerText;
};

// GDI brushes are opaque; these values preblend CSS --st-open over the sidebar color.
constexpr NativeThemeColors NativeThemeColorsFor(bool light, bool violet) noexcept
{
    if (light && violet)
        return {0xf4f4fa, 0xeeeef8, 0xffffff, 0xf0f0f8, 0xe8e8f4, 0xe0e1ef, 0x14152b, 0x33365a, 0x62668a,
                0xe1e1ec, 0x92400e, 0x6d5cf6, 0x5143d9, 0x0284c7, 0x0369a1, 0x10b981, 0x047857, 0xe11d48,
                0xbe123c};
    if (light)
        return {0xffffff, 0xf7f7f9, 0xffffff, 0xf2f2f5, 0xe8e8ed, 0xe6e6eb, 0x18181d, 0x3a3a44, 0x676773,
                0xeaeaec, 0x92400e, 0x6d5cf6, 0x5143d9, 0x0284c7, 0x0369a1, 0x10b981, 0x047857, 0xe11d48,
                0xbe123c};
    if (violet)
        return {0x090a14, 0x0c0d20, 0x11132a, 0x171a34, 0x1e2240, 0x252a4a, 0xeceefe, 0xc3c7e6, 0x8a8fb4,
                0x1f2032, 0xfcd34d, 0x8b7cff, 0xb4aaff, 0x38bdf8, 0x7dd3fc, 0x34d399, 0x6ee7b7, 0xfb7185,
                0xfda4af};
    return {0x19191d, 0x111114, 0x222227, 0x2a2a30, 0x34343b, 0x2e2e35, 0xececf1, 0xc9c9d3, 0x9d9daa,
            0x242427, 0xfcd34d, 0x8b7cff, 0xb4aaff, 0x38bdf8, 0x7dd3fc, 0x34d399, 0x6ee7b7, 0xfb7185,
            0xfda4af};
}

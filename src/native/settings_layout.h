#pragma once

struct NativeSettingsTopRows
{
    int headingY, headingHeight;
    int themeLabelY, themeBoxY;
    int schemeLabelY, schemeBoxY;
    int contextY;
};

inline NativeSettingsTopRows NativeSettingsTopRowsAt(int bodyY) noexcept
{
    return {bodyY, 22, bodyY + 32, bodyY + 28, bodyY + 76, bodyY + 72, bodyY + 110};
}

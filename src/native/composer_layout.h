#pragma once
#include <algorithm>

struct NativeChatColumnLayout
{
    int left = 0;
    int width = 0;
};

inline NativeChatColumnLayout NativeChatColumnFor(int bodyLeft, int bodyWidth, int reservedLeft,
                                                    int maxWidth) noexcept
{
    const int totalWidth = std::max(0, bodyWidth);
    const int leftInset = std::min(totalWidth, std::max(0, reservedLeft));
    const int availableWidth = totalWidth - leftInset;
    const int width = std::min(availableWidth, std::max(0, maxWidth));
    return {bodyLeft + leftInset + (availableWidth - width) / 2, width};
}

inline int NativeComposerHeight(int lines, bool multiline, bool hasAttachments, int lineHeight, int availableHeight) noexcept
{
    const bool expanded = multiline || hasAttachments || lines > 1;
    const int limit = std::max(42, availableHeight * 2 / 5);
    const int wanted = expanded ? std::max(82, std::max(1, lines) * std::max(16, lineHeight) + 20) : 42;
    return std::min(wanted, limit);
}

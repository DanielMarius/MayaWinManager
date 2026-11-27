#pragma once
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

inline bool ContextMeterNeedsRefresh(bool modelChanged, uint64_t previousMax, uint64_t currentMax,
                                     bool serverRecovered = false) noexcept
{
    return serverRecovered || modelChanged || previousMax != currentMax;
}

constexpr bool NativeContextMeterVisible(bool chatPage, bool hasMessages, bool hasMaximum, bool hasCount) noexcept
{
    return chatPage && hasMessages && hasMaximum && hasCount;
}

inline bool NativeContextMeterCountMatches(const std::wstring &currentChat, const std::wstring &countedChat) noexcept
{
    return !currentChat.empty() && currentChat == countedChat;
}

inline std::string FormatContextMeter(uint64_t tokens, uint64_t maximum, bool approximate)
{
    auto grouped = [](uint64_t value) {
        auto text = std::to_string(value);
        for (int at = (int)text.size() - 3; at > 0; at -= 3)
            text.insert((size_t)at, ",");
        return text;
    };
    std::ostringstream out;
    if (approximate)
        out << "~";
    out << "Context: " << grouped(tokens);
    if (maximum)
    {
        const double percentage = std::min(100.0, 100.0 * (double)tokens / (double)maximum);
        out << " / " << grouped(maximum) << " (" << std::fixed << std::setprecision(1)
            << percentage << "%)";
    }
    return out.str();
}

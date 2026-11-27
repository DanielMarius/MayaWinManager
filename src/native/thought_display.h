#pragma once
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <utility>

inline bool NativeThoughtIsStreaming(bool streaming, bool currentChat, std::size_t messageIndex,
                                     std::size_t messageCount, bool answerStarted) noexcept
{
    return streaming && currentChat && messageIndex == messageCount && !answerStarted;
}

inline bool NativeThoughtRefreshOnAnswerStart(bool firstText, bool hasReasoning) noexcept
{
    return firstText && hasReasoning;
}

inline std::wstring ThoughtSummaryLabel(double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0) return L"Thoughts";
    std::wostringstream text;
    text << L"Thought for " << std::fixed << std::setprecision(1) << seconds << L" s";
    return text.str();
}

inline bool ThoughtIsExpanded(const std::set<std::string> &expanded, const std::set<std::string> &collapsed,
                              const std::string &key, bool autoExpand)
{
    if (expanded.count(key)) return true;
    if (collapsed.count(key)) return false;
    return autoExpand;
}

inline bool ToggleThoughtExpansion(std::set<std::string> &expanded, std::set<std::string> &collapsed,
                                   const std::string &key, bool wasExpanded)
{
    expanded.erase(key);
    collapsed.erase(key);
    if (wasExpanded)
    {
        collapsed.insert(key);
        return false;
    }
    return expanded.insert(key).second;
}

inline void ShiftThoughtRangeAfterInsert(long &start, long &end, long at, long count)
{
    if (start >= at) start += count;
    if (end >= at) end += count;
}

inline bool ThoughtLinkHit(long linkMin, long linkMax, long clickMin, long clickMax)
{
    if (clickMin > clickMax) std::swap(clickMin, clickMax);
    return clickMin == clickMax ? clickMin >= linkMin && clickMin < linkMax
                                : clickMin < linkMax && clickMax > linkMin;
}

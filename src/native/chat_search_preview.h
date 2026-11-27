#pragma once
#include "chat_metadata.h"
#include <algorithm>
#include <climits>
#include <string>
#include <windows.h>

constexpr size_t NativeChatSearchResultLimit = 50;

enum class NativeChatSearchEnterAction { NONE, OPEN_SELECTED };

template <typename IsSelectable>
inline int NativeChatSearchMoveSelection(int current, int count, bool down, IsSelectable isSelectable)
{
    if (count <= 0) return -1;
    if (current < 0 || current >= count)
    {
        for (int i = 0; i < count; ++i)
            if (isSelectable(i)) return i;
        return -1;
    }
    const int step = down ? 1 : -1;
    for (int i = current + step; i >= 0 && i < count; i += step)
        if (isSelectable(i)) return i;
    return isSelectable(current) ? current : -1;
}

inline NativeChatSearchEnterAction NativeChatSearchEnter(bool hasSelectedResult)
{
    return hasSelectedResult ? NativeChatSearchEnterAction::OPEN_SELECTED : NativeChatSearchEnterAction::NONE;
}

inline std::vector<int> NativeChatSearchLimitedOrder(const std::vector<int> &order,
                                                     const std::vector<bool> &matches)
{
    std::vector<int> out;
    out.reserve(std::min(order.size(), NativeChatSearchResultLimit));
    for (int index : order)
        if (index >= 0 && (size_t)index < matches.size() && matches[index])
        {
            out.push_back(index);
            if (out.size() == NativeChatSearchResultLimit) break;
        }
    return out;
}

inline std::wstring NativeChatSearchQuery(const std::wstring &query)
{
    return TrimAndLimitName(query, query.size());
}

inline bool NativeChatSearchClearOnSelect(const std::wstring &query)
{
    return !NativeChatSearchQuery(query).empty();
}

inline std::wstring NativeChatSearchText(const std::wstring &text)
{
    std::wstring out;
    bool space = false;
    for (wchar_t c : text)
    {
        if (ChatTitleWhitespace(c))
        {
            space = !out.empty();
            continue;
        }
        if (space) out += L' ';
        out += c;
        space = false;
    }
    return out;
}

inline size_t NativeChatSearchFind(const std::wstring &text, const std::wstring &query)
{
    if (query.empty()) return 0;
    if (text.size() > INT_MAX || query.size() > INT_MAX) return std::wstring::npos;
    const int found = FindStringOrdinal(FIND_FROMSTART, text.c_str(), (int)text.size(), query.c_str(),
                                        (int)query.size(), TRUE);
    return found < 0 ? std::wstring::npos : (size_t)found;
}

inline std::wstring NativeChatSearchPreview(const std::wstring &text, const std::wstring &query,
                                            const std::wstring &title)
{
    const auto normalized = NativeChatSearchText(text);
    if (normalized == title) return {};
    const size_t match = NativeChatSearchFind(normalized, query);
    if (match == std::wstring::npos) return {};
    const size_t begin = match > 30 ? match - 30 : 0;
    const size_t end = std::min(normalized.size(), match + 90);
    return (match > 30 ? L"…" : L"") + normalized.substr(begin, end - begin);
}

inline std::wstring NativeChatAssistantPreview(const std::wstring &text)
{
    std::wstring clean;
    bool space = false;
    for (wchar_t c : text)
    {
        if (c == L'#' || c == L'*' || c == L'`' || c == L'>' || c == L'|' || c == L'_' || c == L'-')
        {
            space = !clean.empty();
            continue;
        }
        if (ChatTitleWhitespace(c))
        {
            space = !clean.empty();
            continue;
        }
        if (space) clean += L' ';
        clean += c;
        space = false;
    }
    if (clean.size() > 90) clean.resize(90);
    return clean;
}

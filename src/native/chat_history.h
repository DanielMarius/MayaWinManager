#pragma once
#include "http.h"
#include <algorithm>
#include <cstdint>
#include <vector>

inline bool NativeCanReuseBlankChat(bool hasCurrentChat, bool isEmpty, bool instructionsMatchDefault) noexcept
{
    return hasCurrentChat && isEmpty && instructionsMatchDefault;
}

inline bool NativePendingChatResumeEligible(bool isCurrent, bool hasPending, bool busy) noexcept
{
    return isCurrent && hasPending && !busy;
}

inline std::vector<int> NativeChatRecentOrder(const std::vector<int64_t> &updated)
{
    std::vector<int> order(updated.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = (int)i;
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return updated[a] > updated[b]; });
    return order;
}

inline bool ApplyChatHistoryRecord(std::vector<Json> &history, const Json &record)
{
    if (!record.is_object())
        return false;
    const auto event = record.value("event", std::string());
    if (event.empty())
    {
        history.push_back(record);
        return true;
    }
    if (event == "truncate" || event == "replace_from")
    {
        if (!record.contains("index") || !record["index"].is_number_integer())
            return false;
        uint64_t index = 0;
        if (record["index"].is_number_unsigned())
            index = record["index"].get<uint64_t>();
        else
        {
            const auto signedIndex = record["index"].get<int64_t>();
            if (signedIndex < 0)
                return false;
            index = (uint64_t)signedIndex;
        }
        if (index > history.size())
            return false;
        if (event == "replace_from" && (!record.contains("message") || !record["message"].is_object()))
            return false;
        history.resize((size_t)index);
        if (event == "replace_from")
            history.push_back(record["message"]);
        return true;
    }
    return false;
}

inline Json TruncateChatHistory(size_t count)
{
    return {{"event", "truncate"}, {"index", count}};
}

inline Json ReplaceChatHistoryFrom(size_t index, const Json &message)
{
    return {{"event", "replace_from"}, {"index", index}, {"message", message}};
}

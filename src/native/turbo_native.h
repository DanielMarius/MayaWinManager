#pragma once
#include "http.h"
#include <string>
#include <stdexcept>
#include <vector>

inline bool NativeTurboInfoEligible(bool hasResult, bool inFlight) noexcept
{
    return !hasResult && !inFlight;
}

inline Json NativeTurboEnter(const Json &saved, const std::string &family, const std::string &chatThinking,
                             const std::vector<std::string> &levels)
{
    if (levels.empty()) throw std::runtime_error("Maya did not report a thinking level for this model.");
    const Json stored = saved.is_object() && saved.contains(family) ? saved[family] : Json(nullptr);
    Json snapshot{{"family", family}, {"stored", stored}, {"chat_thinking", chatThinking}, {"fast", levels.front()}};
    Json next = saved.is_object() ? saved : Json::object();
    if (!next.contains(family) || !next[family].is_object()) next[family] = Json::object();
    next[family]["temperature"] = 0;
    next[family]["reasoning_effort"] = levels.front();
    return {{"snapshot", snapshot}, {"sampling", next}};
}

inline Json NativeTurboLeave(const Json &saved, const Json &snapshot, const std::string &chatThinking,
                             std::string &restoredChatThinking)
{
    restoredChatThinking = chatThinking;
    if (!snapshot.is_object() || !snapshot.contains("family") || !snapshot["family"].is_string()) return saved;
    const auto family = snapshot["family"].get<std::string>();
    Json next = saved.is_object() ? saved : Json::object();
    if (snapshot.contains("stored") && !snapshot["stored"].is_null()) next[family] = snapshot["stored"];
    else next.erase(family);
    if (snapshot.contains("chat_thinking") && snapshot["chat_thinking"].is_string() &&
        snapshot.contains("fast") && snapshot["fast"].is_string() && chatThinking == snapshot["fast"].get<std::string>())
        restoredChatThinking = snapshot["chat_thinking"].get<std::string>();
    return next;
}

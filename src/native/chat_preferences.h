#pragma once
#include "http.h"
#include <vector>

inline int ChatThemeComboIndex(const std::string &theme)
{
    return theme == "system" ? 0 : theme == "light" ? 2 : 1;
}

inline int ChatSchemeComboIndex(const std::string &scheme)
{
    return scheme == "violet" ? 1 : 0;
}

inline std::string ChatSavedScheme(const Json &prefs)
{
    if (!prefs.is_object() || !prefs.contains("scheme") || !prefs["scheme"].is_string()) return "violet";
    return prefs["scheme"] == "violet" ? "violet" : "default";
}

inline int ChatProfileComboIndex(const std::wstring &profile)
{
    return profile == L"maya-ds4.json" ? 0 : 1;
}

inline std::wstring ChatSavedProfile(const Json &prefs)
{
    return prefs.is_object() && prefs.contains("profile") && prefs["profile"].is_string() &&
                   prefs["profile"].get<std::string>() == "maya-ds4.json"
               ? L"maya-ds4.json" : L"maya-ds4-abliterated.json";
}

inline Json ChatPreferences(const std::string &system, bool showThinking = true, const Json &sampling = Json::object(),
                            const std::string &theme = {}, const std::string &scheme = {},
                            const Json &turboSnapshot = nullptr, bool mcp = true,
                            const std::string &gpuOpen = {}, bool sidebarOpen = true,
                            const std::wstring &currentChat = {}, const std::string &modelId = {},
                            const std::string &profile = {})
{
    Json prefs{{"system", system}, {"show_thinking", showThinking}};
    if (sampling.is_object() && !sampling.empty()) prefs["sampling"] = sampling;
    if (!theme.empty()) prefs["theme"] = theme;
    if (!scheme.empty()) prefs["scheme"] = scheme;
    if (!turboSnapshot.is_null()) prefs["turbo_snapshot"] = turboSnapshot;
    if (!mcp) prefs["mcp"] = false;
    if (!gpuOpen.empty()) prefs["gpu_open"] = gpuOpen;
    if (!sidebarOpen) prefs["side_open"] = false;
    if (!currentChat.empty()) prefs["current_chat"] = Utf8(currentChat);
    if (!modelId.empty()) prefs["model_id"] = modelId;
    if (!profile.empty()) prefs["profile"] = profile;
    return prefs;
}

inline std::string ChatSavedModelId(const Json &prefs)
{
    return prefs.is_object() && prefs.contains("model_id") && prefs["model_id"].is_string()
               ? prefs["model_id"].get<std::string>() : std::string();
}

inline std::string ChatModelIdentity(const Json &health)
{
    if (!health.is_object() || !health.contains("model") || !health["model"].is_string()) return {};
    const auto model = health["model"].get<std::string>();
    if (model.empty()) return {};
    const auto label = health.contains("model_label") && health["model_label"].is_string()
                           ? health["model_label"].get<std::string>() : std::string();
    return model + "|" + label;
}

inline bool ChatModelChanged(const std::string &saved, const std::string &current)
{
    return !saved.empty() && !current.empty() && saved != current;
}
inline bool ChatModelListNeedsRefreshAfterStatus(bool modelChanged, bool managerSwitchWatching) noexcept
{
    return modelChanged && !managerSwitchWatching;
}

inline Json ResetChatSamplingForModelChange(Json sampling)
{
    if (!sampling.is_object()) return Json::object();
    for (auto it = sampling.begin(); it != sampling.end();)
    {
        if (it.value().is_object())
        {
            for (const auto *key : {"reasoning_effort", "temperature", "top_p", "top_k", "min_p",
                                    "repetition_penalty", "frequency_penalty", "presence_penalty"})
                it.value().erase(key);
            if (it.value().empty()) { it = sampling.erase(it); continue; }
        }
        ++it;
    }
    return sampling;
}

inline std::wstring ChatCurrentChatId(const Json &prefs)
{
    return prefs.is_object() && prefs.contains("current_chat") && prefs["current_chat"].is_string()
               ? Wide(prefs["current_chat"].get<std::string>())
               : std::wstring();
}

inline int ChatPreferredIndex(const std::vector<std::wstring> &ids, const std::wstring &preferred)
{
    if (!preferred.empty())
        for (size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == preferred) return static_cast<int>(i);
    return ids.empty() ? -1 : 0;
}

inline bool ChatMcpEnabled(const Json &prefs)
{
    return !prefs.is_object() || !prefs.contains("mcp") || !prefs["mcp"].is_boolean() || prefs["mcp"].get<bool>();
}

inline std::string ChatMonitorGpuOpenKey(const Json &prefs)
{
    return prefs.is_object() && prefs.contains("gpu_open") && prefs["gpu_open"].is_string()
               ? prefs["gpu_open"].get<std::string>()
               : std::string();
}

inline bool ChatSidebarOpen(const Json &prefs)
{
    return !prefs.is_object() || !prefs.contains("side_open") || !prefs["side_open"].is_boolean() ||
           prefs["side_open"].get<bool>();
}

inline bool ChatShowThinking(const Json &prefs)
{
    return !prefs.is_object() || !prefs.contains("show_thinking") || !prefs["show_thinking"].is_boolean()
               ? true
               : prefs["show_thinking"].get<bool>();
}

inline std::string ChatDefaultInstructions(const Json &prefs)
{
    return prefs.is_object() && prefs.contains("system") && prefs["system"].is_string()
               ? prefs["system"].get<std::string>()
               : std::string();
}

constexpr bool NativeInstructionAutosaveNeeded(bool fieldFocused, bool changed) noexcept
{
    return fieldFocused && changed;
}

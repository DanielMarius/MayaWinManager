#pragma once
#include "http.h"
#include <algorithm>
#include <cwctype>
#include <string>
#include <vector>

inline bool ChatTitleWhitespace(wchar_t value)
{
    return std::iswspace(value) || value == 0x00A0 || value == 0x1680 || (value >= 0x2000 && value <= 0x200A) ||
           value == 0x2028 || value == 0x2029 || value == 0x202F || value == 0x205F || value == 0x3000 ||
           value == 0xFEFF;
}

inline bool ChatInputTrimWhitespace(wchar_t value) noexcept
{
    return (value >= 0x0009 && value <= 0x000D) || value == 0x0020 || value == 0x00A0 || value == 0x1680 ||
           (value >= 0x2000 && value <= 0x200A) || value == 0x2028 || value == 0x2029 || value == 0x202F ||
           value == 0x205F || value == 0x3000 || value == 0xFEFF;
}

inline std::wstring TrimChatInput(const std::wstring &value)
{
    size_t first = 0, last = value.size();
    while (first < last && ChatInputTrimWhitespace(value[first])) ++first;
    while (last > first && ChatInputTrimWhitespace(value[last - 1])) --last;
    return value.substr(first, last - first);
}

inline std::wstring TrimAndLimitName(const std::wstring &value, size_t limit)
{
    size_t first = 0, last = value.size();
    while (first < last && ChatTitleWhitespace(value[first])) ++first;
    while (last > first && ChatTitleWhitespace(value[last - 1])) --last;
    return value.substr(first, std::min(limit, last - first));
}

inline std::wstring ChatTitleFrom(const std::wstring &text, const std::vector<std::wstring> &files)
{
    std::wstring source = text;
    if (source.empty())
        for (size_t i = 0; i < files.size(); ++i)
            source += (i ? L", " : L"") + files[i];
    std::wstring out;
    bool space = false;
    for (wchar_t c : source)
    {
        if (ChatTitleWhitespace(c))
        {
            space = !out.empty();
            continue;
        }
        if (space) out += L' ';
        out += c;
        space = false;
        if (out.size() == 60) break;
    }
    return out.empty() ? L"New chat" : out;
}

inline bool NativeChatTitleIsAuto(const std::wstring &title, bool manuallyNamed,
                                  const std::wstring &firstUserText)
{
    if (manuallyNamed) return false;
    return title.empty() || title == L"New chat" ||
           (!firstUserText.empty() && title == firstUserText.substr(0, 80));
}

struct ChatMetadata
{
    std::string title, system, folder, thinking = "high", thinkingFor;
    bool pinned = false;
    std::string pendingJob;
    bool titleManual = false;
    bool hasAblate = false, ablate = false;
    std::string ablateFor;
    bool thinkingPicked = false;
};

inline ChatMetadata ParseChatMetadata(const Json &value)
{
    ChatMetadata out;
    if (!value.is_object())
        return out;
    out.title = value.value("title", std::string());
    out.system = value.value("system", std::string());
    out.folder = value.value("folder", std::string());
    out.thinking = value.value("thinking", std::string("high"));
    out.thinkingFor = value.value("thinkingFor", std::string());
    out.pinned = value.value("pinned", false);
    out.pendingJob = value.value("pendingJob", std::string());
    out.titleManual = value.contains("titleManual") && value["titleManual"].is_boolean() && value["titleManual"].get<bool>();
    out.hasAblate = value.contains("ablate") && value["ablate"].is_boolean();
    if (out.hasAblate) out.ablate = value["ablate"].get<bool>();
    out.ablateFor = value.value("ablateFor", std::string());
    out.thinkingPicked = value.contains("thinkingPicked") && value["thinkingPicked"].is_boolean() &&
                         value["thinkingPicked"].get<bool>();
    return out;
}

inline Json SerializeChatMetadata(const ChatMetadata &value)
{
    Json out{{"title", value.title},
             {"system", value.system},
             {"folder", value.folder},
             {"thinking", value.thinking},
             {"thinkingFor", value.thinkingFor},
             {"pinned", value.pinned},
             {"pendingJob", value.pendingJob},
             {"titleManual", value.titleManual}};
    if (value.hasAblate)
    {
        out["ablate"] = value.ablate;
        out["ablateFor"] = value.ablateFor;
    }
    if (value.thinkingPicked) out["thinkingPicked"] = true;
    return out;
}

inline bool NativeChatAblationAvailable(const Json &health)
{
    if (!health.is_object() || !health.contains("ablate") || !health["ablate"].is_object()) return false;
    const auto &ablate = health["ablate"];
    return ablate.contains("available") && ablate["available"].is_boolean() && ablate["available"].get<bool>();
}

inline Json NativeChatAblationRequestValue(const Json &health, const std::string &activeModel,
                                           bool hasChoice, bool choice, const std::string &choiceModel)
{
    if (!NativeChatAblationAvailable(health)) return nullptr;
    if (hasChoice && !activeModel.empty() && activeModel == choiceModel) return choice;
    const auto &ablate = health["ablate"];
    return ablate.contains("default") && ablate["default"].is_boolean() ? ablate["default"] : Json(false);
}

inline void AddNativeChatAblation(Json &request, const Json &health, const std::string &activeModel,
                                  bool hasChoice, bool choice, const std::string &choiceModel)
{
    const auto value = NativeChatAblationRequestValue(health, activeModel, hasChoice, choice, choiceModel);
    if (value.is_boolean()) request["ablate"] = value;
}

inline std::string ThinkingForModel(const ChatMetadata &chat, const std::string &activeModel,
                                    const std::string &fallback = "high")
{
    if (!chat.thinkingFor.empty() && !activeModel.empty() && chat.thinkingFor != activeModel)
        return fallback;
    return chat.thinking.empty() ? fallback : chat.thinking;
}

struct FolderMetadata
{
    std::string id, name;
    bool collapsed = false;
};

inline std::vector<FolderMetadata> ParseFolders(const Json &data)
{
    std::vector<FolderMetadata> out;
    if (!data.is_array())
        return out;
    for (const auto &item : data)
        if (item.is_object() && item.contains("id") && item["id"].is_string() && item.contains("name") &&
            item["name"].is_string())
            out.push_back({item["id"].get<std::string>(), item["name"].get<std::string>(),
                           item.value("collapsed", false)});
    return out;
}

inline Json SerializeFolders(const std::vector<FolderMetadata> &folders)
{
    Json out = Json::array();
    for (const auto &folder : folders)
        out.push_back({{"id", folder.id}, {"name", folder.name}, {"collapsed", folder.collapsed}});
    return out;
}

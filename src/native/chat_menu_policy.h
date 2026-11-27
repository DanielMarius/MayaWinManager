#pragma once
#include <cstddef>
#include <cctype>
#include <string>
#include <vector>

inline std::string NativeCyberFrostVersion(const std::string &id)
{
    constexpr char prefix[] = "cyber-frost-";
    constexpr size_t prefixLength = sizeof(prefix) - 1;
    if (id.size() < prefixLength) return {};
    for (size_t i = 0; i < prefixLength; ++i)
        if (std::tolower((unsigned char)id[i]) != prefix[i]) return {};
    size_t end = prefixLength;
    while (end < id.size() && (std::isdigit((unsigned char)id[end]) || id[end] == '.')) ++end;
    return id.substr(prefixLength, end - prefixLength);
}

inline std::string NativeChatModelButtonName(const std::string &id)
{
    const auto fineTuneVersion = NativeCyberFrostVersion(id);
    if (!fineTuneVersion.empty()) return "Qwen-" + fineTuneVersion + "-Flash-Next";
    std::vector<std::string> words;
    std::string word;
    for (unsigned char c : id)
    {
        if (c == '-' || c == '_' || std::isspace(c))
        {
            if (!word.empty()) { words.push_back(std::move(word)); word.clear(); }
        }
        else word.push_back((char)std::tolower(c));
    }
    if (!word.empty()) words.push_back(std::move(word));
    std::string name;
    for (auto &part : words)
    {
        if (part == "glm" || part == "deepseek" || part == "minimax" || part == "qwen")
        {
            part[0] = (char)std::toupper((unsigned char)part[0]);
            if (part == "Deepseek") part = "DeepSeek";
            else if (part == "Minimax") part = "MiniMax";
        }
        else if (part.size() > 1 && std::isalpha((unsigned char)part[0]) && std::isdigit((unsigned char)part[1]))
            part[0] = (char)std::toupper((unsigned char)part[0]);
        else if (!part.empty()) part[0] = (char)std::toupper((unsigned char)part[0]);
        if (!name.empty()) name.push_back(' ');
        name += part;
    }
    return name.empty() ? "Model" : name;
}

enum class NativeChatMenuMode { INSTRUCTIONS_ONLY, FULL };

struct NativeChatToolbarActions
{
    bool editLast, regenerate, cancelEdit, answerOptions;
};

constexpr bool NativeChatToolbarActionsEnabled(bool busy) noexcept
{
    return !busy;
}

constexpr NativeChatToolbarActions NativeChatToolbarActionsFor(bool chatPage, bool hasUser, bool hasAnswer, bool editing) noexcept
{
    return {chatPage && hasUser && !editing, chatPage && hasAnswer && !editing,
            chatPage && editing, chatPage && hasAnswer && !editing};
}

constexpr NativeChatMenuMode NativeChatMenuFor(size_t messageCount) noexcept
{
    return messageCount ? NativeChatMenuMode::FULL : NativeChatMenuMode::INSTRUCTIONS_ONLY;
}

constexpr int NativeChatMenuVisibleRow(bool keyboard, int selectedRow, int pointedRow, size_t rowCount) noexcept
{
    const int row = keyboard ? selectedRow : pointedRow;
    return row >= 0 && static_cast<size_t>(row) < rowCount ? row : -1;
}

inline std::wstring NativeChatFolderDestination(bool selectedFolderIsCurrent, const std::wstring &destination)
{
    return selectedFolderIsCurrent ? std::wstring() : destination;
}

constexpr bool NativeFolderCollapsedAfterChatMove(bool selectedFolderIsCurrent, bool wasCollapsed) noexcept
{
    return selectedFolderIsCurrent ? wasCollapsed : false;
}

constexpr bool NativeFolderDeleteCanRemove(size_t savedChatCount, size_t folderChatCount) noexcept
{
    return savedChatCount == folderChatCount;
}

constexpr bool NativeComposerShowsTools(size_t serverCount) noexcept
{
    return serverCount != 0;
}

constexpr int NativeChatSelectionAfterDelete(int currentIndex, int removedIndex, size_t remainingCount) noexcept
{
    if (!remainingCount || currentIndex < 0) return -1;
    if (currentIndex == removedIndex) return 0;
    return currentIndex > removedIndex ? currentIndex - 1 : currentIndex;
}

enum class NativeChatModelMenuAction { NONE, CONTEXT, SAMPLING, MODELS };
constexpr int NativeChatModelMenuCommandBase = 1000;
constexpr int NativeGreetingModelButtonId = 275;
constexpr int NativeChatModelButtonId = 276;
constexpr bool NativeChatGreetingVisible(bool chatPage, bool hasChat, bool hasMessages, bool editing) noexcept
{
    return chatPage && (!hasChat || (!hasMessages && !editing));
}
constexpr bool NativeChatModelButtonShouldOpen(int id, bool chatPage, bool hasChat, bool emptyChat) noexcept
{
    if (!chatPage) return false;
    if (!hasChat) return id == NativeGreetingModelButtonId;
    return (id == NativeGreetingModelButtonId && emptyChat) ||
           (id == NativeChatModelButtonId && !emptyChat);
}
constexpr NativeChatModelMenuAction NativeChatModelMenuActionFor(int selected) noexcept
{
    return selected == 1 ? NativeChatModelMenuAction::CONTEXT :
           selected == 2 ? NativeChatModelMenuAction::SAMPLING :
           selected == 3 ? NativeChatModelMenuAction::MODELS : NativeChatModelMenuAction::NONE;
}

constexpr int NativeChatModelMenuModelIndexFor(int selected, size_t modelCount) noexcept
{
    if (selected < NativeChatModelMenuCommandBase) return -1;
    const size_t index = static_cast<size_t>(selected - NativeChatModelMenuCommandBase);
    return index < modelCount ? static_cast<int>(index) : -1;
}

constexpr bool NativeQueuedModelRefreshShouldRun(bool pending, bool idle, bool visible) noexcept
{
    return pending && idle && visible;
}

constexpr bool NativeModelSwitchBlockedByWork(bool busy, bool chat, bool routinePoll) noexcept
{
    return busy && !chat && !routinePoll;
}

constexpr bool NativeChatModelMenuReopenAfterLoad(bool refreshed, int selected) noexcept
{
    return refreshed && selected == 0;
}

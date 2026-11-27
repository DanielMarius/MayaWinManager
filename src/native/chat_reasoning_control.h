#pragma once

#include <cstddef>
#include <string>

inline constexpr int NativeChatReasoningControlId = 124;
inline constexpr int NativeChatAblationControlId = 6000;
inline constexpr int NativeChatReasoningMenuCommandBase = 1400;

constexpr int NativeChatReasoningMenuIndexFor(int selected, std::size_t count) noexcept
{
    if (selected < NativeChatReasoningMenuCommandBase) return -1;
    const std::size_t index = static_cast<std::size_t>(selected - NativeChatReasoningMenuCommandBase);
    return index < count ? static_cast<int>(index) : -1;
}

template <typename Persist>
bool CommitChatReasoningChoice(std::string &effort, std::string &modelFor, const std::string &selected,
                               const std::string &model, Persist persist)
{
    const auto previousEffort = effort;
    const auto previousModel = modelFor;
    effort = selected;
    modelFor = model;
    if (persist()) return true;
    effort = previousEffort;
    modelFor = previousModel;
    return false;
}

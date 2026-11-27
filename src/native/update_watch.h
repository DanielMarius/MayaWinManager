#pragma once
#include <string>

enum class MayaUpdateWatchState { WAITING, COMPLETE, FAILED };

inline constexpr bool MayaUpdateBadgeVisible(bool newer, bool updating) noexcept
{
    return newer && !updating;
}

inline MayaUpdateWatchState MayaUpdateWatchStep(bool online, const std::string &version,
                                                 const std::string &target, bool sawDowntime,
                                                 const std::string &serverState)
{
    if (!online) return MayaUpdateWatchState::WAITING;
    if (!target.empty() && version == target) return MayaUpdateWatchState::COMPLETE;
    if (sawDowntime && serverState != "updating") return MayaUpdateWatchState::FAILED;
    return MayaUpdateWatchState::WAITING;
}

inline std::string MayaUpdateRecoveryTarget(const Json &result)
{
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object()) return {};
    const auto &update = result["update"];
    if (!update.contains("state") || !update["state"].is_object()) return {};
    const auto &state = update["state"];
    return state.contains("state") && state["state"].is_string() && state["state"].get<std::string>() == "updating" &&
                   state.contains("to") && state["to"].is_string()
               ? state["to"].get<std::string>() : std::string();
}

inline std::string MayaUpdateRecoveryError(const Json &result)
{
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object()) return {};
    const auto &update = result["update"];
    if (!update.contains("state") || !update["state"].is_object()) return {};
    const auto &state = update["state"];
    return state.contains("state") && state["state"].is_string() && state["state"].get<std::string>() == "failed" &&
                   state.contains("error") && state["error"].is_string()
               ? state["error"].get<std::string>() : std::string();
}

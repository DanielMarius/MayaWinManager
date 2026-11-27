#pragma once
#include "http.h"
#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <string_view>

inline bool MayaUpdateCheckEligible(bool checked, bool inFlight, bool watching, bool recovering) noexcept
{
    return !checked && !inFlight && !watching && !recovering;
}

inline bool MayaUpdateCanFastForward(std::string_view currentHead, std::string_view mergeBase) noexcept
{
    return !currentHead.empty() && currentHead == mergeBase;
}

inline bool MayaUpdateRecheckedTagMatches(const Json &check, const std::string &requestedTag)
{
    if (requestedTag.empty() || !check.is_object() || !check.contains("latest") || !check["latest"].is_object() ||
        !check["latest"].contains("tag") || !check["latest"]["tag"].is_string())
        return false;
    return check["latest"]["tag"].get<std::string>() == requestedTag;
}

inline bool MayaUpdateCheckVerified(const Json &update)
{
    if (!update.is_object() ||
        (update.contains("enabled") && (!update["enabled"].is_boolean() || !update["enabled"].get<bool>())) ||
        (update.contains("error") && !update["error"].is_null() &&
         (!update["error"].is_string() || !update["error"].get<std::string>().empty())) ||
        !update.contains("current") || !update["current"].is_string() || update["current"].get<std::string>().empty() ||
        !update.contains("latest") || !update["latest"].is_object() ||
        !update["latest"].contains("tag") || !update["latest"]["tag"].is_string() ||
        update["latest"]["tag"].get<std::string>().empty() ||
        !update["latest"].contains("version") || !update["latest"]["version"].is_string() ||
        update["latest"]["version"].get<std::string>().empty() ||
        !update.contains("newer") || !update["newer"].is_boolean())
        return false;
    return update["newer"].get<bool>()
        ? update["current"].get<std::string>() != update["latest"]["version"].get<std::string>()
        : update["current"].get<std::string>() == update["latest"]["version"].get<std::string>();
}

inline std::wstring MayaDependencyCheckModel(const Json &config)
{
    const auto installer = config.is_object() && config.contains("installer") && config["installer"].is_object()
        ? config["installer"] : Json::object();
    const auto family = installer.contains("family") && installer["family"].is_string()
        ? installer["family"].get<std::string>() : std::string();
    if (family == "deepseek") return L"DeepSeek-V4.1-Flash-1017";
    if (family == "minimax") return L"MiniMax-M3.1-Flash";
    if (family == "qwen") return L"CYBER-FROST-3.8-PS";
    std::wstring exe;
    if (config.is_object() && config.contains("exe") && config["exe"].is_string())
        exe = std::filesystem::path(config["exe"].get<std::string>()).filename().wstring();
    std::transform(exe.begin(), exe.end(), exe.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    const auto args = config.is_object() && config.contains("args") && config["args"].is_array()
        ? config["args"] : Json::array();
    auto hasArg = [&](const char *value)
    {
        return std::find(args.begin(), args.end(), value) != args.end();
    };
    if (exe.rfind(L"maya-mm3", 0) == 0) return L"MiniMax-M3.1-Flash";
    if (exe.rfind(L"maya-ds4", 0) == 0 || exe.rfind(L"strata-ds4", 0) == 0 || hasArg("--pack"))
        return L"DeepSeek-V4.1-Flash-1017";
    if (hasArg("--ple-gguf")) return L"CYBER-FROST-3.8-PS";
    return {};
}

inline bool MayaInstalledModelConfigName(std::wstring name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return (wchar_t)std::towlower(c); });
    constexpr std::wstring_view shared = L".shared-settings.json";
    return name.size() > 10 && name.rfind(L"maya-", 0) == 0 && name.substr(name.size() - 5) == L".json" &&
           (name.size() < shared.size() || name.compare(name.size() - shared.size(), shared.size(), shared) != 0);
}

inline bool MayaHasInstalledModelConfig(const std::filesystem::path &root)
{
    std::error_code error;
    std::filesystem::directory_iterator entry(root, error), end;
    if (error) throw std::filesystem::filesystem_error("Cannot inspect Maya model configs", root, error);
    while (entry != end)
    {
        const bool regular = entry->is_regular_file(error);
        if (error) throw std::filesystem::filesystem_error("Cannot inspect Maya model configs", entry->path(), error);
        if (regular && MayaInstalledModelConfigName(entry->path().filename().wstring())) return true;
        entry.increment(error);
        if (error) throw std::filesystem::filesystem_error("Cannot inspect Maya model configs", root, error);
    }
    return false;
}

inline std::wstring MayaPythonSelectorArgument(const std::wstring &selector)
{
    return selector.empty() ? std::wstring() : L" " + selector;
}

inline std::string MayaBoundedUtf8(std::string value, size_t maxBytes)
{
    if (value.size() <= maxBytes) return value;
    value.resize(maxBytes);
    while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0x80) value.pop_back();
    if (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xC0) == 0xC0) value.pop_back();
    return value + "...";
}

inline std::string MayaUpdatePreflightText(const Json &result)
{
    auto text = [](const Json &object, const char *key)
    {
        return object.is_object() && object.contains(key) && object[key].is_string()
                   ? object[key].get<std::string>() : std::string();
    };
    auto boolean = [](const Json &object, const char *key)
    {
        return object.is_object() && object.contains(key) && object[key].is_boolean()
                   ? object[key].get<bool>() : false;
    };
    auto preservedCount = [](const Json &object)
    {
        if (!object.is_object() || !object.contains("preserved_paths")) return size_t(0);
        const auto &count = object["preserved_paths"];
        if (count.is_number_unsigned()) return count.get<size_t>();
        if (!count.is_number_integer()) return size_t(0);
        const auto signedCount = count.get<int64_t>();
        return signedCount > 0 ? static_cast<size_t>(signedCount) : size_t(0);
    };
    const auto update = result.is_object() && result.contains("update") && result["update"].is_object()
                            ? result["update"] : Json::object();
    const auto latest = update.contains("latest") && update["latest"].is_object()
                            ? update["latest"] : Json::object();
    std::string out = "Maya v" + text(latest, "version") + " is available.\n";
    const bool online = boolean(result, "online");
    out += online ? "Check mode: Maya API online.\n"
                  : "Check mode: Maya API offline; Manager will apply the source update and start Maya.\n";
    const bool ready = boolean(result, "ready") && boolean(result, "dependency_ok");
    out += ready ? "Preflight passed.\n" : "Preflight is blocked.\n";
    if (boolean(result, "managed_restart"))
        out += online ? "Update route: Manager will wait for Maya's request queue, make and verify backups, update the source, restart Maya and verify the model.\n"
                      : "Update route: Manager will make and verify backups, update the source, start Maya and verify the model.\n";
    const auto blocker = text(update, "blocker");
    if (boolean(update, "preserve_tracked_changes"))
        out += std::to_string(preservedCount(update)) +
               " locally changed files will be backed up and reapplied.\n";
    if (!ready)
    {
        const auto byHand = MayaBoundedUtf8(text(update, "by_hand"), 1000);
        if (!byHand.empty()) out += "Update by hand: " + byHand + "\n";
    }
    if (!blocker.empty()) out += "Blocker: " + blocker + "\n";
    auto summary = MayaBoundedUtf8(text(latest, "summary"), 1000);
    if (!summary.empty()) out += "\nRelease summary:\n" + summary + "\n";
    auto report = text(result, "dependency_report");
    if (!report.empty())
    {
        if (report.size() > 6000) report.resize(6000);
        out += "\nDependency check:\n" + report;
    }
    return out;
}

inline bool MayaFreshInstallCanStart(const Json &result)
{
    return result.is_object() && result.contains("mode") && result["mode"].is_string() &&
           result["mode"].get<std::string>() == "fresh" &&
           result.contains("installed") && result["installed"].is_boolean() && !result["installed"].get<bool>() &&
           result.contains("ready") && result["ready"].is_boolean() && result["ready"].get<bool>() &&
           result.contains("dependency_ok") && result["dependency_ok"].is_boolean() &&
           result["dependency_ok"].get<bool>();
}

inline bool MayaUpdateCanStart(const Json &result)
{
    if (result.is_object() && result.contains("mode") && result["mode"].is_string() &&
        result["mode"].get<std::string>() == "fresh")
        return MayaFreshInstallCanStart(result);
    if (!result.is_object() || !result.contains("ready") || !result["ready"].is_boolean() ||
        !result["ready"].get<bool>() || !result.contains("dependency_ok") ||
        !result["dependency_ok"].is_boolean() || !result["dependency_ok"].get<bool>() ||
        !result.contains("update") ||
        !result["update"].is_object()) return false;
    const auto &update = result["update"];
    if (!MayaUpdateCheckVerified(update) || !update["newer"].get<bool>() ||
        !update.contains("can_update") || !update["can_update"].is_boolean() || !update["can_update"].get<bool>() ||
        !update.contains("latest") || !update["latest"].is_object()) return false;
    const auto &latest = update["latest"];
    return latest.contains("version") && latest["version"].is_string() && !latest["version"].get<std::string>().empty() &&
           latest.contains("tag") && latest["tag"].is_string() && !latest["tag"].get<std::string>().empty();
}

inline std::string MayaFreshInstallPreflightText(const Json &result)
{
    std::string out = "Maya is not installed yet.\n";
    out += MayaFreshInstallCanStart(result) ? "Fresh-install preflight passed.\n"
                                            : "Fresh-install preflight is blocked.\n";
    if (result.is_object() && result.contains("dependency_report") && result["dependency_report"].is_string())
    {
        auto report = MayaBoundedUtf8(result["dependency_report"].get<std::string>(), 6000);
        if (!report.empty()) out += "\nDependency check:\n" + report;
    }
    return out;
}

inline std::string MayaUpdateSummaryText(const Json &result)
{
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object() ||
        !result["update"].contains("newer") || !result["update"]["newer"].is_boolean() ||
        !result["update"]["newer"].get<bool>()) return {};
    const auto &update = result["update"];
    const auto latest = update.contains("latest") && update["latest"].is_object() ? update["latest"] : Json::object();
    auto summary = MayaBoundedUtf8(latest.contains("summary") && latest["summary"].is_string()
                                       ? latest["summary"].get<std::string>() : std::string(), 1000);
    const auto version = latest.contains("version") && latest["version"].is_string()
                           ? latest["version"].get<std::string>() : std::string("?");
    return "Maya v" + version + " is available.\r\n" + summary;
}

inline std::string MayaUpdateCardText(const Json &result)
{
    if (result.is_object() && result.contains("mode") && result["mode"].is_string() &&
        result["mode"].get<std::string>() == "fresh")
        return MayaFreshInstallCanStart(result) ? "Maya is not installed. Setup checks passed; ready to install."
                                                : "Maya is not installed. Setup is blocked; review the prerequisites.";
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object())
        return "Couldn't check for Maya updates.";
    const auto &update = result["update"];
    const auto current = update.contains("current") && update["current"].is_string()
                           ? update["current"].get<std::string>() : std::string("?");
    if (update.contains("state") && update["state"].is_object())
    {
        const auto &state = update["state"];
        const auto status = state.contains("state") && state["state"].is_string()
                              ? state["state"].get<std::string>() : std::string();
        const auto to = state.contains("to") && state["to"].is_string() ? state["to"].get<std::string>() : std::string("?");
        if (status == "updating") return "Updating Project Maya to v" + to + "…";
        if (status == "failed")
            return "The update did not happen: " + MayaBoundedUtf8(
                state.contains("error") && state["error"].is_string() ? state["error"].get<std::string>() : "unknown error", 500);
    }
    if (update.contains("enabled") && update["enabled"].is_boolean() && !update["enabled"].get<bool>())
        return "Maya update checks are disabled (MAYA_UPDATE_CHECK=0).";
    if (update.contains("error") && update["error"].is_string() && !update["error"].get<std::string>().empty())
        return "Couldn't check for updates. Project Maya v" + current + " is running. " +
               MayaBoundedUtf8(update["error"].get<std::string>(), 500);
    if (!MayaUpdateCheckVerified(update)) return "Couldn't verify the latest Maya release.";
    if (update["newer"].get<bool>())
    {
        auto text = MayaUpdateSummaryText(result);
        const auto blocker = update.contains("blocker") && update["blocker"].is_string()
                                 ? update["blocker"].get<std::string>() : std::string();
        if (!blocker.empty()) text += "\r\nBlocked: " + MayaBoundedUtf8(blocker, 400);
        else if (!result.contains("ready") || !result["ready"].is_boolean() || !result["ready"].get<bool>())
            text += "\r\nPreflight did not pass; open details before proceeding.";
        return text;
    }
    return "Up to date. Project Maya v" + current + " is the latest release.";
}

inline std::string MayaUpdateStatusText(const Json &result)
{
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object())
        return "Couldn't verify the latest Maya release.";
    const auto &update = result["update"];
    if (update.contains("state") && update["state"].is_object())
    {
        const auto &state = update["state"];
        if (state.contains("state") && state["state"].is_string())
        {
            if (state["state"].get<std::string>() == "updating") return "Maya update is in progress.";
            if (state["state"].get<std::string>() == "failed") return "Maya update failed; review About and updates.";
        }
    }
    if (update.contains("enabled") && update["enabled"].is_boolean() && !update["enabled"].get<bool>())
        return "Maya update checks are disabled.";
    if (!MayaUpdateCheckVerified(update)) return "Couldn't verify the latest Maya release.";
    if (update["newer"].get<bool>())
        return result.contains("ready") && result["ready"].is_boolean() && result["ready"].get<bool>()
            ? "Update available; choose Install in About and updates."
            : "Update available; review preflight details in About and updates.";
    return "Maya is up to date.";
}

inline std::string MayaUpdateNotesUrl(const Json &result)
{
    if (!result.is_object() || !result.contains("update") || !result["update"].is_object()) return {};
    const auto &update = result["update"];
    const auto latest = update.contains("latest") && update["latest"].is_object() ? update["latest"] : Json::object();
    const auto url = latest.contains("url") && latest["url"].is_string()
                       ? latest["url"].get<std::string>() : std::string();
    return url.rfind("https://github.com/", 0) == 0 &&
                   url.find_first_of(" \t\r\n") == std::string::npos ? url : std::string();
}

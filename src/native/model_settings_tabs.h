#pragma once

#include <algorithm>
#include <array>
#include <string>
#include <vector>
#include "model_settings.h"

enum class NativeModelSettingsGroup { MODEL, GPUS_MEMORY, PROMPTS_CACHE, SPEED };
enum class NativeModelSettingsTabMarker { NONE, CHANGED, WARNING };

constexpr NativeModelSettingsTabMarker ModelSettingsTabMarker(bool changed, bool warning) noexcept
{
    return warning ? NativeModelSettingsTabMarker::WARNING
                   : changed ? NativeModelSettingsTabMarker::CHANGED : NativeModelSettingsTabMarker::NONE;
}

inline NativeModelSettingsGroup ModelSettingsGroupForKey(const std::string &key)
{
    if (key == "gpus" || key == "tp" || key == "reserve_mib" || key == "reserve_gb" || key == "mmap" ||
        key == "expert_cache" || key == "ram_gb" || key == "cache_gb") return NativeModelSettingsGroup::GPUS_MEMORY;
    if (key == "prefill" || key == "kv_streaming" || key == "kv_resident" || key == "kv_cache" || key == "kv")
        return NativeModelSettingsGroup::PROMPTS_CACHE;
    if (key == "prefill_chunk" || key == "cpu_threads" || key == "prompt_lookup" ||
        key == "fast_prefill" || key == "dspark_conf")
        return NativeModelSettingsGroup::SPEED;
    return NativeModelSettingsGroup::MODEL;
}

inline const wchar_t *ModelSettingsInvalidWarning(const std::string &key)
{
    if (key == "dspark_conf") return L"A number from 0 to 1, or empty.";
    if (key == "reserve_gb" || key == "cache_gb") return L"A number of GB, or empty for the default.";
    if (key == "reserve_mib") return L"A whole number of MiB, or empty for the default.";
    if (key == "expert_cache" || key == "prefill") return L"A whole number, or Auto.";
    if (key == "kv_resident") return L"A whole number, or empty for 32,768.";
    if (key == "cpu_threads" || key == "prefill_chunk") return L"A whole number, or empty.";
    if (key == "ram_gb") return L"A whole number of GB, or empty for all the engine can take.";
    return L"Enter a valid value.";
}

inline int NextModelSettingsTab(int current, int direction, const std::array<bool, 4> &available)
{
    if (current < 0 || current >= 4 || (direction != -1 && direction != 1)) return -1;
    for (int step = 1; step <= 4; ++step)
    {
        const int next = (current + direction * step + 8) % 4;
        if (available[next]) return next;
    }
    return -1;
}

inline bool ModelSettingsGroupChanged(const Json &saved, const Json &draft, NativeModelSettingsGroup group)
{
    if (!saved.is_object() || !draft.is_object()) return false;
    for (auto it = saved.begin(); it != saved.end(); ++it)
        if (it.key() != "env" && ModelSettingsGroupForKey(it.key()) == group &&
            (!draft.contains(it.key()) || draft[it.key()] != it.value())) return true;
    for (auto it = draft.begin(); it != draft.end(); ++it)
        if (it.key() != "env" && ModelSettingsGroupForKey(it.key()) == group && !saved.contains(it.key())) return true;
    return false;
}

inline bool ModelSettingsFieldChanged(const Json &saved, const Json &draft, const std::string &key)
{
    if (!saved.is_object() || !draft.is_object()) return false;
    const bool hadSaved = saved.contains(key), hasDraft = draft.contains(key);
    return hadSaved != hasDraft || (hadSaved && saved[key] != draft[key]);
}

inline void AppendModelSettingsWarning(std::array<std::vector<std::wstring>, 4> &warnings, int group,
                                      const std::wstring &field, const std::wstring &warning)
{
    warnings.at(static_cast<size_t>(group)).push_back(field + L": " + warning);
}

inline bool ModelSettingsChanged(const Json &saved, const Json &draft)
{
    if (!saved.is_object() || !draft.is_object()) return false;
    for (auto it = saved.begin(); it != saved.end(); ++it)
        if (!draft.contains(it.key()) || draft[it.key()] != it.value()) return true;
    return draft.contains("ablate") && !saved.contains("ablate");
}

inline bool NativeModelAblationAvailable(const Json &choices)
{
    if (!choices.is_object() || !choices.contains("ablate") || !choices["ablate"].is_object()) return false;
    const auto &ablate = choices["ablate"];
    return ablate.contains("available") && ablate["available"].is_boolean() && ablate["available"].get<bool>();
}

inline bool NativeModelAblationVisible(const Json &settings, const Json &choices)
{
    return NativeModelAblationAvailable(choices) ||
           (settings.is_object() && settings.contains("ablate") && settings["ablate"].is_boolean() &&
            settings["ablate"].get<bool>());
}

constexpr bool NativeModelSettingsApplyEnabled(bool changed, bool valid, bool previewMatches) noexcept
{
    return changed && valid && previewMatches;
}

inline bool ModelSettingsEnvironmentChanged(const Json &saved, const Json &draft)
{
    if (!saved.is_object() || !draft.is_object()) return false;
    return saved.value("env", Json()) != draft.value("env", Json::object());
}

inline std::string ModelSettingsCommandPreviewText(const Json &current, const Json &saved)
{
    if (!current.is_object()) return {};
    const auto stringify = [](const Json &value) {
        return value.is_string() ? value.get<std::string>() : value.dump();
    };
    const auto pairs = [&](const Json &value) {
        std::vector<std::string> out;
        const auto args = value.is_object() ? value.value("args", Json::array()) : Json::array();
        if (!args.is_array()) return out;
        for (size_t i = 0; i < args.size(); ++i)
        {
            const auto arg = stringify(args[i]);
            if (i + 1 < args.size() && arg.rfind("--", 0) == 0 && stringify(args[i + 1]).rfind("--", 0) != 0)
                out.push_back(arg + " " + stringify(args[++i]));
            else out.push_back(arg);
        }
        return out;
    };
    std::string out = "Environment:\n";
    const auto env = current.value("env", Json::object());
    const auto oldEnv = saved.is_object() ? saved.value("env", Json::object()) : Json::object();
    if (env.is_object()) for (auto it = env.begin(); it != env.end(); ++it)
        out += (oldEnv.is_object() && oldEnv.contains(it.key()) && oldEnv[it.key()] == it.value() ? "  " : "+ ") +
               it.key() + "=" + stringify(it.value()) + "\n";
    const auto exe = current.value("exe", Json());
    out += "Executable: " + (exe.is_null() ? std::string() : stringify(exe)) + "\nArguments:\n";
    const auto oldArgs = pairs(saved);
    for (const auto &arg : pairs(current))
        out += (std::find(oldArgs.begin(), oldArgs.end(), arg) == oldArgs.end() ? "+ " : "  ") + arg + "\n";
    return out;
}

inline std::array<std::vector<std::wstring>, 4> ModelSettingsConditionalWarningMessages(
    const Json &saved, const Json &draft, const Json &range, const std::string &family)
{
    std::array<std::vector<std::wstring>, 4> warnings{};
    if (!draft.is_object()) return warnings;
    const auto number = [](const Json &settings, const char *key, double fallback) {
        return settings.contains(key) && settings[key].is_number() ? settings[key].get<double>() : fallback;
    };
    const auto text = [](const Json &settings, const char *key) {
        return settings.contains(key) && settings[key].is_string() ? settings[key].get<std::string>() : std::string();
    };
    const double context = number(draft, "context", 0), parallel = number(draft, "parallel", 1);
    const double tested = range.contains("tested") && range["tested"].is_number()
        ? range["tested"].get<double>() : 262144.0;
    const auto ctxfmt = [](double tokens) {
        const auto label = ModelContextLabel(tokens);
        return std::wstring(label.begin(), label.end());
    };
    if (saved.is_object() && saved.contains("context") && saved["context"].is_number() &&
        saved["context"] != draft.value("context", Json()) && context > tested)
        warnings[(int)NativeModelSettingsGroup::MODEL].push_back(
            L"Beyond " + ctxfmt(tested) + L" is untested on this engine.");
    if (family == "minimax" && parallel > 1)
        warnings[(int)NativeModelSettingsGroup::MODEL].push_back(
            std::to_wstring((long long)parallel) + L" answers at once: each keeps a whole cache of this size in VRAM. "
            L"The engine shortens a context that does not fit.");
    if (family != "glm" && parallel > 1 && context * parallel > 1048576.0)
        warnings[(int)NativeModelSettingsGroup::MODEL].push_back(
            std::to_wstring((long long)parallel) + L" conversations of " + ctxfmt(context) +
            L" each: a smaller context leaves the experts more VRAM.");
    const auto draftMode = text(draft, "draft");
    if (family == "glm" && draftMode == "dflash2" && parallel > 1)
        warnings[(int)NativeModelSettingsGroup::MODEL].push_back(
            L"DFlash2 drafts for one answer at a time: with " + std::to_wstring((long long)parallel) +
            L" at once the engine drafts with MTP.");
    else if (family != "glm" && draftMode == "dspark" && parallel > 1)
        warnings[(int)NativeModelSettingsGroup::MODEL].push_back(
            L"DSpark drafts for one answer at a time: with " + std::to_wstring((long long)parallel) +
            L" at once the engine decodes without it.");
    if (draft.contains("fast_prefill") && draft["fast_prefill"].is_boolean() &&
        draft["fast_prefill"].get<bool>())
        warnings[(int)NativeModelSettingsGroup::SPEED].push_back(
            L"Fast prompt reading may shift answers slightly. Leave it off when answer precision matters.");
    const auto &gpus = draft.value("gpus", Json::array());
    const double tp = number(draft, "tp", 1);
    if (family == "minimax" && tp > (gpus.is_array() ? gpus.size() : 0))
        warnings[(int)NativeModelSettingsGroup::GPUS_MEMORY].push_back(
            L"A group of " + std::to_wstring((long long)tp) + L" needs at least " +
            std::to_wstring((long long)tp) + L" GPUs picked.");
    else if (family != "glm" && tp == 2 && (!gpus.is_array() || gpus.size() < 2))
        warnings[(int)NativeModelSettingsGroup::GPUS_MEMORY].push_back(
            L"With one GPU picked, 2 runs its layers alone and reads prompts slower: use 1.");
    else if (family != "glm" && tp > std::max(2.0, gpus.is_array() ? (double)gpus.size() : 0.0))
        warnings[(int)NativeModelSettingsGroup::GPUS_MEMORY].push_back(
            L"A group of " + std::to_wstring((long long)tp) + L" needs at least " +
            std::to_wstring((long long)tp) + L" GPUs picked.");
    if (family != "glm" && draftMode == "dspark" && number(draft, "dspark_conf", 0) > 1)
        warnings[(int)NativeModelSettingsGroup::SPEED].push_back(L"A number from 0 to 1, or empty.");
    return warnings;
}

inline std::array<bool, 4> ModelSettingsConditionalWarnings(const Json &saved, const Json &draft,
                                                            const Json &range, const std::string &family)
{
    const auto messages = ModelSettingsConditionalWarningMessages(saved, draft, range, family);
    std::array<bool, 4> warnings{};
    for (size_t i = 0; i < warnings.size(); ++i) warnings[i] = !messages[i].empty();
    return warnings;
}

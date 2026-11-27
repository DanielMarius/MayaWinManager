#pragma once
#include "http.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

inline std::string ChatSamplingFamily(std::string model)
{
    std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (model.find("deepseek") != std::string::npos) return "deepseek";
    if (model.find("minimax") != std::string::npos) return "minimax";
    if (model.find("qwen") != std::string::npos || model.find("cyber-frost") != std::string::npos) return "qwen";
    return "glm";
}

inline std::string ChatSamplingPresetName(const Json &values)
{
    const auto number = [&](const char *key) {
        return values.contains(key) && values[key].is_number() ? values[key].get<double>() :
            std::numeric_limits<double>::quiet_NaN();
    };
    const auto is = [&](const char *key, double expected) { return std::abs(number(key) - expected) < 1e-9; };
    if (is("temperature", 0)) return "Precise";
    if (is("temperature", 0.6) && is("top_p", 0.95) && is("top_k", 20) && is("min_p", 0)) return "Balanced";
    if (is("temperature", 1.0) && is("top_p", 0.95) && is("top_k", 0) && is("min_p", 0.05)) return "Creative";
    return "Custom";
}

inline constexpr const wchar_t *ChatSamplingPresetValue(int preset, int field) noexcept
{
    constexpr const wchar_t *values[3][4] = {
        {L"0", nullptr, nullptr, nullptr},
        {L"0.6", L"0.95", L"20", L"0"},
        {L"1.0", L"0.95", L"0", L"0.05"}};
    return preset >= 0 && preset < 3 && field >= 0 && field < 4 ? values[preset][field] : nullptr;
}

inline std::vector<std::string> ChatSamplingEfforts(const std::string &family)
{
    if (family == "minimax") return {"none", "auto", "low", "medium", "high", "xhigh", "max"};
    if (family == "qwen") return {"none", "low", "medium", "xhigh"};
    return {"none", "low", "high", "max"};
}

inline std::string ChatSamplingEffortForModel(const std::string &effort, const std::string &model)
{
    const auto family = ChatSamplingFamily(model);
    const auto choices = ChatSamplingEfforts(family);
    if (std::find(choices.begin(), choices.end(), effort) != choices.end()) return effort;
    std::vector<std::string> nearest;
    if (effort == "xhigh") nearest = {"max", "high"};
    else if (effort == "medium") nearest = {"high", "low"};
    else if (effort == "max") nearest = {"xhigh", "high"};
    else if (effort == "high") nearest = {"medium", "max"};
    else if (effort == "low") nearest = {"low"};
    else if (effort == "none") nearest = {"none"};
    for (const auto &candidate : nearest)
        if (std::find(choices.begin(), choices.end(), candidate) != choices.end()) return candidate;
    return {};
}
inline std::string ChatSamplingEffortForChat(const std::string &effort, const std::string &savedFor,
                                             const std::string &model, const std::string &fallback)
{
    if (!savedFor.empty() && savedFor[0] != '|' && !model.empty() && savedFor != model) return fallback;
    const auto mapped = ChatSamplingEffortForModel(effort, model);
    return mapped.empty() ? fallback : mapped;
}

inline bool NativeChatDefaultThinkBudget(const Json &health, const Json &saved, const std::string &model,
                                         const std::string &effort, bool chatPicked,
                                         const std::string &chatModel)
{
    if (!health.is_object() || !health.contains("thinking_budget_max") ||
        !health["thinking_budget_max"].is_number() || health["thinking_budget_max"].get<double>() <= 0)
        return false;
    if (chatPicked && !model.empty() && chatModel == model) return false;
    const auto family = ChatSamplingFamily(model);
    if (saved.is_object() && saved.contains(family) && saved[family].is_object())
    {
        const auto &preference = saved[family];
        if (preference.contains("reasoning_effort") && preference["reasoning_effort"].is_string() &&
            preference["reasoning_effort"].get<std::string>() == effort)
            return false;
    }
    return true;
}

inline void AddNativeChatDefaultThinkBudget(Json &request, const Json &health, const Json &saved,
                                             const std::string &model, const std::string &effort,
                                             bool chatPicked, const std::string &chatModel)
{
    if (NativeChatDefaultThinkBudget(health, saved, model, effort, chatPicked, chatModel))
        request["strata_level_default"] = true;
}

inline std::string ChatSamplingEffortLabel(const std::string &effort)
{
    if (effort == "none") return "Off";
    if (effort == "xhigh") return "XHigh";
    std::string label = effort;
    if (!label.empty()) label[0] = (char)std::toupper((unsigned char)label[0]);
    return label;
}
inline bool ChatSamplingUsesFilters(const Json &value)
{
    return value.is_object() && value.contains("temperature") && value["temperature"].is_number() &&
           value["temperature"].get<double>() > 0;
}

inline Json ResolveChatSampling(const Json &saved, const std::string &model, const Json &recommended)
{
    const auto family = ChatSamplingFamily(model);
    Json out = {{"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0},
                {"max_tokens", nullptr}, {"seed", nullptr}, {"repetition_penalty", nullptr},
                {"frequency_penalty", nullptr}, {"presence_penalty", nullptr}};
    if (recommended.is_object())
        for (const auto *key : {"temperature", "top_p", "top_k", "min_p", "reasoning_effort"})
            if (recommended.contains(key) && !recommended[key].is_null()) out[key] = recommended[key];
    if (!out.contains("reasoning_effort"))
        out["reasoning_effort"] = family == "qwen" ? "xhigh" : family == "minimax" ? "auto" : family == "deepseek" ? "high" : "max";
    if (saved.is_object() && saved.contains(family) && saved[family].is_object())
        for (auto it = saved[family].begin(); it != saved[family].end(); ++it) out[it.key()] = it.value();
    const auto efforts = ChatSamplingEfforts(family);
    const auto effort = out.value("reasoning_effort", std::string());
    if (std::find(efforts.begin(), efforts.end(), effort) == efforts.end()) out["reasoning_effort"] = efforts.front();
    return out;
}

inline Json ValidateChatSampling(const Json &value)
{
    if (!value.is_object()) throw std::runtime_error("Chat defaults must be an object.");
    auto number = [&](const char *key, double low, double high, bool lowOpen = false, bool integer = false) {
        if (!value.contains(key) || value[key].is_null()) return;
        if (!value[key].is_number()) throw std::runtime_error(std::string(key) + " must be numeric.");
        const double n = value[key].get<double>();
        if ((lowOpen ? n <= low : n < low) || n > high || (integer && n != (int64_t)n))
            throw std::runtime_error(std::string("Invalid ") + key + " value.");
    };
    number("temperature", 0, 1.5);
    number("top_p", 0.05, 1);
    number("top_k", 0, 64, false, true);
    number("min_p", 0, 0.5);
    number("max_tokens", 0, 2147483647.0, true, true);
    number("seed", 0, 2147483647.0, false, true);
    number("repetition_penalty", 0, 10, true);
    number("frequency_penalty", 0, 2);
    number("presence_penalty", 0, 2);
    for (const auto *key : {"temperature", "top_p", "top_k", "min_p"})
        if (!value.contains(key) || !value[key].is_number())
            throw std::runtime_error(std::string("Enter a value for ") + key + ".");
    if (!value.contains("reasoning_effort") || !value["reasoning_effort"].is_string())
        throw std::runtime_error("Choose a reasoning level.");
    const auto family = value.value("family", std::string("glm"));
    const auto efforts = ChatSamplingEfforts(family);
    if (std::find(efforts.begin(), efforts.end(), value["reasoning_effort"].get<std::string>()) == efforts.end())
        throw std::runtime_error("That reasoning level is not available for this model family.");
    return value;
}

inline Json ApplyChatSampling(Json request, const Json &value)
{
    const auto integer = [&](const char *key, int64_t low, int64_t high) {
        const double n = value[key].get<double>();
        if (!std::isfinite(n) || n < low || n > high || n != std::trunc(n))
            throw std::runtime_error(std::string("Invalid ") + key + " value.");
        return static_cast<int64_t>(n);
    };
    if (value.contains("temperature") && value["temperature"].is_number())
    {
        const auto temperature = value["temperature"].get<double>();
        request["temperature"] = temperature;
        if (temperature > 0)
        {
            for (const auto *key : {"top_p", "min_p"})
                if (value.contains(key) && value[key].is_number()) request[key] = value[key];
            if (value.contains("top_k") && value["top_k"].is_number()) request["top_k"] = integer("top_k", 0, 64);
        }
    }
    if (value.contains("max_tokens") && value["max_tokens"].is_number())
        request["max_tokens"] = integer("max_tokens", 1, 2147483647);
    for (const auto *key : {"repetition_penalty", "frequency_penalty", "presence_penalty"})
        if (value.contains(key) && value[key].is_number()) request[key] = value[key];
    if (value.contains("seed") && value["seed"].is_number() && value["seed"].get<double>() > 0)
        request["seed"] = integer("seed", 1, 2147483647);
    return request;
}

inline Json SharedChatDefaults(const Json &value)
{
    Json defaults = Json::object();
    for (const auto *key : {"reasoning_effort", "temperature"})
        if (value.contains(key) && !value[key].is_null()) defaults[key] = value[key];
    if (value.value("temperature", 0.0) > 0)
        for (const auto *key : {"top_p", "top_k", "min_p"})
            if (value.contains(key) && !value[key].is_null()) defaults[key] = value[key];
    for (const auto *key : {"seed", "max_tokens", "repetition_penalty", "frequency_penalty", "presence_penalty"})
        if (value.contains(key) && !value[key].is_null()) defaults[key] = value[key];
    return defaults;
}

inline std::string ChatSharedDefaultsSummary(const Json &settings)
{
    if (!settings.is_object() || !settings.value("shared", false))
        return "API clients get them for anything they don't set themselves.";
    const auto defaults = settings.value("defaults", Json::object());
    if (!defaults.is_object() || defaults.empty())
        return "API clients get them for anything they don't set themselves.";
    const std::pair<const char *, const char *> labels[] = {
        {"reasoning_effort", "thinking"}, {"temperature", "temperature"}, {"top_p", "top-p"},
        {"top_k", "top-k"}, {"min_p", "min-p"}, {"seed", "seed"}, {"max_tokens", "max tokens"},
        {"repetition_penalty", "repetition penalty"}, {"frequency_penalty", "frequency penalty"},
        {"presence_penalty", "presence penalty"}};
    std::string summary = "API clients get now: ";
    bool first = true;
    for (auto it = defaults.begin(); it != defaults.end(); ++it)
    {
        if (!first) summary += ", ";
        first = false;
        std::string label = it.key();
        for (const auto &[key, name] : labels) if (it.key() == key) { label = name; break; }
        summary += label + " " + (it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    }
    return summary;
}

inline Json SaveChatSampling(Json saved, const std::string &model, const Json &value)
{
    if (!saved.is_object()) saved = Json::object();
    const auto family = ChatSamplingFamily(model);
    Json kept = value;
    kept.erase("family");
    for (auto it = kept.begin(); it != kept.end();)
        if (it.value().is_null()) it = kept.erase(it);
        else ++it;
    saved[family] = std::move(kept);
    return saved;
}

inline Json SaveChatReasoningEffort(Json saved, const std::string &model, const std::string &effort)
{
    if (!saved.is_object()) saved = Json::object();
    const auto family = ChatSamplingFamily(model);
    if (!saved.contains(family) || !saved[family].is_object()) saved[family] = Json::object();
    saved[family]["reasoning_effort"] = effort;
    return saved;
}

inline bool NativeSamplingAutosaveNeeded(bool loading, bool changed, bool valid) noexcept
{
    return !loading && changed && valid;
}

inline bool NativeSharedDefaultsSyncReady(bool pending, bool sharing, bool busy) noexcept
{
    return pending && sharing && !busy;
}

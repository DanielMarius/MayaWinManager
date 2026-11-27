#pragma once
#include "http.h"
#include "settings_flow.h"
#include "savings_price.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

inline std::wstring ModelOperationErrorText(bool loading, const std::wstring &error)
{
    return (loading ? L"Model load failed: " : L"Model unload failed: ") + error;
}
inline std::wstring ModelOperationRefreshErrorText(const std::wstring &error)
{
    return error + L" Current model state could not be refreshed.";
}

inline bool ModelContextTelemetryRequested(const Json &modelsData)
{
    return modelsData.is_object() && modelsData.contains("running") && modelsData["running"].is_string() &&
           !modelsData["running"].get<std::string>().empty();
}

inline std::string ModelContextLabel(const Json &context)
{
    if (!context.is_number()) return "–";
    const double value = context.get<double>();
    if (value <= 0) return "–";
    const auto count = static_cast<uint64_t>(value);
    if (count % 1048576 == 0) return std::to_string(count / 1048576) + "M";
    if (count % 1024 == 0) return std::to_string(count / 1024) + "K";
    std::ostringstream out;
    if (value >= 10000) out << std::fixed << std::setprecision(0) << std::round(value / 1000) << "k";
    else if (value >= 1000) out << std::fixed << std::setprecision(1) << value / 1000 << "k";
    else out << std::fixed << std::setprecision(0) << value;
    return out.str();
}

inline std::string ModelSettingsChoiceLabel(const std::string &key, const Json &option)
{
    if (key == "context" && option.is_number()) return ModelContextLabel(option);
    const auto value = option.is_string() ? option.get<std::string>() : option.dump();
    if (key == "tp" && value == "0") return "Off";
    if (key == "draft")
    {
        if (value == "mtp") return "MTP";
        if (value == "dflash2") return "DFlash2";
        if (value == "dspark") return "DSpark";
        if (value == "none" || value == "off") return "Off";
    }
    if (key == "mmap")
    {
        if (value == "auto") return "Auto";
        if (value == "on") return "From the files";
        if (value == "off") return "In RAM";
    }
    if (key == "kv" || key == "kv_cache")
    {
        if (value == "fp16") return "FP16";
        if (value == "bf16") return "BF16";
        if (value == "int8") return "INT8";
        if (value == "fp8") return "FP8";
        if (value == "fp4") return "FP4";
        if (value == "k8v4") return "K8 V4";
        if (value == "q4_0") return "Q4_0";
    }
    return value;
}

inline bool ModelSettingsEnvironmentKeyHasText(const std::string &key)
{
    const auto wide = Wide(key);
    return std::any_of(wide.begin(), wide.end(), [](wchar_t c) { return !SavingsPriceWhitespace(c); });
}

inline bool ModelSettingsNumericKey(const std::string &key);
inline bool ModelSettingsSetScalar(Json &settings, const std::string &key, const std::string &text);

inline Json ModelSettingsRequest(const std::string &file, const Json &settings, bool dryRun)
{
    if (file.empty() || !settings.is_object())
        throw std::runtime_error("Select a model and provide its settings as a JSON object.");
    Json sent = settings;
    for (auto it = sent.begin(); it != sent.end(); ++it)
    {
        // The web Models page does not normalize the native-only ram_gb field.
        if (!it.value().is_string() || it.key() == "ram_gb" || !ModelSettingsNumericKey(it.key())) continue;
        ModelSettingsSetScalar(sent, it.key(), it.value().get<std::string>());
    }
    Json env = Json::object();
    if (sent.contains("env") && sent["env"].is_object())
        for (auto it = sent["env"].begin(); it != sent["env"].end(); ++it)
            if (ModelSettingsEnvironmentKeyHasText(it.key())) env[it.key()] = it.value();
    sent["env"] = std::move(env);
    Json request{{"file", file}, {"settings", std::move(sent)}};
    if (dryRun) request["dry_run"] = true;
    return request;
}

inline std::vector<std::pair<std::string, std::string>> ModelSettingsEnvironmentRows(const Json &settings)
{
    std::vector<std::pair<std::string, std::string>> rows;
    if (!settings.is_object() || !settings.contains("env") || !settings["env"].is_object()) return rows;
    for (auto it = settings["env"].begin(); it != settings["env"].end(); ++it)
        rows.emplace_back(it.key(), it.value().is_string() ? it.value().get<std::string>() : it.value().dump());
    return rows;
}

inline Json ModelSettingsEnvironmentObject(const std::vector<std::pair<std::string, std::string>> &rows)
{
    Json env = Json::object();
    for (const auto &[key, value] : rows)
        if (ModelSettingsEnvironmentKeyHasText(key)) env[key] = value;
    return env;
}

inline std::wstring ModelSettingsContextHint(const Json &original, const Json &draft,
                                             const Json &liveContext = Json::object(), bool running = false,
                                             const std::string &family = {})
{
    constexpr const wchar_t *generic = L"How much text the model holds at once: chat, files and tool output. More context takes GPU memory from experts, so answers may get slower.";
    if (!original.is_object() || !draft.is_object() || !original.contains("context") ||
        !original["context"].is_number() || !draft.contains("context") || !draft["context"].is_number())
        return generic;
    const double next = draft["context"].get<double>();
    const double now = original["context"].get<double>();
    const bool same = next == now;
    const auto fmt = [](double value) {
        std::wostringstream out;
        out << std::fixed << std::setprecision(1) << value;
        return out.str();
    };
    const auto sizeLabel = [](double value) {
        const auto n = static_cast<unsigned long long>(std::max(0.0, value));
        if (n && n % 1048576 == 0) return std::to_wstring(n / 1048576) + L"M";
        if (n && n % 1024 == 0) return std::to_wstring(n / 1024) + L"K";
        return std::to_wstring(n);
    };
    const auto more = [&](double delta) {
        return same || delta == 0.0 ? std::wstring(L".") : L" · " + fmt(std::abs(delta)) + L" GB " +
            (delta > 0 ? L"more" : L"less") + L" than now.";
    };
    const auto number = [](const Json &value, const char *key) {
        return value.is_object() && value.contains(key) && value[key].is_number() ? value[key].get<double>() : 0.0;
    };
    constexpr const char *liveKeys[] = {"kv_streaming", "kv_resident", "parallel"};
    const bool liveSettingsMatch = std::all_of(std::begin(liveKeys), std::end(liveKeys), [&](const char *key) {
            return original.value(key, Json()) == draft.value(key, Json());
        });
    const bool live = running && liveSettingsMatch;
    const double liveContextSize = number(liveContext, "context");
    const double resident = number(liveContext, "kv_resident");
    const double kvRam = number(liveContext, "kv_ram_gb");
    const double kvGb = number(liveContext, "kv_gb");
    const double kvCtx = number(liveContext, "kv_ctx");
    std::wstring hint;
    if (live && resident > 0 && kvRam > 0 && liveContextSize > 0)
    {
        const double estimate = kvRam * next / liveContextSize;
        const double window = resident > 0 ? resident : 0;
        hint = L"≈ " + fmt(estimate) + L" GB of RAM for the context (kept pinned; " + sizeLabel(window) +
               L" positions of it also in VRAM)" + more(estimate - kvRam);
    }
    else if (live && !resident && kvGb > 0 && kvCtx > 0)
    {
        const double perToken = kvGb / kvCtx;
        const double estimate = perToken * next;
        const double expertGb = number(liveContext, "vram_gb") > 0 && number(liveContext, "vram_slots") > 0
            ? number(liveContext, "vram_gb") / number(liveContext, "vram_slots") : 0;
        hint = L"≈ " + fmt(estimate) + L" GB of GPU memory for the context";
        if (same) hint += L".";
        else
        {
            const int experts = expertGb > 0 ? static_cast<int>(std::llround((estimate - perToken * liveContextSize) / expertGb)) : 0;
            hint += experts ? L" · about " + fmt(std::abs(experts)) + (experts > 0
                ? L" fewer experts held in VRAM than now (answers a little slower)."
                : L" more experts held in VRAM than now.") : L".";
        }
    }
    else if (family == "glm")
    {
        const int parallel = std::max(1, static_cast<int>(number(draft, "parallel")));
        const double bytesPerToken = 12288.0 * parallel;
        const bool streaming = draft.contains("kv_streaming") && draft["kv_streaming"].is_boolean() &&
                               draft["kv_streaming"].get<bool>();
        const double window = streaming ? std::max(1.0, number(draft, "kv_resident") > 0
            ? number(draft, "kv_resident") : 32768.0) : 0;
        if (streaming && next <= window)
            hint = L"Up to " + sizeLabel(window) + L" the whole cache stays in VRAM.";
        else if (streaming)
        {
            const double estimate = bytesPerToken * next / 1073741824.0;
            hint = L"≈ " + fmt(estimate) + L" GB of RAM for the context (kept pinned; " + sizeLabel(window) +
                   L" positions of it also in VRAM)" + ((now > 0 && now <= window) ? L"." : more(bytesPerToken * (next - now) / 1073741824.0));
        }
        else
        {
            const double estimate = bytesPerToken * next / 1073741824.0;
            hint = L"≈ " + fmt(estimate) + L" GB of GPU memory for the context, taken from the experts" +
                   more(bytesPerToken * (next - now) / 1073741824.0);
        }
    }
    else hint = generic;

    return hint;
}

constexpr bool NativeModelSettingsRequestMayOverlapChat(bool chatActive, bool requestInFlight) noexcept
{
    return chatActive && !requestInFlight;
}

constexpr bool NativeModelSettingsAutoPreviewReady(bool open, bool valid, bool idleOrChat,
                                                    bool requestInFlight) noexcept
{
    return open && valid && idleOrChat && !requestInFlight;
}

constexpr bool NativeModelSettingsResetEnabled(bool hasSetup, bool requestInFlight) noexcept
{
    return hasSetup && !requestInFlight;
}

constexpr bool NativeModelImageToggleEnabled(bool imageAvailable, bool imagesEnabled) noexcept
{
    return imageAvailable || imagesEnabled;
}

inline bool ModelSettingsDecimalKey(const std::string &key)
{
    return key == "reserve_gb" || key == "cache_gb" || key == "dspark_conf";
}
inline bool ModelSettingsWholeKey(const std::string &key)
{
    return key == "context" || key == "parallel" || key == "reserve_mib" || key == "expert_cache" ||
           key == "prefill" || key == "kv_resident" || key == "tp" || key == "prefill_chunk" ||
           key == "cpu_threads" || key == "ram_gb";
}
inline bool ModelSettingsNumericKey(const std::string &key)
{
    return ModelSettingsWholeKey(key) || ModelSettingsDecimalKey(key);
}
inline int ModelSettingsInteger(const std::string &text)
{
    const auto wide = Wide(text);
    size_t begin = 0, end = wide.size();
    while (begin < end && SavingsPriceWhitespace(wide[begin])) ++begin;
    while (end > begin && SavingsPriceWhitespace(wide[end - 1])) --end;
    return NativeSettingsInteger(wide.substr(begin, end - begin));
}

inline bool ModelSettingsSetGpuIndexes(Json &settings, const std::string &key, std::vector<int> indexes)
{
    if (!settings.is_object() || key != "gpus" || !settings.contains(key) || !settings[key].is_array() ||
        indexes.empty() || std::any_of(indexes.begin(), indexes.end(), [](int index) { return index < 0; }))
        return false;
    std::sort(indexes.begin(), indexes.end());
    if (std::adjacent_find(indexes.begin(), indexes.end()) != indexes.end()) return false;
    settings[key] = indexes;
    return true;
}

inline bool ModelSettingsSetScalar(Json &settings, const std::string &key, const std::string &text)
{
    if (!settings.is_object() || !settings.contains(key))
        return false;
    auto &value = settings[key];
    if (value.is_boolean())
    {
        if (text != "true" && text != "false") return false;
        value = text == "true";
        return true;
    }
    const bool numericString = value.is_string() && ModelSettingsNumericKey(key);
    if (value.is_string() && !numericString)
    {
        value = text;
        return true;
    }
    if (value.is_number() || value.is_null() || numericString)
    {
        if (text.empty())
        {
            value = nullptr;
            return true;
        }
        if (ModelSettingsWholeKey(key))
        {
            try { value = ModelSettingsInteger(text); return true; }
            catch (...) { return false; }
        }
        if (ModelSettingsDecimalKey(key))
        {
            try { value = SavingsPriceNumber(text, "Model setting", 0.0, std::numeric_limits<double>::max()); return true; }
            catch (...) { return false; }
        }
        const auto parsed = Json::parse(text, nullptr, false);
        if (!parsed.is_number()) return false;
        value = parsed;
        return true;
    }
    return false;
}

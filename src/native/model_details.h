#pragma once
#include "http.h"
#include "chat_menu_policy.h"
#include "model_settings.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>

inline std::string ModelDetailsValue(const Json &object, const char *key, const std::string &fallback = "not set")
{
    if (!object.is_object() || !object.contains(key) || object[key].is_null()) return fallback;
    return object[key].is_string() ? object[key].get<std::string>() : object[key].dump();
}

inline bool ModelDetailsTruthy(const Json &value)
{
    if (value.is_null()) return false;
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number()) return value.get<double>() != 0.0;
    if (value.is_string()) return !value.get<std::string>().empty();
    return true;
}

inline std::string ModelDetailsTruthyValue(const Json &object, const char *key, const std::string &fallback)
{
    return object.is_object() && object.contains(key) && ModelDetailsTruthy(object[key])
        ? ModelDetailsValue(object, key, fallback) : fallback;
}

inline std::string ModelDisplayName(const std::string &id)
{
    auto name = NativeChatModelButtonName(id);
    return NativeCyberFrostVersion(id).empty() ? name : name + " (CYBER-FROST)";
}

inline std::string ModelDetailsStartScript(const Json &model)
{
    const auto script = ModelDetailsTruthyValue(model, "script", "");
    return script.empty() ? "–" : "./" + script;
}

inline std::string ModelStatusLabel(const Json &model, bool loaded)
{
    if (!model.is_object()) return "unknown";
    if (model.value("running", false)) return loaded ? "running" : "unloaded";
    if (model.value("ok", true)) return "installed";
    const auto missing = ModelDetailsValue(model, "missing", "");
    return missing.rfind("its model files", 0) == 0 ? "files missing" : "incomplete";
}

inline bool ModelSettingsButtonEnabled(const Json &model)
{
    return model.is_object() && model.value("ok", false);
}

inline std::string ModelBackendLabel(const Json &model)
{
    auto backend = ModelDetailsValue(model, "backend", "unknown");
    std::transform(backend.begin(), backend.end(), backend.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return backend == "CUDA" || backend == "HIP" ? backend : ModelDetailsValue(model, "backend", "unknown");
}

inline std::string ModelCardAbout(const Json &model, const wchar_t *locale);
inline std::string ModelLastUsed(const Json &model, std::time_t now, const wchar_t *locale);
constexpr bool NativeModelListDetailsKey(UINT message, WPARAM key) noexcept
{
    return message == WM_KEYDOWN && key == VK_RETURN;
}
inline std::string ModelListLabel(const Json &model, bool loaded)
{
    if (!model.is_object()) return "Unknown model";
    std::string out = ModelDisplayName(model.value("model", std::string("Model"))) + "  " +
                      model.value("quant", std::string());
    const auto about = ModelCardAbout(model, LOCALE_NAME_USER_DEFAULT);
    if (!about.empty()) out += "  ·  " + about;
    const auto lastUsed = ModelLastUsed(model, std::time(nullptr), LOCALE_NAME_USER_DEFAULT);
    if (lastUsed == "running now") out += "  ·  running now";
    else if (lastUsed != "not yet" && lastUsed != "not set") out += "  ·  last used " + lastUsed;
    if (model.value("running", false))
        out += loaded ? "  [RUNNING]" : "  [UNLOADED]";
    else if (!model.value("ok", true))
    {
        const auto missing = ModelDetailsValue(model, "missing", "");
        out += missing.rfind("its model files", 0) == 0 ? "  [FILES MISSING]" : "  [INCOMPLETE]";
    }
    return out;
}

inline std::string ModelSetupDate(const Json &model, const wchar_t *locale = LOCALE_NAME_USER_DEFAULT)
{
    if (!model.is_object() || !model.contains("written") || !ModelDetailsTruthy(model["written"]) ||
        !model["written"].is_string())
        return "not set";
    const auto value = model["written"].get<std::string>();
    std::tm parsed{};
    std::istringstream input(value);
    input >> std::get_time(&parsed, "%Y-%m-%d %H:%M");
    if (input.fail()) return "not set";
    input >> std::ws;
    if (input.peek() != std::char_traits<char>::eof() || parsed.tm_year < 0 || parsed.tm_mon < 0 || parsed.tm_mon > 11 ||
        parsed.tm_hour < 0 || parsed.tm_hour > 23 || parsed.tm_min < 0 || parsed.tm_min > 59)
        return "not set";
    constexpr int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const int year = parsed.tm_year + 1900;
    const bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int lastDay = monthDays[parsed.tm_mon] + (parsed.tm_mon == 1 && leap ? 1 : 0);
    if (parsed.tm_mday < 1 || parsed.tm_mday > lastDay) return "not set";
    SYSTEMTIME date{};
    date.wYear = static_cast<WORD>(parsed.tm_year + 1900);
    date.wMonth = static_cast<WORD>(parsed.tm_mon + 1);
    date.wDay = static_cast<WORD>(parsed.tm_mday);
    wchar_t text[128]{};
    const int length = GetDateFormatEx(locale, 0, &date, L"MMM d", text, _countof(text), nullptr);
    return length > 0 ? Utf8(text) : "not set";
}

inline std::string ModelCardAbout(const Json &model, const wchar_t *locale)
{
    if (!model.is_object()) return {};
    std::string out = ModelDetailsValue(model, "backend", "cuda") == "hip" ? "HIP" : "CUDA";
    if (model.contains("size") && model["size"].is_number() && model["size"].get<double>() >= 1e8)
    {
        const double bytes = model["size"].get<double>();
        std::ostringstream formatted;
        try { formatted.imbue(std::locale("")); }
        catch (...) {}
        const int digits = bytes < 1e10 ? 1 : 0;
        const double scale = digits ? 10.0 : 1.0;
        formatted << std::fixed << std::setprecision(digits) << std::round(bytes / 1e9 * scale) / scale << " GB";
        out += " · " + formatted.str();
    }
    const auto setup = ModelSetupDate(model, locale);
    if (!setup.empty() && setup != "not set") out += " · set up " + setup;
    return out;
}

inline std::string ModelRelativeTime(double seconds, std::time_t now = std::time(nullptr),
                                    const wchar_t *locale = LOCALE_NAME_USER_DEFAULT)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds >= static_cast<double>(std::numeric_limits<std::time_t>::max()))
        return "not set";
    const auto usedAt = static_cast<std::time_t>(seconds);
    std::tm used{}, current{};
    if (localtime_s(&used, &usedAt) != 0 || localtime_s(&current, &now) != 0) return "not set";
    if (used.tm_year == current.tm_year && used.tm_yday == current.tm_yday)
    {
        SYSTEMTIME time{};
        time.wHour = static_cast<WORD>(used.tm_hour);
        time.wMinute = static_cast<WORD>(used.tm_min);
        time.wSecond = static_cast<WORD>(used.tm_sec);
        wchar_t text[128]{};
        const int length = GetTimeFormatEx(locale, TIME_NOSECONDS, &time, nullptr, text, _countof(text));
        return length > 0 ? Utf8(text) : "not set";
    }
    auto previous = current;
    previous.tm_mday--;
    previous.tm_hour = 12;
    previous.tm_min = previous.tm_sec = 0;
    const auto yesterdayAt = std::mktime(&previous);
    std::tm yesterday{};
    if (yesterdayAt != static_cast<std::time_t>(-1) && localtime_s(&yesterday, &yesterdayAt) == 0 &&
        used.tm_year == yesterday.tm_year && used.tm_yday == yesterday.tm_yday)
        return "Yesterday";
    SYSTEMTIME date{};
    date.wYear = static_cast<WORD>(used.tm_year + 1900);
    date.wMonth = static_cast<WORD>(used.tm_mon + 1);
    date.wDay = static_cast<WORD>(used.tm_mday);
    wchar_t text[128]{};
    const int length = GetDateFormatEx(locale, 0, &date, L"MMM d", text, _countof(text), nullptr);
    return length > 0 ? Utf8(text) : "not set";
}

inline std::string ModelLastUsed(const Json &model, std::time_t now = std::time(nullptr),
                                 const wchar_t *locale = LOCALE_NAME_USER_DEFAULT)
{
    if (model.is_object() && model.value("running", false)) return "running now";
    return model.is_object() && model.contains("used") && model["used"].is_number() &&
           model["used"].get<double>() != 0.0
        ? ModelRelativeTime(model["used"].get<double>(), now, locale) : "not yet";
}

inline int ModelSheetNextTab(int current, int direction, bool engineEnabled)
{
    if (current < 0 || current > 2) current = 0;
    for (int step = 0; step < 2; ++step)
    {
        current = (current + (direction >= 0 ? 1 : 2)) % 3;
        if (current != 1 || engineEnabled) return current;
    }
    return current;
}

inline std::string ModelSettingsSummary(const Json &settings)
{
    const auto draft = ModelDetailsValue(settings, "draft", "–");
    const auto draftLabel = draft == "none" ? "off" : draft == "mtp" ? "MTP" :
                            draft == "dflash2" ? "DFlash2" : draft == "dspark" ? "DSpark" : draft;
    std::string gpus;
    if (settings.is_object() && settings.contains("gpus") && settings["gpus"].is_array())
        for (const auto &gpu : settings["gpus"])
        {
            if (!gpus.empty()) gpus += ", ";
            gpus += gpu.is_string() ? gpu.get<std::string>() : gpu.dump();
        }
    if (gpus.empty()) gpus = "–";
    std::string out = "Context: " + ModelContextLabel(settings.is_object() ? settings.value("context", Json()) : Json()) +
        "\nDraft: " + draftLabel + "\nGPUs: " + gpus + "\nImages: " +
        (settings.is_object() && settings.value("images", false) ? "on" : "off") + "\n";
    if (settings.is_object() && settings.contains("parallel") && settings["parallel"].is_number() &&
        settings["parallel"].get<double>() > 1)
        out += "At once: " + ModelDetailsValue(settings, "parallel") + "\n";
    return out;
}

inline std::string ModelOverviewReport(const Json &model, bool loaded)
{
    if (!model.is_object()) return "Maya returned an invalid model record.";
    std::string out = ModelDisplayName(ModelDetailsValue(model, "model", "Unknown model")) + "  " +
                      ModelDetailsValue(model, "quant", "quant unknown") + "\n";
    const auto missing = ModelDetailsTruthyValue(model, "missing", "");
    if (!missing.empty() && !ModelDetailsTruthy(model.value("running", Json())))
        out += "It can't start: " + missing + ". Run START-MAYA.bat --setup to repair it.\n";
    out += "Setup: " + ModelCardAbout(model, LOCALE_NAME_USER_DEFAULT) + "\n";
    if (model.contains("settings") && model["settings"].is_object())
        out += ModelSettingsSummary(model["settings"]);
    out += "Status: " + ModelStatusLabel(model, loaded) + "\n";
    out += "Config: " + ModelDetailsValue(model, "path", ModelDetailsValue(model, "file")) + "\n";
    out += "Start script: " + ModelDetailsStartScript(model) + "\n";
    out += "Model folder: " + ModelDetailsTruthyValue(model, "folder", "–") + "\n";
    out += "Port: " + ModelDetailsTruthyValue(model, "port", "8080") + "\n";
    out += "API model name: " + ModelDetailsValue(model, "model") + "\n";
    std::string size = "unknown";
    if (model.contains("size") && model["size"].is_number())
    {
        const auto bytes = model["size"].get<uint64_t>();
        std::ostringstream formatted;
        if (bytes >= 1000000000ULL) formatted << std::fixed << std::setprecision(bytes < 10000000000ULL ? 1 : 0) << bytes / 1e9 << " GB";
        else if (bytes >= 1000000ULL) formatted << std::fixed << std::setprecision(0) << bytes / 1e6 << " MB";
        else formatted << bytes << " bytes";
        size = formatted.str();
    }
    out += "Model files size: " + size + "\n";
    out += "Last used: " + ModelLastUsed(model) + "\n";
    return out;
}

inline std::string ModelEngineReport(const Json &model)
{
    if (!model.is_object() || !model.contains("settings") || !model["settings"].is_object())
        return "Maya did not provide editable engine settings for this model.";
    return ModelSettingsSummary(model["settings"]) + "\nEngine settings\n" + model["settings"].dump(2);
}

inline std::string ModelDetailsReport(const Json &model, bool loaded)
{
    auto out = ModelOverviewReport(model, loaded);
    if (model.is_object() && model.contains("settings") && model["settings"].is_object())
        out += "\nEngine settings\n" + model["settings"].dump(2);
    return out;
}

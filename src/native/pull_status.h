#pragma once
#include "http.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

inline std::locale PullUserLocale() noexcept
{
    try { return std::locale(""); }
    catch (...) { return std::locale::classic(); }
}

inline std::string PullFormatNumber(double value, int precision)
{
    std::ostringstream out;
    out.imbue(PullUserLocale());
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

inline std::string PullFormatBytes(int64_t bytes)
{
    const auto value = static_cast<double>(std::max<int64_t>(0, bytes));
    if (value >= 1e9) return PullFormatNumber(value / 1e9, value < 1e10 ? 1 : 0) + " GB";
    if (value >= 1e6) return PullFormatNumber(value / 1e6, 0) + " MB";
    if (value >= 1e3) return PullFormatNumber(value / 1e3, 0) + " KB";
    return PullFormatNumber(value, 0) + " bytes";
}

inline int PullPercent(int64_t completed, int64_t total)
{
    if (total <= 0 || completed <= 0) return 0;
    if (completed >= total) return 100;
    return static_cast<int>((static_cast<long double>(completed) * 100) / total);
}

inline bool PullStatusActive(const Json &pull)
{
    if (!pull.is_object() || !pull.contains("state") || !pull["state"].is_string()) return false;
    const auto &state = pull["state"].get_ref<const std::string &>();
    return state == "starting" || state == "downloading";
}

inline int PullProgressBarPosition(const Json &pull)
{
    if (!pull.is_object()) return 0;
    const auto state = pull.value("state", std::string());
    const bool showDownloadRow = state == "starting" || state == "downloading" || state == "error";
    const auto percent = pull.find("percent");
    if (percent != pull.end() && percent->is_number())
    {
        const double value = percent->get<double>();
        if (std::isfinite(value))
        {
            if (value <= 0) return 0;
            if (value >= 100) return 100;
            return static_cast<int>(value);
        }
    }
    return showDownloadRow ? 4 : 0;
}

inline bool PullStatusNeedsModelRefresh(bool wasActive, const Json &pull)
{
    if (!wasActive || !pull.is_object()) return false;
    const auto state = pull.find("state");
    return state != pull.end() && state->is_string() && state->get<std::string>() == "success";
}
inline std::string PullCancelErrorText(const std::string &error)
{
    return "Download was not cancelled: " + error;
}
inline std::string PullStatusText(const Json &pull)
{
    if (!pull.is_object()) return "No model download is active.";
    const auto stringValue = [&](const char *key, const std::string &fallback = {}) {
        const auto it = pull.find(key);
        return it != pull.end() && it->is_string() ? it->get<std::string>() : fallback;
    };
    const auto intValue = [&](const char *key) {
        const auto it = pull.find(key);
        return it != pull.end() && it->is_number_integer() ? it->get<int64_t>() : int64_t(0);
    };
    const auto model = stringValue("model", "model");
    const auto state = stringValue("state");
    if (state == "error")
        return "Download failed: " + stringValue("error", "Maya setup failed.");
    if (state == "cancelled")
        return "Download cancelled. Partial files are retained for resume.";
    if (state == "success")
        return "Download complete: " + model;
    if (state == "starting")
    {
        const auto message = stringValue("message");
        return message.empty() ? "The setup is starting…" : message;
    }
    if (state != "downloading")
        return "Download state unavailable.";

    std::ostringstream out;
    out << "Downloading " << model;
    const auto total = intValue("total");
    const auto done = intValue("completed");
    if (total > 0)
    {
        const auto percent = pull.value("percent", Json());
        const auto shownPercent = percent.is_number() && std::isfinite(percent.get<double>())
                                      ? std::clamp<int>(static_cast<int>(percent.get<double>()), 0, 100)
                                      : PullPercent(done, total);
        out << " · " << PullFormatBytes(done) << " of " << PullFormatBytes(total) << " (" << shownPercent << "%)";
    }
    const auto speed = pull.contains("bytes_per_s") && pull["bytes_per_s"].is_number()
                           ? pull["bytes_per_s"].get<double>()
                           : 0.0;
    if (speed > 0)
        out << " · " << PullFormatNumber(speed / 1e6, 0) << " MB/s";
    if (total <= 0) {
        const auto message = stringValue("message");
        if (!message.empty())
            out << " · " << message;
    }
    return out.str();
}

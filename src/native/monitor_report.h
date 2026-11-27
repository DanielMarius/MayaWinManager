#pragma once
#include "http.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

inline int64_t MonitorInteger(const Json &object, const char *key, int64_t fallback = 0)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_number_integer())
        return fallback;
    if (object[key].is_number_unsigned())
    {
        const auto value = object[key].get<uint64_t>();
        return value <= (uint64_t)std::numeric_limits<int64_t>::max() ? (int64_t)value : fallback;
    }
    return object[key].get<int64_t>();
}

inline bool MonitorAllRequestsIncomplete(const Json &metrics, bool showAll)
{
    if (!showAll || !metrics.is_object()) return false;
    const auto requests = metrics.value("requests", Json::array());
    const auto kept = MonitorInteger(metrics, "requests_kept", requests.is_array() ? (int64_t)requests.size() : 0);
    return requests.is_array() && kept > 0 && requests.size() < (uint64_t)kept;
}

inline bool MonitorReportRefreshDue(uint64_t nowMs, uint64_t lastUpdatedMs, bool force) noexcept
{
    return force || lastUpdatedMs == 0 ||
           (nowMs >= lastUpdatedMs && nowMs - lastUpdatedMs >= 3000);
}

inline bool MonitorRequestsQueryAll(bool monitorVisible, bool showAll) noexcept
{
    return monitorVisible && showAll;
}

struct MonitorChartPoint { int x, y; };

inline std::vector<MonitorChartPoint> MonitorSparklinePoints(const Json &series, int width, int height)
{
    if (!series.is_array() || series.size() < 2 || width < 2 || height < 6)
        return {};
    const size_t begin = series.size() > 60 ? series.size() - 60 : 0;
    const size_t count = series.size() - begin;
    double top = 0;
    for (size_t i = begin; i < series.size(); ++i)
        if (series[i].is_number())
            top = std::max(top, series[i].get<double>());
    if (!(top > 0) || !std::isfinite(top))
        return {};
    std::vector<MonitorChartPoint> points;
    points.reserve(count);
    for (size_t i = begin; i < series.size(); ++i)
    {
        const double value = series[i].is_number() ? std::max(0.0, series[i].get<double>()) : 0.0;
        const int x = (int)((i - begin) * (size_t)(width - 1) / (count - 1));
        const int y = height - 2 - (int)std::lround((value / top) * (height - 6));
        points.push_back({x, y});
    }
    return points;
}

inline std::vector<MonitorChartPoint> MonitorSparklineAreaPoints(const std::vector<MonitorChartPoint> &points, int height)
{
    if (points.size() < 2 || height < 1)
        return {};
    auto area = points;
    area.reserve(points.size() + 2);
    area.push_back({points.back().x, height - 1});
    area.push_back({points.front().x, height - 1});
    return area;
}

inline std::wstring MonitorText(const Json &object, const char *key, const wchar_t *fallback)
{
    if (!object.is_object() || !object.contains(key) || !object[key].is_string())
        return fallback;
    const auto value = object[key].get<std::string>();
    return std::wstring(value.begin(), value.end());
}

inline std::wstring MonitorMetric(const Json &object, const char *key, int precision = 1)
{
    if (!object.is_object() || !object.contains(key) || object[key].is_null() || !object[key].is_number())
        return L"—";
    std::wostringstream value;
    value << std::fixed << std::setprecision(precision) << object[key].get<double>();
    return value.str();
}

inline std::wstring MonitorRequestApiName(const std::string &api)
{
    const std::string name = api == "web" ? "Chat" : api == "openai" ? "OpenAI" :
                             api == "anthropic" ? "Anthropic" : api;
    return std::wstring(name.begin(), name.end());
}

inline std::array<std::wstring, 10> MonitorRequestColumns(const Json &request)
{
    std::array<std::wstring, 10> cells{L"—", L"—", L"—", L"—", L"—", L"—", L"—", L"—", L"—", L"—"};
    if (!request.is_object()) return cells;
    std::wstring at = L"—";
    const auto timestamp = request.value("time", Json());
    if (timestamp.is_number())
    {
        const std::time_t raw = (std::time_t)timestamp.get<double>();
        std::tm local{};
        if (localtime_s(&local, &raw) == 0)
        {
            std::wostringstream time;
            time << std::put_time(&local, L"%H:%M:%S");
            at = time.str();
        }
    }
    const auto finish = request.value("finish", std::string());
    const wchar_t *status = finish == "stop" ? L"Done" : finish == "length" ? L"Max tokens" :
                           finish == "cancel" ? L"Stopped" : finish == "disconnect" ? L"Closed" :
                           finish == "error" ? L"Error" : nullptr;
    const std::wstring statusText = status ? status : finish.empty() ? L"—" : std::wstring(finish.begin(), finish.end());
    const auto api = request.value("api", std::string());
    const auto reused = MonitorInteger(request, "reused");
    const auto prefill = request.value("prefill_tok_s", Json());
    std::wstring prefillText = L"—";
    if (prefill.is_number()) prefillText = MonitorMetric(request, "prefill_tok_s", 0);
    else
    {
        const auto prompt = request.value("prompt_tokens", Json());
        const auto ms = request.value("prompt_ms", Json());
        if (prompt.is_number() && prompt.get<double>() > reused && ms.is_number() && ms.get<double>() > 0.0)
        {
            const double rate = (prompt.get<double>() - reused) / (ms.get<double>() / 1000.0);
            std::wostringstream value;
            value << std::fixed << std::setprecision(0) << rate;
            prefillText = value.str();
        }
    }
    cells[0] = at;
    cells[1] = statusText;
    if (!api.empty()) cells[2] = MonitorRequestApiName(api);
    cells[3] = MonitorMetric(request, "prompt_tokens", 0);
    cells[4] = MonitorMetric(request, "reused", 0);
    cells[5] = prefillText;
    cells[6] = MonitorMetric(request, "output_tokens", 0);
    cells[7] = MonitorMetric(request, "decode_tok_s");
    const auto hit = request.value("hit_rate", Json());
    if (hit.is_number())
    {
        std::wostringstream value;
        value << std::fixed << std::setprecision(1) << hit.get<double>() * 100.0 << L'%';
        cells[8] = value.str();
    }
    cells[9] = MonitorMetric(request, "duration_s");
    return cells;
}

inline std::wstring MonitorRequestText(const Json &request)
{
    const auto cells = MonitorRequestColumns(request);
    const auto api = request.is_object() ? request.value("api", std::string()) : std::string();
    std::wostringstream out;
    out << cells[0] << L" · " << cells[1] << L" · " << MonitorRequestApiName(api)
        << L" · prompt " << cells[3]
        << L" · reused " << cells[4] << L" · prefill " << cells[5] << L" tok/s"
        << L" · output " << cells[6]
        << L" · decode " << cells[7] << L" tok/s · VRAM hit " << cells[8]
        << L" · took " << cells[9] << L" s";
    if (request.contains("loop_guard") && request["loop_guard"].is_array() && !request["loop_guard"].empty())
    {
        out << L" · loop guard: ";
        for (size_t i = 0; i < request["loop_guard"].size(); ++i)
        {
            if (i) out << L", ";
            if (request["loop_guard"][i].is_string())
            {
                const auto note = request["loop_guard"][i].get<std::string>();
                out << std::wstring(note.begin(), note.end());
            }
        }
    }
    return out.str();
}

inline std::wstring MonitorContextSizeLabel(const Json &value);
inline Json MonitorTierNow(const Json &metrics);

inline Json MonitorDecodeSpeed(const Json &metrics)
{
    const auto liveValue = metrics.value("live", Json::object());
    const auto live = liveValue.is_object() ? liveValue : Json::object();
    if (MonitorInteger(live, "active") > 1)
    {
        const auto historyValue = metrics.value("history", Json::object());
        const auto history = historyValue.is_object() ? historyValue : Json::object();
        const auto series = history.value("tok_s", Json::array());
        if (series.is_array() && !series.empty()) return series.back();
        return Json();
    }
    if (MonitorText(live, "state", L"") == L"generating")
        return live.value("tok_s", Json());
    const auto requestsValue = metrics.value("requests", Json::array());
    const auto requests = requestsValue.is_array() ? requestsValue : Json::array();
    const Json last = !requests.empty() && requests[0].is_object() ? requests[0] : Json::object();
    return last.value("decode_tok_s", Json());
}

inline std::wstring MonitorHeroValue(const Json &metrics, const Json &health, const char *which)
{
    const auto live = metrics.value("live", Json::object());
    const auto requests = metrics.value("requests", Json::array());
    const Json last = requests.is_array() && !requests.empty() && requests[0].is_object() ? requests[0] : Json::object();
    Json value;
    int precision = 0;
    if (std::string(which) == "decode")
    {
        precision = 1;
        value = MonitorDecodeSpeed(metrics);
    }
    else if (std::string(which) == "prefill")
    {
        if (live.value("state", std::string()) != "idle") value = live.value("prefill_tok_s_mean", Json());
        else
        {
            value = last.value("prefill_tok_s", Json());
            if (!value.is_number())
            {
                const auto prompt = last.value("prompt_tokens", Json());
                const auto reused = MonitorInteger(last, "reused");
                const auto ms = last.value("prompt_ms", Json());
                if (prompt.is_number() && ms.is_number() && ms.get<double>() > 0.0)
                    value = std::max(0.0, prompt.get<double>() - reused) / (ms.get<double>() / 1000.0);
            }
        }
    }
    else if (std::string(which) == "context")
    {
        value = health.value("max_context", Json());
        if (!value.is_number()) value = metrics.value("engine", Json::object()).value("max_context", Json());
        return MonitorContextSizeLabel(value);
    }
    if (!value.is_number() || !std::isfinite(value.get<double>())) return L"—";
    std::wostringstream out;
    out << std::fixed << std::setprecision(precision) << value.get<double>();
    return out.str();
}

inline std::wstring MonitorTimePerTokenValue(const Json &metrics)
{
    const auto liveValue = metrics.value("live", Json::object());
    const auto live = liveValue.is_object() ? liveValue : Json::object();
    const auto tiers = MonitorTierNow(metrics);
    const auto current = tiers.value("ms_tok", Json());
    if (MonitorText(live, "state", L"") == L"generating" && current.is_number() &&
        std::isfinite(current.get<double>()) && current.get<double>() != 0.0)
        return MonitorMetric(tiers, "ms_tok");

    const auto speed = MonitorDecodeSpeed(metrics);
    if (!speed.is_number() || !std::isfinite(speed.get<double>()) || speed.get<double>() <= 0.0)
        return L"—";
    std::wostringstream value;
    value << std::fixed << std::setprecision(1) << 1000.0 / speed.get<double>();
    return value.str();
}

inline bool MonitorVramHitTierFresh(const Json &metrics, double nowSeconds = (double)std::time(nullptr))
{
    const auto tiers = MonitorTierNow(metrics);
    const auto time = tiers.value("time", Json());
    const auto hit = tiers.value("vram_hit", Json());
    return time.is_number() && std::isfinite(time.get<double>()) && time.get<double>() != 0.0 &&
           nowSeconds - time.get<double>() <= 30.0 && hit.is_number() && std::isfinite(hit.get<double>());
}

inline std::wstring MonitorVramHitValue(const Json &metrics, double nowSeconds = (double)std::time(nullptr))
{
    const auto tiers = MonitorTierNow(metrics);
    Json hit = MonitorVramHitTierFresh(metrics, nowSeconds) ? tiers.value("vram_hit", Json()) : Json();
    if (!hit.is_number())
    {
        const auto requestsValue = metrics.value("requests", Json::array());
        const auto requests = requestsValue.is_array() ? requestsValue : Json::array();
        const Json last = !requests.empty() && requests[0].is_object() ? requests[0] : Json::object();
        hit = last.value("hit_rate", Json());
    }
    if (!hit.is_number() || !std::isfinite(hit.get<double>())) return L"—";
    std::wostringstream value;
    value << std::fixed << std::setprecision(1) << hit.get<double>() * 100.0 << L"%";
    return value.str();
}

struct MonitorPrefillPeakState
{
    bool reading = false, hasRequestCount = false, hasPeak = false;
    int64_t requestCount = -1;
    double peak = 0.0;
};

struct MonitorPrefillPeak
{
    bool visible = false;
    double rate = 0.0;
    std::wstring label;
};

// Maya does not retain a per-request prefill peak, so track the values seen by the Monitor.
inline MonitorPrefillPeak UpdateMonitorPrefillPeak(const Json &metrics, MonitorPrefillPeakState &state)
{
    const auto live = metrics.value("live", Json::object());
    const auto totals = metrics.value("totals", Json::object());
    const auto count = MonitorInteger(totals, "requests", -1);
    const auto stateValue = live.value("state", Json());
    const auto liveState = stateValue.is_string() ? stateValue.get<std::string>() : std::string();
    if (liveState == "reading")
    {
        if (!state.reading)
        {
            state.hasPeak = false;
            state.hasRequestCount = count >= 0;
            state.requestCount = count;
        }
        state.reading = true;
        const auto rate = live.value("prefill_tok_s_mean", Json());
        if (rate.is_number() && std::isfinite(rate.get<double>()) && rate.get<double>() > 0.0)
        {
            state.peak = state.hasPeak ? std::max(state.peak, rate.get<double>()) : rate.get<double>();
            state.hasPeak = true;
        }
        return {state.hasPeak, state.peak, L"reading now"};
    }

    state.reading = false;
    if (!state.hasPeak || !state.hasRequestCount || count < 0) return {};
    if (liveState != "idle" && count == state.requestCount)
        return {true, state.peak, L"peak this request"};
    if (liveState == "idle" && state.requestCount < std::numeric_limits<int64_t>::max() &&
        count == state.requestCount + 1)
        return {true, state.peak, L"peak last request"};
    return {};
}

inline std::wstring MonitorPrefillPeakValue(const MonitorPrefillPeak &peak)
{
    if (!peak.visible) return L"—";
    std::wostringstream out;
    out << std::fixed << std::setprecision(0) << peak.rate;
    return out.str();
}

inline std::wstring MonitorPeakDecodeValue(const Json &metrics)
{
    const auto peak = metrics.value("totals", Json::object()).value("peak_decode", Json());
    if (!peak.is_object()) return L"—";
    const auto speed = peak.value("tok_s", Json());
    if (!speed.is_number() || !std::isfinite(speed.get<double>())) return L"—";
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << speed.get<double>();
    return out.str();
}

inline std::wstring MonitorPeakDecodeDetails(const Json &peak)
{
    if (!peak.is_object()) return L"—";
    const auto speed = peak.value("tok_s", Json());
    if (!speed.is_number() || !std::isfinite(speed.get<double>())) return L"—";
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << speed.get<double>() << L" tok/s";
    const auto stamp = peak.value("time", Json());
    if (stamp.is_number())
    {
        const std::time_t raw = (std::time_t)stamp.get<double>();
        std::tm local{};
        if (localtime_s(&local, &raw) == 0)
            out << L" at " << std::put_time(&local, L"%a %H:%M");
    }
    const auto tokens = MonitorInteger(peak, "tokens", -1);
    if (tokens >= 0) out << L", " << tokens << L" tokens into the answer";
    return out.str();
}

struct MonitorContextUsage
{
    int64_t maximum = 0, prompt = 0, reused = 0, fresh = 0, written = 0, used = 0, window = 0;
    bool live = false, reusedReported = false;
};

inline MonitorContextUsage MonitorContextUsageValues(const Json &metrics, const Json &health = Json::object())
{
    MonitorContextUsage result;
    const auto engine = metrics.value("engine", Json::object());
    result.maximum = MonitorInteger(engine, "max_context");
    if (result.maximum <= 0) result.maximum = MonitorInteger(health, "max_context");
    result.window = MonitorInteger(engine, "kv_resident");
    if (result.window <= 0 || result.window >= result.maximum) result.window = 0;

    const auto liveValue = metrics.value("live", Json::object());
    const auto live = liveValue.is_object() ? liveValue : Json::object();
    const auto stateValue = live.value("state", Json());
    const auto state = stateValue.is_string() ? stateValue.get<std::string>() : std::string();
    result.live = state == "reading" || state == "generating";
    Json request = live;
    if (!result.live)
    {
        const auto requests = metrics.value("requests", Json::array());
        request = requests.is_array() && !requests.empty() && requests[0].is_object()
            ? requests[0] : Json::object();
    }
    result.prompt = std::max<int64_t>(0, MonitorInteger(request, "prompt_tokens"));
    const auto reuseKey = result.live ? "prompt_reused" : "reused";
    result.reusedReported = request.contains(reuseKey) && request[reuseKey].is_number();
    result.reused = std::clamp<int64_t>(MonitorInteger(request, reuseKey), 0, result.prompt);
    result.fresh = result.prompt - result.reused;
    result.written = std::max<int64_t>(0, MonitorInteger(request, result.live ? "generated" : "output_tokens"));
    const int64_t total = result.written > std::numeric_limits<int64_t>::max() - result.prompt
        ? std::numeric_limits<int64_t>::max() : result.prompt + result.written;
    result.used = result.maximum > 0 ? std::min(result.maximum, total) : total;
    return result;
}

enum class MonitorRequestActivity { idle, reading, generating };

inline MonitorRequestActivity MonitorRequestActivityFor(const Json &metrics)
{
    const auto live = metrics.value("live", Json::object());
    const auto state = live.is_object() ? MonitorText(live, "state", L"") : L"";
    return state == L"reading" ? MonitorRequestActivity::reading
         : state == L"generating" ? MonitorRequestActivity::generating
                                   : MonitorRequestActivity::idle;
}

inline int MonitorRequestProgressPercent(const Json &metrics, MonitorRequestActivity activity)
{
    const auto live = metrics.value("live", Json::object());
    if (!live.is_object()) return -1;
    const char *doneKey = activity == MonitorRequestActivity::reading ? "prompt_read" : "generated";
    const char *totalKey = activity == MonitorRequestActivity::reading ? "prompt_total" : "max_tokens";
    if (activity == MonitorRequestActivity::idle) return -1;
    const auto total = MonitorInteger(live, totalKey);
    if (total <= 0) return -1;
    const auto done = MonitorInteger(live, doneKey, -1);
    if (done < 0) return -1;
    return (int)std::lround(std::clamp(100.0 * (double)done / (double)total, 0.0, 100.0));
}

enum class MonitorGpuTemperatureTone { normal, warm, hot };

inline MonitorGpuTemperatureTone MonitorGpuTemperatureLevel(const Json &reading)
{
    if (!reading.is_object() || !reading.contains("temp") || !reading["temp"].is_number() ||
        !std::isfinite(reading["temp"].get<double>()))
        return MonitorGpuTemperatureTone::normal;
    const double temp = reading["temp"].get<double>();
    return temp >= 87.0 ? MonitorGpuTemperatureTone::hot
         : temp >= 80.0 ? MonitorGpuTemperatureTone::warm
                        : MonitorGpuTemperatureTone::normal;
}

inline std::wstring MonitorContextSizeLabel(const Json &value)
{
    if (!value.is_number() || !std::isfinite(value.get<double>()) || value.get<double>() <= 0.0)
        return L"—";
    const auto tokens = (uint64_t)value.get<double>();
    if (tokens % 1048576 == 0) return std::to_wstring(tokens / 1048576) + L"M";
    if (tokens % 1024 == 0) return std::to_wstring(tokens / 1024) + L"K";
    if (tokens >= 10000) return std::to_wstring(tokens / 1000) + L"k";
    if (tokens >= 1000)
    {
        std::wostringstream out;
        out << std::fixed << std::setprecision(1) << tokens / 1000.0 << L"k";
        return out.str();
    }
    return std::to_wstring(tokens);
}

enum class MonitorBannerTone { info, danger, warning };

struct MonitorBanner
{
    MonitorBannerTone tone;
    std::wstring text;
    bool reportTag = true;
};

inline std::vector<MonitorBanner> MonitorBanners(const Json &metrics)
{
    std::vector<MonitorBanner> warnings;
    const auto reload = metrics.value("reload", Json::object());
    const auto reloadState = reload.is_object() ? reload.value("state", std::string()) : std::string();
    if (reloadState == "waiting" || reloadState == "running")
    {
        const auto target = reload.value("to", Json());
        std::wstring message = L"Reloading the model with ";
        message += MonitorContextSizeLabel(target) + L" context";
        if (reloadState == "waiting")
            message += L", after the current answer finishes";
        message += L". Requests wait until it is back.";
        warnings.push_back({MonitorBannerTone::info, std::move(message)});
    }
    else if (reloadState == "failed" && reload.value("ended", Json()).is_number() &&
             std::time(nullptr) - reload["ended"].get<long long>() < 600)
        warnings.push_back({MonitorBannerTone::danger,
            L"The context change failed: " + MonitorText(reload, "error", L"the engine did not start") +
            L". It is running again with " + MonitorContextSizeLabel(reload.value("context", Json())) + L" context."});

    const auto engine = metrics.value("engine", Json::object());
    const auto capped = engine.is_object() ? engine.value("gpu_commit_capped_gb", Json()) : Json();
    if (capped.is_number() && capped.get<double>() > 0.0)
        warnings.push_back({MonitorBannerTone::warning,
            MonitorMetric(engine, "gpu_commit_capped_gb") +
            L" GB of experts are staying off the GPU because the Windows page file is too small. Suggested page file: " +
            MonitorMetric(engine, "pagefile_hint_gb") + L" GB; restart Maya after changing it."});

    const auto hardware = metrics.value("hardware", Json::object());
    const bool hasGpuRows = hardware.is_object() && hardware.contains("gpus");
    const auto gpus = hasGpuRows ? hardware["gpus"] : Json();
    if (gpus.is_array())
    {
        for (const auto &gpu : gpus)
            if (gpu.is_object() && gpu.value("temp", Json()).is_number() && gpu["temp"].get<double>() >= 87.0)
            {
                std::wstring label = L"GPU";
                if (gpu.contains("index") && gpu["index"].is_number())
                    label += L" " + std::to_wstring(gpu["index"].get<int>());
                warnings.push_back({MonitorBannerTone::warning,
                    label + L" is at " + MonitorMetric(gpu, "temp", 0) +
                    L" °C and may slow down to cool itself.", false});
            }
    }
    // JavaScript treats [] as truthy, so the aggregate fallback applies only when gpus is absent or null.
    else if (hardware.is_object() && (!hasGpuRows || gpus.is_null()) && hardware.value("gpu_temp", Json()).is_number() &&
             hardware["gpu_temp"].get<double>() >= 87.0)
        warnings.push_back({MonitorBannerTone::warning,
            L"The GPU is at " + MonitorMetric(hardware, "gpu_temp", 0) +
            L" °C and may slow down to cool itself.", false});

    return warnings;
}

inline std::wstring MonitorWarnings(const Json &metrics)
{
    const auto warnings = MonitorBanners(metrics);
    if (warnings.empty())
        return L"";
    std::wostringstream out;
    out << L"MONITOR ALERTS\r\n";
    for (const auto &warning : warnings)
    {
        if (warning.reportTag)
            out << (warning.tone == MonitorBannerTone::info ? L"INFO: "
                 : warning.tone == MonitorBannerTone::danger ? L"ERROR: " : L"WARNING: ");
        out << warning.text << L"\r\n";
    }
    out << L"\r\n";
    return out.str();
}

inline std::wstring MonitorGigabytes(const Json &bytes)
{
    if (!bytes.is_number())
        return L"—";
    std::wostringstream value;
    value << std::fixed << std::setprecision(1) << bytes.get<double>() / (1024.0 * 1024.0 * 1024.0) << L" GB";
    return value.str();
}

inline std::wstring MonitorDataRate(const Json &megabytes)
{
    if (!megabytes.is_number() || !std::isfinite(megabytes.get<double>())) return L"—";
    const double rate = megabytes.get<double>();
    const bool gigabytes = rate >= 1000.0;
    std::wostringstream out;
    out << std::fixed << std::setprecision(gigabytes ? 1 : rate < 10.0 ? 1 : 0)
        << (gigabytes ? rate / 1024.0 : rate) << (gigabytes ? L" GB/s" : L" MB/s");
    return out.str();
}

inline std::wstring MonitorDiskReadRate(const Json &hardware)
{
    const auto value = hardware.value("disk_read_mb", Json());
    if (!value.is_number() || !std::isfinite(value.get<double>())) return L"—";
    const double rate = value.get<double>();
    const bool gigabytes = rate >= 1000.0;
    std::wostringstream out;
    out << std::fixed << std::setprecision(gigabytes ? 2 : rate < 10.0 ? 1 : 0)
        << (gigabytes ? rate / 1024.0 : rate) << (gigabytes ? L" GB/s" : L" MB/s");
    return out.str();
}

inline std::wstring MonitorDiskWriteRate(const Json &hardware)
{
    const auto value = hardware.value("disk_write_mb", Json());
    if (!value.is_number() || !std::isfinite(value.get<double>())) return {};
    std::wostringstream out;
    out << std::fixed << std::setprecision(1) << value.get<double>() << L" MB/s";
    return out.str();
}

inline std::wstring MonitorDiskWriteLabel(const Json &hardware)
{
    const auto rate = MonitorDiskWriteRate(hardware);
    return rate.empty() ? std::wstring{} : L"write " + rate;
}

inline std::wstring MonitorHardwareSubtext(const Json &metrics, const char *key, bool each,
                                            const std::wstring &gpuName)
{
    if (!metrics.is_object() || !key) return {};
    const auto rawHardware = metrics.value("hardware", Json::object());
    const auto rawStatic = metrics.value("hardware_static", Json::object());
    const auto rawEngine = metrics.value("engine", Json::object());
    const auto hardware = rawHardware.is_object() ? rawHardware : Json::object();
    const auto staticHardware = rawStatic.is_object() ? rawStatic : Json::object();
    const auto engine = rawEngine.is_object() ? rawEngine : Json::object();
    const auto readings = hardware.value("gpus", Json::array());
    const bool multi = readings.is_array() && readings.size() > 1;
    const bool individual = each && multi;
    const auto count = std::to_wstring(readings.is_array() ? readings.size() : 0);
    const std::string field(key);
    const auto gpuList = [&](auto render) {
        std::wstring result;
        for (size_t i = 0; i < readings.size(); ++i)
        {
            const auto &gpu = readings[i];
            if (!gpu.is_object()) continue;
            const auto rawIndex = gpu.value("index", Json(static_cast<unsigned int>(i)));
            const auto index = rawIndex.is_number() ? std::to_wstring(rawIndex.get<int>()) : std::to_wstring(i);
            const auto item = render(gpu, index);
            if (item.empty()) continue;
            if (!result.empty()) result += L" · ";
            result += item;
        }
        return result;
    };
    if (field == "gpu_util")
    {
        if (individual) return gpuList([](const Json &gpu, const std::wstring &index) {
            return L"GPU " + index + L" " + MonitorMetric(gpu, "util", 0) + L"%";
        });
        return multi ? L"mean of " + count + L" cards" : gpuName;
    }
    if (field == "gpu_mem_used")
    {
        if (individual) return gpuList([](const Json &gpu, const std::wstring &index) {
            return L"GPU " + index + L" " + MonitorGigabytes(gpu.value("mem_used", Json()));
        });
        const auto slots = engine.value("expert_slots", Json());
        const auto cached = slots.is_number() && slots.get<double>() > 0
            ? MonitorMetric(engine, "expert_slots", 0) + L" experts cached" : std::wstring{};
        if (!multi) return cached;
        return cached.empty() ? count + L" cards" : count + L" cards · " + cached;
    }
    if (field == "gpu_temp")
    {
        if (individual) return gpuList([](const Json &gpu, const std::wstring &index) {
            return L"GPU " + index + L" " + MonitorMetric(gpu, "temp", 0) + L"°";
        });
        return multi ? L"hottest of " + count + L" cards" : std::wstring{};
    }
    if (field == "gpu_power")
    {
        if (individual) return gpuList([](const Json &gpu, const std::wstring &index) {
            const auto power = MonitorMetric(gpu, "power", 0);
            const auto limit = MonitorMetric(gpu, "power_limit", 0);
            std::wstring text = L"GPU " + index + L" " + power;
            if (limit != L"—" && limit != L"0") text += L"/" + limit;
            return text + L" W";
        });
        const auto limit = hardware.value("gpu_power_limit", Json());
        if (!limit.is_number() || limit.get<double>() <= 0) return {};
        std::wstring text = L"of " + MonitorMetric(hardware, "gpu_power_limit", 0) + L" W limit";
        if (multi) text += L" (all " + count + L" cards)";
        return text;
    }
    if (field == "gpu_pcie_rx_mb")
    {
        if (individual) return gpuList([](const Json &gpu, const std::wstring &index) {
            auto gen = MonitorMetric(gpu, "pcie_gen_max", 0);
            if (gen == L"—") gen = MonitorMetric(gpu, "pcie_gen", 0);
            const auto width = MonitorMetric(gpu, "pcie_width", 0);
            std::wstring text = L"GPU " + index + L" Gen";
            text += gen == L"—" ? L"?" : gen;
            if (width != L"—") text += L" x" + width;
            return text + L" " + MonitorDataRate(gpu.value("pcie_rx_mb", Json()));
        });
        const auto rate = hardware.value("gpu_pcie_rx_mb", Json());
        if (!rate.is_number()) return {};
        auto result = L"to GPU " + MonitorDataRate(rate);
        const auto current = hardware.value("gpu_pcie_gen", Json());
        const auto maximum = hardware.value("gpu_pcie_gen_max", Json());
        if (current.is_number() && maximum.is_number() && current.get<double>() < maximum.get<double>())
            result += L" · idle Gen" + MonitorMetric(hardware, "gpu_pcie_gen", 0);
        return result;
    }
    if (field == "cpu")
    {
        const auto threads = staticHardware.value("threads", Json());
        if (!threads.is_number() || threads.get<double>() <= 0) return {};
        const auto cores = staticHardware.value("cores", Json());
        std::wstring text;
        if (cores.is_number() && cores.get<double>() > 0) text = MonitorMetric(staticHardware, "cores", 0) + L" cores · ";
        return text + MonitorMetric(staticHardware, "threads", 0) + L" threads";
    }
    if (field == "ram_used")
    {
        const auto pinned = engine.value("ram_gb", Json());
        if (!pinned.is_number() || pinned.get<double>() <= 0) return {};
        return MonitorMetric(engine, "ram_gb", 1) + L" GB pinned for experts";
    }
    if (field == "disk_read_mb")
    {
        const auto read = hardware.value("disk_read_mb", Json());
        if (!read.is_number())
        {
            const auto psutil = staticHardware.value("psutil", Json(false));
            return psutil.is_boolean() && psutil.get<bool>() ? L"" : L"needs psutil (setup installs it)";
        }
        return MonitorDiskWriteLabel(hardware);
    }
    return {};
}

inline std::wstring MonitorExpertFileReadLabel(const Json &metrics)
{
    const auto engine = metrics.is_object() ? metrics.value("engine", Json::object()) : Json::object();
    return engine.is_object() && engine.contains("engine_kind") && engine["engine_kind"].is_string() &&
           engine["engine_kind"] == "qwen" ? L"model file" : L"SSD";
}

inline Json MonitorTierNow(const Json &metrics)
{
    if (!metrics.is_object()) return Json::object();
    const auto tiers = metrics.value("tiers", Json::object());
    if (!tiers.is_object()) return Json::object();
    const auto now = tiers.value("now", Json::object());
    return now.is_object() ? now : Json::object();
}

inline std::wstring MonitorExpertReport(const Json &metrics)
{
    const auto now = MonitorTierNow(metrics);
    if (!now.is_object() || now.empty())
        return L"ENGINE EXPERT COUNTERS\r\nNo engine STAT reading is available.\r\n";
    const auto engine = metrics.value("engine", Json::object());
    const auto fileReadLabel = MonitorExpertFileReadLabel(metrics);
    const auto vram = now.value("vram_used", Json());
    const auto ram = now.value("ram_used", Json());
    const auto cpu = now.value("cpu_n", engine.value("cpu_experts", Json()));
    const auto all = engine.value("experts", Json());
    const bool ds4 = engine.value("engine_kind", std::string()) == "ds4";
    const bool known = vram.is_number() && (ds4 ? cpu.is_number() : all.is_number());
    const double total = !known ? 0.0 : ds4 ? vram.get<double>() + cpu.get<double>() : all.get<double>();
    const double disk = known ? std::max(0.0, total - vram.get<double>() - (ram.is_number() ? ram.get<double>() : 0.0)) : 0.0;
    std::wostringstream out;
    out << L"ENGINE EXPERT COUNTERS (" << MonitorText(engine, "engine_kind", L"engine")
        << L"; may show the last answer while idle)\r\nVRAM hit: ";
    const auto hit = now.value("vram_hit", Json());
    if (hit.is_number())
        out << std::fixed << std::setprecision(1) << 100.0 * hit.get<double>() << L"%";
    else
        out << L"—";
    out << L"   VRAM/RAM/SSD: " << MonitorMetric(now, "vram_used", 0) << L" / "
        << MonitorMetric(now, "ram_used", 0) << L" / ";
    if (known)
        out << disk;
    else
        out << L"—";
    out << L" experts\r\nVRAM/RAM/SSD cache: " << MonitorMetric(now, "vram_gb") << L" / "
        << MonitorMetric(now, "ram_gb") << L" / " << MonitorMetric(now, "disk_gb") << L" GB\r\n"
        << L"From RAM: " << MonitorMetric(now, "ram_fetch", 2) << L" /tok   From " << fileReadLabel << L": "
        << MonitorMetric(now, "disk", 2) << L" /tok   Promoted: " << MonitorMetric(now, "promo", 2) << L" /tok";
    if (now.contains("disk_mb"))
        out << (fileReadLabel == L"model file" ? L"   Model-file reads: " : L"   SSD reads: ")
            << MonitorMetric(now, "disk_mb", 2) << L" MB/tok";
    if (now.contains("engram_ms"))
    {
        out << L"   N-gram memory: " << MonitorMetric(now, "engram_ms", 2) << L" ms/tok";
        const auto rows = now.value("eg_lookup_rows", Json());
        if (rows.is_number() && rows.get<double>() > 0.0)
        {
            const auto hitsValue = now.value("eg_lookup_hits", Json());
            const double hits = hitsValue.is_number() ? hitsValue.get<double>() : 0.0;
            std::wostringstream share;
            share << std::fixed << std::setprecision(0) << (100.0 * hits / rows.get<double>());
            out << L" · " << share.str()
                << L"% from RAM cache (" << MonitorMetric(now, "eg_cache_rows", 0) << L" of "
                << MonitorMetric(now, "eg_cache_cap", 0) << L" rows held; "
                << MonitorMetric(now, "eg_lookup_rows", 0) << L" lookups, "
                << MonitorMetric(now, "eg_lookup_hits", 0) << L" RAM hits, "
                << MonitorMetric(now, "eg_lookup_reads", 0) << L" SSD reads)";
        }
    }
    if (now.contains("helper_held") || now.contains("helper"))
        out << L"   Helper: " << MonitorMetric(now, "helper_held", 0) << L" slots, "
            << MonitorMetric(now, "helper", 2) << L" selections/tok";
    out << L"\r\n";
    const auto readingTime = now.value("time", Json());
    const bool stale = readingTime.is_number() && readingTime.get<double>() > 0.0 &&
                       std::time(nullptr) - readingTime.get<double>() > 30.0;
    const auto hitRate = now.value("vram_hit", Json());
    const auto number = [](const Json &object, const char *key) {
        const auto value = object.value(key, Json());
        return value.is_number() ? value.get<double>() : 0.0;
    };
    std::wstring advice;
    if (!stale)
    {
        const auto kind = engine.value("engine_kind", std::string());
        if (ds4)
        {
            if (number(now, "disk_mb") >= 500.0)
                advice = L"Much of every token is read from the SSD: more RAM keeps more of the model's experts cached, and answers get faster.";
            else if (hitRate.is_number() && hitRate.get<double>() < 0.5)
            {
                const auto dead = now.value("helper_dead", Json());
                const bool helper = number(now, "helper_held") > 0.0 || number(now, "helper") > 0.0 ||
                                    number(engine, "helper_allocated_bytes") > 0.0;
                advice = helper && !(dead.is_boolean() && dead.get<bool>())
                    ? L"Most selections miss the primary cache; the helper handles some before CPU fallback."
                    : L"Most experts run on the CPU: more VRAM (or a second GPU) keeps more of them on the card.";
            }
        }
        else if ((kind == "mm3" || kind == "qwen") && number(now, "disk") >= 3.0 && disk > 0.0)
            advice = L"Many experts are read from the SSD for every token: more RAM, a smaller model or a shorter context makes answers faster.";
        else if (kind == "mm3" || kind == "qwen")
        {
            if (hitRate.is_number() && hitRate.get<double>() < 0.5)
                advice = L"Most experts run on the CPU: more VRAM (or a second GPU) keeps more of them on the card.";
        }
        else if (hitRate.is_number() && hitRate.get<double>() < 0.85 && number(now, "ram_fetch") >= 4.0)
            advice = L"Most misses come from RAM over PCIe: more VRAM (or a second GPU) keeps more experts on the card.";
    }
    if (!advice.empty()) out << L"Advice: " << advice << L"\r\n";
    return out.str();
}

inline Json MonitorTierShares(const Json &metrics)
{
    const auto now = MonitorTierNow(metrics);
    const auto engineValue = metrics.is_object() ? metrics.value("engine", Json::object()) : Json::object();
    const auto engine = engineValue.is_object() ? engineValue : Json::object();
    const auto kind = engine.value("engine_kind", Json());
    const bool ds4 = kind.is_string() && kind == "ds4";
    const auto number = [](const Json &object, const char *key, double fallback = 0.0) {
        const auto value = object.value(key, Json());
        return value.is_number() ? value.get<double>() : fallback;
    };
    const double vram = number(now, "vram_used"), ram = number(now, "ram_used");
    const auto cpuValue = now.value("cpu_n", Json());
    const double cpu = cpuValue.is_number() ? cpuValue.get<double>() : number(engine, "cpu_experts");
    const double counted = ds4 ? vram + cpu : number(engine, "experts");
    const double total = counted > 0.0 ? counted : std::max(1.0, vram + ram);
    const auto optional = [&](const char *key) { return now.value(key, Json()); };
    return {{"vram", vram}, {"ram", ram}, {"disk", std::max(0.0, total - vram - ram)}, {"total", total},
            {"vram_gb", optional("vram_gb")}, {"ram_gb", optional("ram_gb")}, {"disk_gb", optional("disk_gb")}};
}

inline std::vector<double> MonitorTierBarFractions(const Json &metrics)
{
    const auto shares = MonitorTierShares(metrics);
    const double vram = std::max(0.0, shares.value("vram", 0.0));
    const double ram = std::max(0.0, shares.value("ram", 0.0));
    const double disk = std::max(0.0, shares.value("disk", 0.0));
    const double total = vram + ram + disk;
    return total > 0.0 ? std::vector<double>{vram / total, ram / total, disk / total}
                       : std::vector<double>{0.0, 0.0, 0.0};
}

inline std::wstring MonitorNumberLabel(double value, int precision = 0)
{
    std::wostringstream out;
    static const std::locale userLocale = [] {
        try { return std::locale(""); } catch (...) { return std::locale::classic(); }
    }();
    out.imbue(userLocale);
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

inline std::wstring MonitorCountLabel(const Json &object, const char *key)
{
    const auto value = MonitorInteger(object, key, -1);
    return value < 0 ? L"—" : MonitorNumberLabel((double)value);
}

inline std::wstring MonitorAgeText(double seconds)
{
    if (!std::isfinite(seconds)) return {};
    seconds = std::max(0.0, seconds);
    if (seconds < 90.0) return MonitorNumberLabel(std::round(seconds)) + L" s";
    if (seconds < 5400.0) return MonitorNumberLabel(std::round(seconds / 60.0)) + L" min";
    return MonitorNumberLabel(std::floor(seconds / 3600.0)) + L" h " +
           MonitorNumberLabel(std::round(std::fmod(seconds, 3600.0) / 60.0)) + L" min";
}

inline std::wstring MonitorTierTitle(const Json &metrics)
{
    const auto now = MonitorTierNow(metrics);
    const auto engineValue = metrics.is_object() ? metrics.value("engine", Json::object()) : Json::object();
    const auto engine = engineValue.is_object() ? engineValue : Json::object();
    const auto kind = engine.value("engine_kind", Json());
    const auto positive = [](const Json &object, const char *key) {
        const auto value = object.value(key, Json());
        return value.is_number() && value.get<double>() > 0.0;
    };
    const bool helper = kind.is_string() && kind == "ds4" &&
        (positive(now, "helper_held") || positive(now, "helper") || positive(engine, "helper_allocated_bytes"));
    return helper ? L"Primary cache and RAM backing" : L"Where the experts live";
}

inline std::wstring MonitorTierLegendLabel(const Json &metrics, int tier)
{
    if (tier == 0)
        return MonitorTierTitle(metrics) == L"Primary cache and RAM backing" ? L"Primary VRAM" : L"VRAM";
    if (tier == 1)
    {
        const auto engineValue = metrics.is_object() ? metrics.value("engine", Json::object()) : Json::object();
        const auto engine = engineValue.is_object() ? engineValue : Json::object();
        const auto kind = engine.value("engine_kind", Json());
        return kind.is_string() && kind == "ds4" ? L"RAM" : L"Pinned RAM";
    }
    return tier == 2 ? L"SSD only" : std::wstring();
}

inline std::wstring MonitorTierCaption(const Json &metrics, double nowSeconds)
{
    const auto now = MonitorTierNow(metrics);
    const auto stamp = now.value("time", Json());
    if (!stamp.is_number() || stamp.get<double>() == 0.0 || !std::isfinite(nowSeconds)) return {};
    const double age = std::max(0.0, nowSeconds - stamp.get<double>());
    if (age > 30.0) return L"last reading " + MonitorAgeText(age) + L" ago, from the last answer";
    return L"live · " + MonitorNumberLabel(MonitorTierShares(metrics).value("total", 1.0)) + L" routed experts";
}

inline std::wstring MonitorGpuMemoryReport(const Json &metrics)
{
    const auto maps = metrics.value("gpu_map", Json::array());
    if (!maps.is_array() || maps.empty())
        return L"\r\nGPU MEMORY MAP\r\nNo per-GPU memory breakdown is available.\r\n";
    const auto engine = metrics.value("engine", Json::object());
    const auto hardware = metrics.value("hardware", Json::object());
    const auto readings = hardware.value("gpus", Json::array());
    const std::pair<const char *, const wchar_t *> labels[] = {
        {"weights", L"weights"}, {"kv", L"KV"}, {"act", L"activations"}, {"prompt", L"prompt"},
        {"tp", L"tensor parallel"}, {"drafter", L"drafter"}, {"pool", L"expert pool"},
        {"helper", L"helper experts"}, {"vision", L"vision"}, {"rest", L"CUDA/rest"}, {"other", L"other apps"}};
    std::wostringstream out;
    out << L"\r\nGPU MEMORY MAP\r\n";
    for (const auto &gpu : maps)
    {
        if (!gpu.is_object())
            continue;
        int index = gpu.value("dev", -1);
        if (gpu.contains("index") && gpu["index"].is_number())
            index = gpu["index"].get<int>();
        out << L"GPU " << index;
        const auto name = MonitorText(gpu, "name", L"");
        if (!name.empty())
            out << L" · " << name;
        const auto total = gpu.value("total", Json());
        const auto free = gpu.value("free", Json());
        if (total.is_number() && free.is_number())
            out << L" · " << MonitorGigabytes(Json(std::max(0.0, total.get<double>() - free.get<double>())))
                << L" used / " << MonitorGigabytes(total) << L" total";
        const auto layers = gpu.value("layers", Json());
        if (layers.is_array() && layers.size() >= 2 && layers[0].is_number() && layers[1].is_number())
        {
            const int first = layers[0].get<int>();
            const int last = layers[1].get<int>() - ((engine.value("engine_kind", std::string()) == "ds4" ||
                                                       engine.value("engine_kind", std::string()) == "mm3") ? 0 : 1);
            if (first >= 0 && last >= first)
                out << L" · " << (first == last ? L"layer " : L"layers ") << first
                    << (first == last ? L"" : L"–" + std::to_wstring(last));
        }
        out << (gpu.value("measured", std::string()) == "start" ? L" · start measurement" : L" · current reading")
            << L"\r\n";
        const auto parts = gpu.value("parts", Json::object());
        bool any = false;
        for (const auto &[key, label] : labels)
            if (parts.is_object() && parts.contains(key) && parts[key].is_number() && parts[key].get<double>() > 0)
            {
                out << L"  " << label << L": " << MonitorGigabytes(parts[key]) << L"\r\n";
                std::wstring detail;
                if (key == std::string("weights")) detail = L"attention, shared experts and other dense tensors";
                else if (key == std::string("kv"))
                {
                    const auto context = gpu.value("ctx", engine.value("max_context", Json()));
        const auto window = MonitorInteger(engine, "kv_resident");
                    if (context.is_number() && context.get<long long>() > 0)
                    {
                        const auto ctx = context.get<long long>();
                        detail = window > 0 && window < ctx
                            ? std::to_wstring(window) + L" of " + std::to_wstring(ctx) + L" positions here, the rest in RAM"
                            : std::to_wstring(ctx) + L" positions";
                    }
                    const auto kvRam = gpu.value("kv_ram", Json());
                    if (kvRam.is_number() && kvRam.get<double>() > 0)
                        detail += (detail.empty() ? L"" : L" · ") + MonitorGigabytes(kvRam) + L" pinned in RAM";
                }
                else if (key == std::string("act")) detail = L"one token's working buffers";
                else if (key == std::string("prompt")) detail = L"prompt chunks and CPU-lane working buffers";
                else if (key == std::string("tp") && gpu.value("tp_n", 0) > 1)
                    detail = L"tensor copies for a group of " + std::to_wstring(gpu.value("tp_n", 0)) + L" GPUs";
                else if (key == std::string("drafter"))
                {
                    const auto draft = gpu.value("drafter", Json());
                    const auto window = gpu.value("window", Json());
                    if (draft.is_number() && draft.get<double>() > 0) detail = L"weights " + MonitorGigabytes(draft);
                    if (window.is_number() && window.get<double>() > 0)
                        detail += (detail.empty() ? std::wstring() : std::wstring(L" · ")) + L"verify window " + MonitorGigabytes(window);
                }
                else if (key == std::string("pool"))
                {
                    const auto slots = gpu.value("slots", 0);
                    const auto borrow = gpu.value("borrow", Json());
                    if (slots > 0) detail = std::to_wstring(slots) + L" expert slots";
                    if (borrow.is_number() && borrow.get<double>() > 0)
                        detail += (detail.empty() ? std::wstring() : std::wstring(L" · ")) + L"prompts borrow up to " + MonitorGigabytes(borrow);
                }
                else if (key == std::string("helper")) detail = L"separate helper expert pool; CPU fallback available";
                else if (key == std::string("vision"))
                {
                    const auto visionOwn = gpu.value("vision_own", Json());
                    detail = gpu.value("vision_lent", false) ? L"expert-pool tail lent to the encoder while it runs"
                            : visionOwn.is_number() && visionOwn.get<double>() > 0 ? L"vision tower in the engine process"
                                                                                  : L"vision encoder process";
                }
                else if (key == std::string("rest")) detail = L"driver reserve, context and small buffers";
                else if (key == std::string("other")) detail = L"processes other than Maya";
                if (!detail.empty()) out << L"    " << detail << L"\r\n";
                any = true;
            }
        if (free.is_number())
        {
            out << L"  free: " << MonitorGigabytes(free) << L" · "
                << (free.get<double>() < 1.5 * 1024 * 1024 * 1024
                        ? L"kept free for the driver and sampler" : L"unused") << L"\r\n";
            any = true;
        }
        if (!any)
            out << L"  No segment measurements reported.\r\n";
        if (readings.is_array())
            for (const auto &reading : readings)
                if (reading.is_object() && reading.value("index", -2) == index)
                {
                    out << L"  Sensors: " << MonitorMetric(reading, "util", 0) << L"% load · "
                        << MonitorMetric(reading, "temp", 0) << L" °C · " << MonitorMetric(reading, "power")
                        << L" W · PCIe Gen" << MonitorMetric(reading, "pcie_gen_max", 0) << L" x"
                        << MonitorMetric(reading, "pcie_width", 0);
                    if (reading.contains("pcie_rx_mb") && reading["pcie_rx_mb"].is_number())
                        out << L" · " << MonitorDataRate(reading["pcie_rx_mb"]);
                    out << L"\r\n";
                    break;
                }
    }
    return out.str();
}

inline std::vector<Json> MonitorGpuRows(const Json &metrics)
{
    const auto map = metrics.value("gpu_map", Json::array());
    std::vector<Json> rows;
    if (map.is_array() && !map.empty())
    {
        for (const auto &gpu : map)
            if (gpu.is_object())
                rows.push_back(gpu);
        return rows;
    }
    const auto hardware = metrics.value("hardware", Json::object());
    const auto readings = hardware.value("gpus", Json::array());
    if (!readings.is_array() || readings.size() < 5)
        return rows;
    std::vector<std::string> names;
    const auto staticHardware = metrics.value("hardware_static", Json::object());
    const auto name = staticHardware.value("gpu_name", std::string());
    size_t start = 0;
    while (start < name.size())
    {
        const auto end = name.find(" + ", start);
        names.push_back(name.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos)
            break;
        start = end + 3;
    }
    for (size_t i = 0; i < readings.size(); ++i)
    {
        const auto &reading = readings[i];
        if (!reading.is_object())
            continue;
        const auto total = reading.value("mem_total", Json());
        const auto used = reading.value("mem_used", Json());
        const double totalBytes = total.is_number() ? total.get<double>() : 0.0;
        const double usedBytes = used.is_number() ? used.get<double>() : 0.0;
        Json row = {{"index", reading.value("index", (int)i)},
                    {"total", totalBytes},
                    {"free", std::max(0.0, totalBytes - usedBytes)},
                    {"parts", {{"used", usedBytes}}},
                    {"measured", "nvml"},
                    {"plain", true}};
        if (names.size() == readings.size())
            row["name"] = names[i];
        rows.push_back(std::move(row));
    }
    return rows;
}

inline std::string MonitorGpuRowKey(const Json &gpu)
{
    if (!gpu.is_object()) return {};
    const auto index = gpu.value("index", Json());
    if (index.is_string()) return index.get<std::string>();
    if (index.is_number()) return index.dump();
    if (!gpu.contains("dev")) return {};
    const auto dev = gpu["dev"];
    return "d" + (dev.is_string() ? dev.get<std::string>() : dev.dump());
}

inline int MonitorGpuOpenIndex(const std::vector<Json> &rows, const std::string &key)
{
    if (key.empty()) return -1;
    for (size_t i = 0; i < rows.size(); ++i)
        if (MonitorGpuRowKey(rows[i]) == key) return (int)i;
    return -1;
}

inline int MonitorGpuPanelHeight(size_t rows, bool expanded = false)
{
    return rows ? 40 + (int)rows * 64 + (expanded ? 46 : 0) : 0;
}

inline constexpr int MonitorGpuRowHeight = 64;
inline constexpr int MonitorGpuBarHeight = 28;
inline constexpr int MonitorTierBarHeight = 24;
inline constexpr int MonitorTierCardHeight = 68;

inline constexpr int MonitorPageContentMaxWidth = 1056; // v1.1.50's 1120px page minus 32px side padding.

inline constexpr bool MonitorPageOwnsBackground(bool monitorPage) noexcept
{
    return monitorPage;
}

struct MonitorPageBounds
{
    int left = 0, width = 0;
};

inline MonitorPageBounds MonitorPageBoundsFor(int left, int availableWidth) noexcept
{
    const int width = std::clamp(availableWidth, 0, MonitorPageContentMaxWidth);
    return {left + std::max(0, (availableWidth - width) / 2), width};
}

struct MonitorPageSections
{
    bool stacked = false;
    int throughputHeight = 352, rightNowTop = 0, rightNowHeight = 352;
    int contextTop = 0, contextHeight = 240, tiersTop = 0, gpuMapTop = 0;
};

inline MonitorPageSections MonitorPageSectionsFor(int monitorPageTop, int width) noexcept
{
    MonitorPageSections result;
    result.stacked = width < 836;
    result.rightNowTop = result.stacked ? monitorPageTop + result.throughputHeight + 16 : monitorPageTop;
    const int rowBottom = result.stacked ? result.rightNowTop + result.rightNowHeight
                                         : monitorPageTop + result.throughputHeight;
    result.contextTop = rowBottom + 16;
    result.tiersTop = result.contextTop + result.contextHeight + 16;
    result.gpuMapTop = result.tiersTop + MonitorTierCardHeight + 4;
    return result;
}

struct MonitorPageRefreshRegion
{
    int left = 0, top = 0, right = 0, bottom = 0;
};

inline MonitorPageRefreshRegion MonitorPageLayoutRefreshRegionFor(
    MonitorPageBounds before, MonitorPageBounds after, int top, int bottom) noexcept
{
    const int boundedTop = std::clamp(top, 0, std::max(0, bottom));
    return {std::min(before.left, after.left), boundedTop,
            std::max(before.left + before.width, after.left + after.width), std::max(boundedTop, bottom)};
}

inline constexpr int MonitorHardwareHistoryHeight = 216;
inline constexpr int MonitorRequestCardHeight = 82;

inline MonitorPageRefreshRegion MonitorGpuPanelRegionFor(
    int x, int monitorPageTop, int width, size_t gpuRows, bool gpuExpanded) noexcept
{
    const auto sections = MonitorPageSectionsFor(monitorPageTop, width);
    return {x, sections.gpuMapTop, x + width,
            sections.gpuMapTop + MonitorGpuPanelHeight(gpuRows, gpuExpanded)};
}

struct MonitorPageRefreshChanges
{
    bool throughput, rightNow, context, tiers, gpu, hardware;
};

inline Json MonitorPageMetricValue(const Json &object, const char *key)
{
    return object.is_object() && object.contains(key) ? object[key] : Json();
}

inline Json MonitorPageNestedMetricValue(const Json &object, const char *section, const char *key)
{
    return MonitorPageMetricValue(MonitorPageMetricValue(object, section), key);
}

inline bool MonitorPageLiveFieldsChanged(const Json &before, const Json &after,
                                         const char *const *keys, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (MonitorPageNestedMetricValue(before, "live", keys[i]) !=
            MonitorPageNestedMetricValue(after, "live", keys[i])) return true;
    return false;
}

inline Json MonitorPageLatestRequest(const Json &metrics)
{
    const auto requests = MonitorPageMetricValue(metrics, "requests");
    return requests.is_array() && !requests.empty() ? requests[0] : Json();
}

inline bool MonitorPageHistoryChanged(const Json &before, const Json &after, const char *const *keys, size_t count)
{
    for (size_t i = 0; i < count; ++i)
        if (MonitorPageNestedMetricValue(before, "history", keys[i]) !=
            MonitorPageNestedMetricValue(after, "history", keys[i])) return true;
    return false;
}

inline MonitorPageRefreshChanges MonitorPageRefreshChangesFor(
    const Json &before, const Json &after, const Json &beforeHealth = Json::object(),
    const Json &afterHealth = Json::object())
{
    const char *throughputLiveKeys[] = {"active", "state", "tok_s", "prefill_tok_s_mean", "writing", "spec_off", "elapsed_s"};
    const char *rightNowLiveKeys[] = {"state", "phase", "prompt_total", "prompt_read", "prompt_tokens",
                                      "generated", "elapsed_s", "active", "queued", "max_tokens"};
    const char *contextLiveKeys[] = {"state", "active", "prompt_tokens", "prompt_reused", "generated"};
    const bool throughputLive = MonitorPageLiveFieldsChanged(before, after, throughputLiveKeys,
        sizeof(throughputLiveKeys) / sizeof(throughputLiveKeys[0]));
    const bool rightNowLive = MonitorPageLiveFieldsChanged(before, after, rightNowLiveKeys,
        sizeof(rightNowLiveKeys) / sizeof(rightNowLiveKeys[0]));
    const bool contextLive = MonitorPageLiveFieldsChanged(before, after, contextLiveKeys,
        sizeof(contextLiveKeys) / sizeof(contextLiveKeys[0]));
    const bool request = MonitorPageLatestRequest(before) != MonitorPageLatestRequest(after);
    const bool totals = MonitorPageMetricValue(before, "totals") != MonitorPageMetricValue(after, "totals");
    const bool engine = MonitorPageMetricValue(before, "engine") != MonitorPageMetricValue(after, "engine");
    const bool tierNow = MonitorPageNestedMetricValue(before, "tiers", "now") !=
                         MonitorPageNestedMetricValue(after, "tiers", "now");
    const char *throughputKeys[] = {"tok_s", "prefill_tok_s_mean"};
    const char *hardwareKeys[] = {"gpu_util", "gpu_mem_used", "gpu_power", "gpu_temp", "gpu_pcie_rx_mb",
                                  "cpu", "ram_used", "disk_read_mb"};
    const bool throughputHistory = MonitorPageHistoryChanged(before, after, throughputKeys,
        sizeof(throughputKeys) / sizeof(throughputKeys[0]));
    const bool hardwareHistory = MonitorPageHistoryChanged(before, after, hardwareKeys,
        sizeof(hardwareKeys) / sizeof(hardwareKeys[0]));
    const bool healthContext = MonitorPageMetricValue(beforeHealth, "max_context") !=
                               MonitorPageMetricValue(afterHealth, "max_context");
    const bool gpu = MonitorPageMetricValue(before, "gpu_map") != MonitorPageMetricValue(after, "gpu_map") ||
                     MonitorPageMetricValue(before, "hardware_static") !=
                         MonitorPageMetricValue(after, "hardware_static") || engine;
    const bool hardware = MonitorPageMetricValue(before, "hardware") !=
                              MonitorPageMetricValue(after, "hardware") ||
                          hardwareHistory || engine;
    return {
        throughputLive || request || totals || engine || tierNow || throughputHistory,
        rightNowLive || request,
        contextLive || request || engine || healthContext,
        tierNow || engine,
        gpu,
        hardware};
}

inline std::vector<MonitorPageRefreshRegion> MonitorPageRefreshRegionsFor(
    int x, int pageTop, int width, size_t gpuRows, bool gpuExpanded,
    bool refreshThroughput = true, bool refreshRightNow = true, bool refreshContext = true,
    bool refreshTiers = true, bool refreshGpu = true, bool refreshHardware = true)
{
    const auto sections = MonitorPageSectionsFor(pageTop, width);
    const int gap = 16;
    const int throughputWidth = sections.stacked ? width : 520 + std::max(0, width - gap - 520 - 300) * 2 / 3;
    const int rightNowX = sections.stacked ? x : x + throughputWidth + gap;
    const int rightNowWidth = sections.stacked ? width : std::max(1, width - throughputWidth - gap);
    const int gpuPanelHeight = MonitorGpuPanelHeight(gpuRows, gpuExpanded);
    std::vector<MonitorPageRefreshRegion> regions;
    if (refreshThroughput) regions.push_back({x + 8, pageTop + 34, x + throughputWidth - 8,
                                               pageTop + sections.throughputHeight - 8});
    if (refreshRightNow) regions.push_back({rightNowX + 8, sections.rightNowTop + 34,
                                            rightNowX + rightNowWidth - 8,
                                            sections.rightNowTop + sections.rightNowHeight - 8});
    if (refreshContext) regions.push_back({x + 8, sections.contextTop + 12, x + width - 8,
                                           sections.contextTop + sections.contextHeight - 8});
    if (refreshTiers) regions.push_back({x + 8, sections.tiersTop + 2, x + width - 8,
                                         sections.tiersTop + MonitorTierCardHeight - 2});
    if (refreshGpu)
        regions.push_back(MonitorGpuPanelRegionFor(x, pageTop, width, gpuRows, gpuExpanded));
    if (refreshHardware)
    {
        const int hardwareTop = sections.gpuMapTop + gpuPanelHeight + 8;
        regions.push_back({x + 8, hardwareTop, x + width - 8,
                           hardwareTop + MonitorHardwareHistoryHeight});
    }
    return regions;
}

inline int MonitorGpuMapTop(int monitorPageTop, int width) noexcept
{
    return MonitorPageSectionsFor(monitorPageTop, width).gpuMapTop;
}

inline int MonitorGpuActionsTop(int monitorPageTop, int width, size_t rows, bool expanded) noexcept
{
    return MonitorGpuMapTop(monitorPageTop, width) + MonitorGpuPanelHeight(rows, expanded) +
           8 + MonitorHardwareHistoryHeight + 18;
}

inline int MonitorPageContentBottom(int monitorPageTop, int width, size_t rows, bool expanded,
                                    size_t requestRows = 0) noexcept
{
    const int actionsTop = MonitorGpuActionsTop(monitorPageTop, width, rows, expanded);
    const int requestsBottom = actionsTop + 70 + (int)requestRows * MonitorRequestCardHeight + 16;
    return std::max(actionsTop + 192, requestsBottom);
}

inline int MonitorPageScrollLimit(int monitorPageTop, int width, size_t rows, bool expanded,
                                  int viewportBottom, size_t requestRows = 0) noexcept
{
    return std::max(0, MonitorPageContentBottom(monitorPageTop, width, rows, expanded, requestRows) -
                       std::max(monitorPageTop, viewportBottom));
}

inline bool MonitorPanelGeometryChanged(size_t previousRows, bool previousExpanded,
                                        size_t currentRows, bool currentExpanded) noexcept
{
    return MonitorGpuPanelHeight(previousRows, previousExpanded) !=
           MonitorGpuPanelHeight(currentRows, currentExpanded);
}

inline int MonitorStatsColumns(int width, bool mobile) noexcept
{
    if (mobile) return 3;
    const int contentWidth = std::max(1, width - 44);
    // shortcut: keep two rows in the fixed-height slot; reflow Monitor geometry for panes under 336px wide.
    return std::clamp((contentWidth + 12) / 152, 2, 4);
}

inline int MonitorStatsColumnSpan(int index, int columns, bool mobile) noexcept
{
    return mobile && index == 3 ? columns : 1;
}

inline int MonitorGpuRowAt(int x, int y, int panelX, int panelY, int width, size_t rows, int expanded)
{
    if (x < panelX || x >= panelX + width) return -1;
    for (size_t i = 0; i < rows; ++i)
    {
        const int top = panelY + 39 + (int)i * MonitorGpuRowHeight +
                        (expanded >= 0 && (int)i > expanded ? 46 : 0);
        if (y >= top && y < top + MonitorGpuRowHeight + ((int)i == expanded ? 46 : 0)) return (int)i;
    }
    return -1;
}

struct MonitorGpuSegmentHit
{
    int row = -1;
    std::string key;
    std::wstring label;
    double bytes = 0;
    double total = 0;
};

inline MonitorGpuSegmentHit MonitorGpuSegmentAt(const std::vector<Json> &rows, int px, int py,
                                                int panelX, int panelY, int width, int expanded)
{
    if (width <= 0) return {};
    const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
    const int barX = panelX + 14;
    const int barWidth = std::max(1, width - 28);
    if (px < barX || px >= barX + barWidth) return {};
    struct Part { const char *key; const wchar_t *label; };
    static const Part parts[] = {
        {"weights", L"Model weights"}, {"kv", L"KV cache"}, {"act", L"Activations"},
        {"prompt", L"Prompt buffers"}, {"tp", L"Tensor parallel"}, {"drafter", L"Drafter"},
        {"pool", L"Expert pool"}, {"helper", L"Helper experts"}, {"vision", L"Vision encoder"},
        {"rest", L"CUDA and the rest"}, {"other", L"Other programs"}, {"used", L"In use"},
        {"free", L"Free"}};
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto &gpu = rows[i];
        const int rowY = panelY + 39 + (int)i * MonitorGpuRowHeight +
                         (fold && expanded >= 0 && (int)i > expanded ? 46 : 0);
        if (py < rowY + 16 || py >= rowY + 16 + MonitorGpuBarHeight) continue;
        const auto totalValue = gpu.value("total", Json());
        if (!totalValue.is_number() || totalValue.get<double>() <= 0) continue;
        const double total = totalValue.get<double>();
        const auto freeValue = gpu.value("free", Json());
        const double freeBytes = freeValue.is_number() ? std::max(0.0, freeValue.get<double>()) : 0.0;
        const auto values = gpu.value("parts", Json::object());
        double remaining = total;
        int drawn = 0;
        for (const auto &part : parts)
        {
            double bytes = 0;
            if (std::string(part.key) == "used" && gpu.value("plain", false))
                bytes = values.value("used", 0.0);
            else if (std::string(part.key) == "free") bytes = freeBytes;
            else if (values.is_object() && values.contains(part.key) && values[part.key].is_number())
                bytes = values[part.key].get<double>();
            bytes = std::min(remaining, std::max(0.0, bytes));
            if (bytes <= 0) continue;
            const int pixels = std::min(barWidth - drawn, (int)std::lround(barWidth * bytes / total));
            if (pixels <= 0) continue;
            if (px >= barX + drawn && px < barX + drawn + pixels)
                return {(int)i, part.key, part.label, bytes, total};
            drawn += pixels;
            remaining -= bytes;
            if (drawn >= barWidth) break;
        }
    }
    return {};
}

inline std::wstring MonitorGpuExpandedText(const Json &gpu, const Json &engine)
{
    std::wostringstream out;
    const auto layers = gpu.value("layers", Json());
    if (layers.is_array() && layers.size() >= 2 && layers[0].is_number() && layers[1].is_number())
    {
        const int first = layers[0].get<int>();
        const int last = layers[1].get<int>() - ((engine.value("engine_kind", std::string()) == "ds4" ||
                                                   engine.value("engine_kind", std::string()) == "mm3") ? 0 : 1);
        if (first >= 0 && last >= first)
            out << (first == last ? L"layer " : L"layers ") << first
                << (first == last ? L"" : L"–" + std::to_wstring(last));
    }
    const auto parts = gpu.value("parts", Json::object());
    const std::pair<const char *, const wchar_t *> labels[] = {
        {"weights", L"weights"}, {"kv", L"KV"}, {"act", L"activations"}, {"prompt", L"prompt"},
        {"tp", L"tensor"}, {"drafter", L"drafter"}, {"pool", L"expert pool"}, {"helper", L"helper"},
        {"vision", L"vision"}, {"rest", L"CUDA/rest"}, {"other", L"other apps"}};
    for (const auto &[key, label] : labels)
    {
        const auto bytes = parts.value(key, Json());
        if (!bytes.is_number() || bytes.get<double>() <= 0) continue;
        if (out.tellp() > 0) out << L" · ";
        out << label << L" " << MonitorGigabytes(bytes);
    }
    const auto context = gpu.value("ctx", engine.value("max_context", Json()));
    const auto window = MonitorInteger(engine, "kv_resident");
    if (context.is_number() && context.get<long long>() > 0 && window > 0 && window < context.get<long long>())
        out << L"\r\nKV window " << window << L" / " << context.get<long long>() << L" positions; remainder in RAM";
    const auto slots = gpu.value("slots", 0);
    if (slots > 0) out << (out.tellp() > 0 ? L" · " : L"") << slots << L" expert slots";
    const auto borrow = gpu.value("borrow", Json());
    if (borrow.is_number() && borrow.get<double>() > 0) out << L" · prompt borrow " << MonitorGigabytes(borrow);
    const auto kvRam = gpu.value("kv_ram", Json());
    if (kvRam.is_number() && kvRam.get<double>() > 0) out << L" · KV pinned " << MonitorGigabytes(kvRam) << L" RAM";
    if (gpu.value("vision_lent", false)) out << L" · vision encoder borrows the pool tail";
    return out.str();
}

inline std::wstring MonitorGpuSegmentHint(const Json &gpu, const Json &metrics, const std::string &key)
{
    const auto engine = metrics.value("engine", Json::object());
    if (key == "weights") return L"Attention, shared experts, and other dense tensors.";
    if (key == "act") return L"Working buffers for one token.";
    if (key == "prompt") return L"Prompt chunks, working buffers, and the CPU expert streaming ring.";
    if (key == "tp")
        return L"Tensor copies and each GPU group's whole tensors" +
            (gpu.value("tp_n", 0) > 1 ? L" (group size " + std::to_wstring(gpu.value("tp_n", 0)) + L")." : L".");
    if (key == "drafter")
    {
        std::wstring hint = L"Draft model";
        const auto weights = gpu.value("drafter", Json());
        const auto window = gpu.value("window", Json());
        if (weights.is_number()) hint += L" weights " + MonitorGigabytes(weights);
        if (window.is_number()) hint += L"; verify window " + MonitorGigabytes(window);
        return hint + L".";
    }
    if (key == "pool")
    {
        const auto slots = MonitorInteger(gpu, "slots");
        const auto borrow = gpu.value("borrow", Json());
        std::wstring hint = slots > 0 ? std::to_wstring(slots) + L" expert slots." : L"Routed expert cache.";
        if (borrow.is_number() && borrow.get<double>() > 0)
            hint += L" Prompts can borrow up to " + MonitorGigabytes(borrow) + L".";
        return hint;
    }
    if (key == "helper") return L"Separate helper expert pool; CPU backing remains available for fallback.";
    if (key == "vision")
    {
        if (gpu.value("vision_lent", false)) return L"The expert pool's tail, while the vision encoder processes an image.";
        const auto own = gpu.value("vision_own", Json());
        return own.is_number() && own.get<double>() > 0
            ? L"Vision tower memory in the engine process." : L"Vision tower memory in its own process.";
    }
    if (key == "rest") return L"Driver reserve, context, conversation snapshots, and small buffers.";
    if (key == "other") return L"Memory used by processes outside Maya.";
    if (key == "used") return L"Total GPU memory in use, as reported by the driver.";
    if (key == "free")
    {
        const auto free = gpu.value("free", Json());
        return free.is_number() && free.get<double>() < 1.5 * 1024.0 * 1024.0 * 1024.0
            ? L"Kept free for the driver and sampler." : L"Currently unused.";
    }
    if (key == "kv")
    {
        const auto context = gpu.value("ctx", engine.value("max_context", Json()));
        const auto window = MonitorInteger(engine, "kv_resident");
        std::wstring hint;
        if (context.is_number() && context.get<int64_t>() > 0)
        {
            const auto positions = context.get<int64_t>();
            hint = window > 0 && window < positions
                ? std::to_wstring(window) + L" of " + std::to_wstring(positions) + L" positions here; the rest in RAM."
                : std::to_wstring(positions) + L" context positions.";
        }
        const auto live = metrics.value("live", Json::object());
        const auto requests = metrics.value("requests", Json::array());
        int64_t inUse = 0;
        if (live.value("state", std::string("idle")) != "idle")
            inUse = MonitorInteger(live, "prompt_tokens") + MonitorInteger(live, "generated");
        else if (requests.is_array() && !requests.empty() && requests[0].is_object())
            inUse = MonitorInteger(requests[0], "prompt_tokens") + MonitorInteger(requests[0], "output_tokens");
        if (inUse > 0) hint += L" " + std::to_wstring(inUse) + L" tokens in use.";
        const auto kvRam = gpu.value("kv_ram", Json());
        if (kvRam.is_number() && kvRam.get<double>() > 0) hint += L" " + MonitorGigabytes(kvRam) + L" pinned in RAM.";
        return hint;
    }
    return {};
}

inline std::wstring MonitorTotalsLine(const Json &totals, const Json &guard)
{
    if (!totals.is_object() || !totals.value("requests", Json()).is_number() ||
        totals["requests"].get<double>() <= 0.0) return {};
    std::wstring since = L"—";
    const auto stamp = totals.value("since", Json());
    if (!stamp.is_number()) return {};
    const std::time_t raw = (std::time_t)stamp.get<double>();
    std::tm local{};
    if (localtime_s(&local, &raw) == 0)
    {
        std::wostringstream time;
        time << std::put_time(&local, L"%a %H:%M");
        since = time.str();
    }
    const auto prompt = MonitorInteger(totals, "prompt_tokens");
    const auto reused = MonitorInteger(totals, "reused");
    const auto read = std::max(0LL, prompt - reused);
    const auto output = MonitorInteger(totals, "output_tokens");
    std::wostringstream out;
    out << L"Since " << since << L": " << read << L" prompt tokens read";
    const auto promptMs = totals.value("prompt_ms", Json());
    if (read > 0 && promptMs.is_number() && promptMs.get<double>() > 0.0)
        out << L" at " << std::fixed << std::setprecision(0) << read / (promptMs.get<double>() / 1000.0) << L" tok/s";
    out << L" (" << reused << L" reused) · " << output << L" written";
    const auto decodeMs = totals.value("decode_ms", Json());
    if (output > 0 && decodeMs.is_number() && decodeMs.get<double>() > 0.0)
        out << L" at " << std::fixed << std::setprecision(1) << output / (decodeMs.get<double>() / 1000.0) << L" tok/s";

    if (guard.is_object())
    {
        std::vector<std::wstring> did, tools;
        const auto add = [&](std::vector<std::wstring> &items, const char *key,
                             const wchar_t *singular, const wchar_t *plural) {
            const auto value = guard.value(key, Json());
            if (value.is_number() && value.get<double>() > 0.0)
                items.push_back(MonitorMetric(guard, key, 0) + L" " +
                    (value.get<double>() == 1.0 ? singular : plural));
        };
        add(did, "penalised", L"cut with a short penalty", L"cut with a short penalty");
        add(did, "thinking_closed", L"closed the thinking", L"closed the thinking");
        add(did, "stopped", L"ended the answer", L"ended the answer");
        add(tools, "tool_repeats", L"repeated tool call noted", L"repeated tool calls noted");
        add(tools, "tool_blocked", L"blocked", L"blocked");
        add(tools, "kept_going", L"kept going", L"kept going");
        add(tools, "collapsed", L"prompt without the repeats", L"prompts without the repeats");
        const auto detected = guard.value("detected", Json());
        if (detected.is_number() && detected.get<double>() > 0.0)
        {
            out << L" · loop guard: " << MonitorMetric(guard, "detected", 0)
                << (detected.get<double>() == 1.0 ? L" loop seen" : L" loops seen");
            if (!did.empty())
            {
                out << L" (";
                for (size_t i = 0; i < did.size(); ++i) out << (i ? L", " : L"") << did[i];
                out << L")";
            }
        }
        if (!tools.empty())
        {
            out << (detected.is_number() && detected.get<double>() > 0.0 ? L", " : L" · loop guard: ");
            for (size_t i = 0; i < tools.size(); ++i) out << (i ? L", " : L"") << tools[i];
        }
        else if (did.empty() && guard.value("on", true) == false)
            out << L" · loop guard off";
    }
    return out.str();
}

inline std::wstring MonitorSpeculationText(const Json &metrics, double now = (double)std::time(nullptr))
{
    const auto engine = metrics.value("engine", Json::object());
    const auto modes = engine.value("speculation", Json());
    if (!modes.is_array()) return L"not reported";
    std::vector<std::wstring> names;
    for (const auto &mode : modes)
        if (mode.is_string())
        {
            const auto value = mode.get<std::string>();
            names.emplace_back(value.begin(), value.end());
        }
    if (std::any_of(names.begin(), names.end(), [](const auto &value) { return value != L"Lookup"; }))
        names.erase(std::remove(names.begin(), names.end(), L"Lookup"), names.end());
    std::wstring name;
    for (const auto &value : names)
    {
        if (!name.empty()) name += L" + ";
        name += value;
    }
    if (name.empty()) return L"off";
    const auto live = metrics.value("live", Json::object());
    const bool running = live.value("state", std::string()) == "generating";
    if (running && live.value("spec_off", false)) return L"off";
    const auto requests = metrics.value("requests", Json::array());
    const Json last = requests.is_array() && !requests.empty() && requests[0].is_object()
        ? requests[0] : Json::object();
    if (!running && last.value("spec_off", false)) return L"off";
    Json stat = Json::object();
    if (running)
    {
        const auto current = MonitorTierNow(metrics);
        const auto stamp = current.value("time", Json());
        bool fresh = stamp.is_number() && now - stamp.get<double>() <= 30.0;
        const auto elapsed = live.value("elapsed_s", Json());
        if (fresh && elapsed.is_number())
            fresh = stamp.get<double>() >= now - elapsed.get<double>() - 1.0;
        if (fresh) stat = current;
    }
    else stat = last;
    const auto offeredValue = stat.value(running ? "draft_off" : "drafts_offered", Json());
    const auto acceptedValue = stat.value(running ? "draft_acc" : "drafts_accepted", Json());
    double accepted = 0.0, offered = 0.0, percent = 0.0;
    bool hasPercent = false;
    if (offeredValue.is_number() && offeredValue.get<double>() > 0.0 && acceptedValue.is_number())
    {
        offered = offeredValue.get<double>();
        accepted = acceptedValue.get<double>();
        percent = 100.0 * accepted / offered;
        hasPercent = true;
    }
    else if (running && stat.contains("drafts") && stat["drafts"].is_number())
    {
        percent = 100.0 * stat["drafts"].get<double>();
        hasPercent = true;
    }
    if (!hasPercent) return name + L" · waiting for drafts";
    std::wostringstream out;
    out << name << L" · " << std::fixed << std::setprecision(0) << percent << L"%";
    if (offered > 0.0)
        out << L" (" << std::setprecision(0) << accepted << L" of " << offered << L" drafts kept)";
    return out.str();
}

inline std::wstring MonitorReport(const Json &metrics, bool allRequests = false, const Json &health = Json::object())
{
    const auto live = metrics.value("live", Json::object());
    const auto hardware = metrics.value("hardware", Json::object());
    const auto tiers = MonitorTierNow(metrics);
    const auto engine = metrics.value("engine", Json::object());
    const auto totals = metrics.value("totals", Json::object());
    const auto peak = totals.value("peak_decode", Json::object());
    const auto context = MonitorContextUsageValues(metrics, health);
    std::wostringstream out;
    out << MonitorWarnings(metrics) << L"THROUGHPUT\r\nDecode: " << MonitorMetric(live, "tok_s") << L" tok/s   Prefill: "
        << MonitorMetric(live, "prefill_tok_s_mean") << L" tok/s   Time/token: " << MonitorMetric(tiers, "ms_tok")
        << L" ms   VRAM hit: ";
    const auto hit = tiers.value("vram_hit", Json());
    if (hit.is_number())
        out << std::fixed << std::setprecision(1) << (100.0 * hit.get<double>()) << L"%";
    else
        out << L"—";
    out << L"   Peak: " << MonitorPeakDecodeDetails(peak) << L"   Requests: "
        << MonitorMetric(totals, "requests", 0) << L"\r\nSpeculation: " << MonitorSpeculationText(metrics) << L"\r\n";
    const auto totalsSummary = MonitorTotalsLine(totals, metrics.value("loop_guard", Json::object()));
    if (!totalsSummary.empty()) out << totalsSummary << L"\r\n";
    out
        << L"\r\nCURRENT REQUEST\r\nState: " << MonitorText(live, "state", L"unknown") << L" / "
        << MonitorText(live, "phase", L"idle") << L"   Active: " << MonitorMetric(live, "active", 0)
        << L"   Queued: " << MonitorMetric(live, "queued", 0) << L"   Writing: "
        << MonitorMetric(live, "writing", 0) << L"\r\nPrompt: " << MonitorMetric(live, "prompt_tokens", 0)
        << L" tokens   Read: " << MonitorMetric(live, "prompt_read", 0) << L" / "
        << MonitorMetric(live, "prompt_total", 0) << L"   Generated: " << MonitorMetric(live, "generated", 0)
        << L" tokens   Elapsed: " << MonitorMetric(live, "elapsed_s") << L" s\r\nContext used: ";
    if (context.maximum > 0)
        out << context.used << L" / " << context.maximum << L" tokens"
            << L"\r\nPrompt: " << context.prompt << L" tokens (";
    else
        out << L"—";
    if (context.maximum > 0)
    {
        if (context.reusedReported)
            out << context.reused << L" reused from cache, " << context.fresh << L" read new";
        else
            out << L"cache reuse not reported, " << context.prompt << L" read new";
        out << L") · Written: " << context.written << L" tokens · Free: "
            << (context.maximum - context.used) << L" tokens";
        if (context.window > 0)
        {
            out << L"\r\nVRAM window: " << context.window << L" / " << context.maximum << L" tokens. ";
            if (context.used > context.window)
                out << L"This chat is past the window; its remaining KV cache is kept in RAM (about "
                    << MonitorGigabytes(Json((double)context.used * 12288.0)) << L"). ";
            else
                out << L"A chat up to this size keeps its full KV cache in GPU memory; longer chats keep the remainder in RAM. ";
            out << L"Change it in Models > Engine settings > Prompts and cache.";
        }
        else
            out << L"\r\nContext limit includes the chat, files and tool results. Change it in Settings > Model > Context size.";
    }
    out << L"\r\n\r\nHARDWARE\r\nGPU: " << MonitorMetric(hardware, "gpu_util", 0) << L"%   "
        << MonitorMetric(hardware, "gpu_temp", 0) << L" C   VRAM: " << MonitorMetric(hardware, "gpu_mem_used", 0)
        << L" / " << MonitorMetric(hardware, "gpu_mem_total", 0) << L" bytes\r\nCPU: "
        << MonitorMetric(hardware, "cpu") << L"%   RAM: " << MonitorMetric(hardware, "ram_used", 0) << L" / "
        << MonitorMetric(hardware, "ram_total", 0) << L" bytes   Disk read: " << MonitorDiskReadRate(hardware);
    const auto diskWrite = MonitorDiskWriteRate(hardware);
    if (!diskWrite.empty()) out << L"   Disk write: " << diskWrite;
    out << L"\r\n\r\nEXPERT PLACEMENT\r\nVRAM: "
        << MonitorMetric(tiers, "vram_gb") << L" GB   RAM: " << MonitorMetric(tiers, "ram_gb") << L" GB   SSD: "
        << MonitorMetric(tiers, "disk_gb") << L" GB   PCIe: " << MonitorMetric(tiers, "pcie") << L" MB/s\r\n";
    out << L"\r\n" << MonitorExpertReport(metrics) << MonitorGpuMemoryReport(metrics);
    const auto &requests = metrics.value("requests", Json::array());
    if (requests.is_array())
    {
        out << L"\r\nRECENT REQUESTS\r\nTime · Status · API · Prompt · Reused · Prefill · Output · Decode · VRAM hit · Took\r\n";
        size_t count = 0;
        for (const auto &request : requests)
        {
            if (!allRequests && count == 12)
                break;
            ++count;
            out << MonitorRequestText(request) << L"\r\n";
        }
        if (!count)
            out << L"No requests yet\r\n";
    }
    return out.str();
}

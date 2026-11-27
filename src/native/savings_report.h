#pragma once
#include "http.h"
#include "model_details.h"
#include "savings_price.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <vector>

inline std::string SavingsMoney(const Json &value)
{
    if (value.is_null() || !value.is_number())
        return "–";
    const double amount = value.get<double>();
    if (amount != 0.0 && amount < 0.01) return "< $0.01";
    std::ostringstream out;
    out << '$' << std::fixed << std::setprecision(amount >= 1000.0 ? 0 : 2) << amount;
    return out.str();
}

inline std::string SavingsPriceMoney(const Json &value)
{
    if (value.is_null() || !value.is_number()) return "–";
    const double amount = value.get<double>();
    if (amount < 0.01)
    {
        const auto text = SavingsPriceNumberText(amount);
        return text.empty() ? "–" : '$' + text;
    }
    std::ostringstream out;
    out << '$' << std::fixed << std::setprecision(2) << amount;
    return out.str();
}

inline std::string SavingsTokens(const Json &value)
{
    if (!value.is_number()) return "–";
    const double count = value.get<double>();
    if (count >= 1e6 || count >= 1e3)
    {
        const bool millions = count >= 1e6;
        const double scaled = count / (millions ? 1e6 : 1e3);
        std::ostringstream out;
        out << std::fixed << std::setprecision(scaled < 10.0 ? 1 : 0) << scaled << (millions ? 'M' : 'K');
        return out.str();
    }
    return std::to_string(static_cast<uint64_t>(std::max(0.0, count)));
}

inline std::string SavingsDate(const Json &value)
{
    if (!value.is_number()) return {};
    const double seconds = value.get<double>();
    if (!std::isfinite(seconds) || seconds < 0 || seconds > static_cast<double>(std::numeric_limits<std::time_t>::max()))
        return {};
    const auto raw = static_cast<std::time_t>(seconds);
    std::tm local{};
    if (localtime_s(&local, &raw) != 0) return {};
    char month[16]{};
    if (!std::strftime(month, sizeof(month), "%b", &local)) return {};
    return std::string(month) + " " + std::to_string(local.tm_mday);
}

inline std::string SavingsUptime(const Json &value, std::time_t now = std::time(nullptr))
{
    if (!value.is_number()) return {};
    const double started = value.get<double>();
    if (!std::isfinite(started) || started < 0 || started > static_cast<double>(std::numeric_limits<std::time_t>::max()))
        return {};
    const auto minutes = std::max<long long>(0, std::llround((static_cast<double>(now) - started) / 60.0));
    return minutes < 60 ? std::to_string(minutes) + " min"
                        : std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

inline std::string SavingsString(const Json &object, const char *key)
{
    return object.is_object() && object.contains(key) && object[key].is_string()
        ? object[key].get<std::string>() : std::string();
}

inline std::string SavingsReportText(const Json &report)
{
    if (!report.is_object())
        return "Maya returned an invalid savings report.";
    std::ostringstream out;
    out << "SAVED TOTAL   " << SavingsMoney(report.value("usd", Json())) << "\r\n";
    std::vector<std::string> apis;
    const auto countedPrices = report.value("prices", Json::array());
    if (countedPrices.is_array())
        for (const auto &price : countedPrices)
        {
            const auto api = SavingsString(price, "api");
            if (!api.empty() && std::find(apis.begin(), apis.end(), api) == apis.end()) apis.push_back(api);
        }
    std::string attribution = "the model's own API";
    if (!apis.empty())
    {
        attribution.clear();
        for (size_t i = 0; i < apis.size(); ++i)
        {
            if (i) attribution += i + 1 == apis.size() ? " and " : ", ";
            attribution += apis[i] + "'s";
        }
        attribution += apis.size() == 1 ? " own API" : " own APIs";
    }
    const auto since = SavingsDate(report.value("since", Json()));
    out << "What " << attribution << " would have charged for the same tokens";
    if (!since.empty()) out << ", since " << since;
    out << ".\r\n";
    const auto start = report.value("start", Json::object());
    if (start.is_object() && !start.empty())
    {
        out << "\r\nTHIS START    " << ModelDisplayName(SavingsString(start, "model"));
        const auto quant = SavingsString(start, "quant"), uptime = SavingsUptime(start.value("started", Json()));
        if (!quant.empty()) out << " · " << quant;
        if (!uptime.empty()) out << " · up " << uptime;
        out << " · " << SavingsMoney(start.value("usd", Json())) << "\r\n";
    }
    out << "\r\nMODEL HISTORY\r\n";
    const auto models = report.value("models", Json::array());
    if (!models.is_array() || models.empty())
        out << "No model requests have been counted yet.\r\n";
    else
        for (const auto &m : models)
        {
            const auto tokens = m.value("tokens", Json::object());
            const auto model = SavingsString(m, "model");
            out << (model.empty() ? "Unknown model" : ModelDisplayName(model));
            const auto quant = SavingsString(m, "quant");
            if (!quant.empty()) out << " · " << quant;
            if (m.value("running", false)) out << " [RUNNING]";
            const auto cached = tokens.value("cached", Json(0));
            const auto input = tokens.value("input", Json(0));
            const auto read = input.is_number() && cached.is_number()
                ? Json(input.get<double>() + cached.get<double>()) : Json();
            out << "\r\n  Requests: " << SavingsTokens(tokens.value("requests", Json(0)))
                << " · read " << SavingsTokens(read);
            if (cached.is_number() && cached.get<double>() > 0)
                out << " (" << SavingsTokens(cached) << " cached)";
            out << " · wrote " << SavingsTokens(tokens.value("output", Json(0)))
                << " · saved " << (m.value("usd", Json()).is_null() ? "no API price" : SavingsMoney(m.value("usd", Json())))
                << "\r\n";
        }
    out << "\r\nPRICE LIST (USD per 1M tokens)\r\n";
    const auto priceList = report.value("price_list", Json::array());
    if (priceList.is_array())
        for (const auto &p : priceList)
        {
            const auto model = SavingsString(p, "model");
            out << (model.empty() ? "Unknown model" : ModelDisplayName(model));
            const auto api = SavingsString(p, "api");
            out << " (" << (p.value("custom", false) ? "your prices" : (api.empty() ? "API" : api)) << ")";
            out << "\r\n  input " << SavingsPriceMoney(p.value("input", Json()))
                << " · cached " << SavingsPriceMoney(p.value("cached", Json()))
                << " · output " << SavingsPriceMoney(p.value("output", Json()));
            if (p.contains("peak") && p["peak"].is_number() && p["peak"].get<double>() > 1.0)
                out << " · peak ×" << p["peak"].get<double>();
            out << "\r\n";
        }
    const auto prices = report.value("prices", Json::array());
    if (prices.is_array() && std::any_of(prices.begin(), prices.end(), [](const Json &price) {
            return price.is_object() && price.contains("peak") && price["peak"].is_number() && price["peak"].get<double>() > 1.0;
        }))
        out << "Peak-hour prices apply weekdays 01–04 and 06–10 UTC. Cached prompt tokens use the cached rate; electricity is not counted.\r\n";
    return out.str();
}

inline std::string SavingsSidebarLabel(const Json &metrics)
{
    const auto savings = metrics.is_object() ? metrics.value("savings", Json()) : Json();
    if (!savings.is_object() || !savings.value("priced", false)) return {};
    const auto usd = savings.value("usd", Json());
    return "Saved " + (usd.is_number() ? SavingsMoney(usd) : std::string("–")) + " vs the API";
}

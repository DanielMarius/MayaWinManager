#pragma once
#include "http.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

inline bool SavingsPriceWhitespace(wchar_t c) noexcept
{
    return (c >= 0x0009 && c <= 0x000D) || c == 0x0020 || c == 0x00A0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200A) || c == 0x2028 || c == 0x2029 || c == 0x202F ||
           c == 0x205F || c == 0x3000 || c == 0xFEFF;
}

inline std::string SavingsPriceNumberText(double amount)
{
    if (!std::isfinite(amount)) return {};
    char buffer[128]{};
    const double magnitude = std::fabs(amount);
    const auto format = magnitude >= 1e-6 && magnitude < 1e21 ? std::chars_format::fixed : std::chars_format::general;
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), amount, format);
    if (result.ec != std::errc{}) return {};
    std::string text(buffer, result.ptr);
    const size_t exponent = text.find('e');
    if (exponent != std::string::npos)
    {
        size_t digits = exponent + 1;
        const char sign = digits < text.size() && (text[digits] == '-' || text[digits] == '+') ? text[digits++] : '\0';
        while (digits + 1 < text.size() && text[digits] == '0') ++digits;
        std::string normalized = text.substr(0, exponent + 1);
        if (sign) normalized += sign;
        normalized += text.substr(digits);
        text = std::move(normalized);
    }
    return text;
}

inline std::string SavingsPriceEditorText(const Json &value, bool peak)
{
    if (!value.is_number()) return {};
    const double amount = value.get<double>();
    const double scaled = amount * 1e6;
    const double rounded = std::floor(scaled + 0.5) / 1e4;
    if (!peak && std::isfinite(scaled) && std::isfinite(rounded) && (amount == 0.0 || rounded != 0.0) &&
        std::trunc(rounded) == rounded)
    {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << amount;
        return out.str();
    }
    return SavingsPriceNumberText(amount);
}

inline double SavingsPriceNumber(const std::string &text, const char *label, double min, double max)
{
    const auto wide = Wide(text);
    size_t begin = 0, end = wide.size();
    while (begin < end && SavingsPriceWhitespace(wide[begin])) ++begin;
    while (end > begin && SavingsPriceWhitespace(wide[end - 1])) --end;
    size_t at = begin;
    bool hasDigit = false;
    while (at < end && wide[at] >= L'0' && wide[at] <= L'9') { ++at; hasDigit = true; }
    if (at < end && wide[at] == L'.')
    {
        ++at;
        while (at < end && wide[at] >= L'0' && wide[at] <= L'9') { ++at; hasDigit = true; }
    }
    if (!hasDigit || at != end)
        throw std::runtime_error(std::string(label) + " must be a number.");
    size_t used = 0;
    double value = 0;
    try { value = std::stod(wide.substr(begin, end - begin), &used); }
    catch (...) { throw std::runtime_error(std::string(label) + " must be a number."); }
    if (used != end - begin || !std::isfinite(value) || value < min || value > max)
        throw std::runtime_error(std::string(label) + " must be from " + std::to_string(min) + " to " + std::to_string(max) + ".");
    return value;
}

inline bool SavingsPriceDraftChanged(const std::vector<std::string> &draft, const std::vector<std::string> &saved)
{
    if (draft.size() != saved.size()) return true;
    for (size_t i = 0; i < draft.size(); ++i)
    {
        const auto a = Wide(draft[i]), b = Wide(saved[i]);
        size_t ab = 0, ae = a.size(), bb = 0, be = b.size();
        while (ab < ae && SavingsPriceWhitespace(a[ab])) ++ab;
        while (ae > ab && SavingsPriceWhitespace(a[ae - 1])) --ae;
        while (bb < be && SavingsPriceWhitespace(b[bb])) ++bb;
        while (be > bb && SavingsPriceWhitespace(b[be - 1])) --be;
        if (a.compare(ab, ae - ab, b, bb, be - bb) != 0) return true;
    }
    return false;
}

inline bool SavingsPriceDraftCanRestore(const std::vector<std::string> &draft,
                                        const std::vector<std::string> &saved,
                                        const std::vector<std::string> &current)
{
    return draft.size() == saved.size() && !SavingsPriceDraftChanged(saved, current);
}

inline Json SavingsPriceRequest(const std::string &model, const std::vector<std::string> &values, bool peak)
{
    if (model.empty() || values.size() < 3)
        throw std::runtime_error("Select a model and enter input, cached and output prices.");
    Json price{{"input", SavingsPriceNumber(values[0], "Input price", 0, 1000)},
               {"cached", SavingsPriceNumber(values[1], "Cached input price", 0, 1000)},
               {"output", SavingsPriceNumber(values[2], "Output price", 0, 1000)}};
    if (peak)
    {
        if (values.size() < 4) throw std::runtime_error("Enter a peak-hours multiplier.");
        price["peak"] = SavingsPriceNumber(values[3], "Peak-hours multiplier", 1, 10);
    }
    return Json{{"model", model}, {"price", price}};
}

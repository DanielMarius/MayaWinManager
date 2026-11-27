#pragma once
#include "http.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <vector>

inline constexpr int NativeAboutReportControlId = 209;
inline constexpr int NativeModelSetupCommandControlId = 279;
static_assert(NativeAboutReportControlId != NativeModelSetupCommandControlId);

inline std::string CatalogSizeLabel(const Json &entry)
{
    if (!entry.contains("size_gb") || !entry["size_gb"].is_number() || entry["size_gb"].get<double>() == 0)
        return "size shown when it starts";
    std::ostringstream out;
    try { out.imbue(std::locale("")); }
    catch (...) {}
    out << std::fixed << std::setprecision(0) << std::round(entry["size_gb"].get<double>());
    return "about " + out.str() + " GB";
}

inline std::string CatalogDetailsLabel(const Json &entry)
{
    std::vector<std::string> details;
    if (entry.is_object())
    {
        const auto group = entry.find("group"), by = entry.find("by");
        if (group != entry.end() && group->is_string() && group->get<std::string>() == "other" &&
            by != entry.end() && by->is_string() && !by->get_ref<const std::string &>().empty())
            details.push_back("by " + by->get<std::string>());
    }
    details.push_back(CatalogSizeLabel(entry));
    if (entry.is_object())
    {
        const auto note = entry.find("note");
        if (note != entry.end() && note->is_string() && !note->get_ref<const std::string &>().empty())
            details.push_back(note->get<std::string>());
    }
    std::string label;
    for (const auto &detail : details)
    {
        if (!label.empty()) label += " · ";
        label += detail;
    }
    return label;
}

inline constexpr const wchar_t *NativeModelSetupCommand(unsigned choice) noexcept
{
    if (choice == 1) return L"START-MAYA.bat --setup";
    if (choice == 2) return L"./maya.sh --setup";
    return nullptr;
}

inline bool CatalogInstalled(const Json &entry)
{
    if (!entry.is_object()) return false;
    const auto it = entry.find("installed");
    if (it == entry.end() || it->is_null()) return false;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number()) return it->get<double>() != 0;
    if (it->is_string()) return !it->get_ref<const std::string &>().empty();
    return true;
}
inline bool ModelCatalogHasSnapshot(const Json &catalog)
{
    return catalog.is_object() && catalog.contains("models") && catalog["models"].is_array();
}
inline constexpr const char *CatalogUnavailablePrefix() noexcept
{
    return "Model catalog unavailable:";
}
inline std::string CatalogUnavailableStatus(const std::string &error)
{
    return std::string(CatalogUnavailablePrefix()) + " " + error;
}
inline bool CatalogStatusIsUnavailable(const std::string &status)
{
    return status.rfind(CatalogUnavailablePrefix(), 0) == 0;
}
inline bool CatalogCanDownload(const Json &entry)
{
    return entry.is_object() && entry.contains("id") && entry["id"].is_string() &&
           !entry["id"].get<std::string>().empty() && !CatalogInstalled(entry);
}

inline bool CatalogCanStartPull(const Json &entry, bool pullActive, bool requestInFlight, bool catalogUnavailable)
{
    return !catalogUnavailable && !pullActive && !requestInFlight && CatalogCanDownload(entry);
}

inline std::vector<int> CatalogListRowModelIndices(const Json &models)
{
    std::vector<int> rows;
    if (!models.is_array()) return rows;
    const auto isOther = [](const Json &entry)
    {
        return entry.is_object() && entry.contains("group") && entry["group"].is_string() &&
               entry["group"].get<std::string>() == "other";
    };
    bool hasOther = false;
    for (std::size_t i = 0; i < models.size(); ++i)
    {
        if (isOther(models[i])) hasOther = true;
        else rows.push_back(static_cast<int>(i));
    }
    if (hasOther) rows.push_back(-1);
    for (std::size_t i = 0; i < models.size(); ++i)
        if (isOther(models[i])) rows.push_back(static_cast<int>(i));
    return rows;
}

inline int CatalogModelIndexForListRow(const std::vector<int> &rows, int row) noexcept
{
    return row >= 0 && static_cast<std::size_t>(row) < rows.size() ? rows[static_cast<std::size_t>(row)] : -1;
}

inline bool CatalogDownloadFailed(const Json &entry, const Json &pull)
{
    if (!entry.is_object() || !pull.is_object()) return false;
    const auto id = entry.find("id"), model = pull.find("model"), state = pull.find("state");
    return id != entry.end() && id->is_string() && model != pull.end() && model->is_string() &&
           id->get<std::string>() == model->get<std::string>() && state != pull.end() &&
           state->is_string() && state->get<std::string>() == "error";
}

inline bool ModelCatalogVisible(bool expanded, bool pullActive)
{
    return expanded || pullActive;
}

constexpr const wchar_t *NativeModelLoadButtonText(bool loaded, bool loading, bool unloading) noexcept
{
    if (loading) return L"Loading…";
    if (unloading) return L"Unloading…";
    return loaded ? L"Unload model" : L"Load model";
}

constexpr bool NativeModelLoadButtonEnabled(bool selectedRunning, bool canUnload, bool busy) noexcept
{
    return selectedRunning && canUnload && !busy;
}

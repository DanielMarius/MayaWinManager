#pragma once
#include "http.h"

inline bool ModelLeftoverDeleteEnabled(bool selected, bool busy, bool switchRequest,
                                      bool switchRestart, bool settingsRestart) noexcept
{
    return selected && !busy && !switchRequest && !switchRestart && !settingsRestart;
}

inline Json ModelLeftoverRequest(const std::string &path)
{
    if (path.empty())
        throw std::runtime_error("Select an unused file or folder first.");
    return Json{{"path", path}};
}

inline uint64_t ModelLeftoverFreed(const Json &outcome)
{
    if (!outcome.is_object() || !outcome.contains("freed")) return 0;
    const auto &freed = outcome["freed"];
    if (freed.is_number_unsigned()) return freed.get<uint64_t>();
    return freed.is_number_integer() && freed.get<int64_t>() > 0 ? (uint64_t)freed.get<int64_t>() : 0;
}

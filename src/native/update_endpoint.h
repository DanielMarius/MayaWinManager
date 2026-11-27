#pragma once

inline constexpr const wchar_t *MayaUpdateEndpointPath(bool post, bool force) noexcept
{
    return post ? L"/api/update" : force ? L"/api/update?force=1" : L"/api/update";
}

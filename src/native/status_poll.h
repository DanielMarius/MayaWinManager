#pragma once

enum class NativeApiConnection { CONNECTING, READY, API_KEY, UNREACHABLE };

template <typename JsonValue>
inline unsigned NativeMetricsHttpStatusOrZero(const JsonValue &value)
{
    if (!value.is_object()) return 0u;
    const auto status = value.find("http_status");
    if (status == value.end() || !status->is_number_integer()) return 0u;
    const auto code = status->get<long long>();
    return code > 0 && code <= 999 ? static_cast<unsigned>(code) : 0u;
}

template <typename JsonValue>
inline bool NativeMetricsPayloadIsObject(const JsonValue &value)
{
    return value.is_object();
}

constexpr NativeApiConnection NativeApiConnectionAfterSuccess() noexcept
{
    return NativeApiConnection::READY;
}

constexpr NativeApiConnection NativeApiConnectionAfterFailure(NativeApiConnection previous,
                                                               unsigned failures,
                                                               bool unauthorized) noexcept
{
    if (unauthorized) return NativeApiConnection::API_KEY;
    return failures >= 3 ? NativeApiConnection::UNREACHABLE : previous;
}

inline NativeApiConnection NativeApiConnectionAfterMetricsPoll(NativeApiConnection previous,
                                                                unsigned &failures,
                                                                unsigned httpStatus,
                                                                bool success) noexcept
{
    if (success)
    {
        failures = 0;
        return NativeApiConnection::READY;
    }
    if (httpStatus == 401) return NativeApiConnection::API_KEY;
    if (failures < 3) ++failures;
    return NativeApiConnectionAfterFailure(previous, failures, false);
}

constexpr const wchar_t *NativeApiConnectionText(NativeApiConnection connection) noexcept
{
    switch (connection)
    {
    case NativeApiConnection::API_KEY: return L"API key needed";
    case NativeApiConnection::UNREACHABLE: return L"Server not reachable";
    case NativeApiConnection::CONNECTING: return L"Connecting…";
    default: return L"";
    }
}

constexpr bool NativeStatusUiNeedsRefresh(bool healthChanged, bool serverRecovered, bool modelChanged) noexcept
{
    return healthChanged || serverRecovered || modelChanged;
}

constexpr unsigned NativeStatusPollIntervalMs(bool minimized) noexcept
{
    return minimized ? 10000u : 2000u;
}

constexpr unsigned NativeMetricsPollIntervalMs(bool minimized, bool monitor, bool offline,
                                               bool foreground = true) noexcept
{
    if (offline) return minimized ? 30000u : 10000u;
    if (minimized) return 5000u;
    return monitor && foreground ? 1000u : 10000u;
}

constexpr bool NativeJobShowsProgress(bool backgroundPoll, bool routineRefresh = false) noexcept
{
    return !backgroundPoll && !routineRefresh;
}

constexpr bool NativePollShowsError(bool backgroundPoll, bool routinePoll = false) noexcept
{
    return !backgroundPoll && !routinePoll;
}

constexpr bool NativePollInvalidatesWindow(bool backgroundPoll, bool routineRefresh = false) noexcept
{
    return !backgroundPoll && !routineRefresh;
}

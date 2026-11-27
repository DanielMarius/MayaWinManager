#pragma once

enum class ModelSwitchWatchState { WAITING, COMPLETE, TIMED_OUT };

inline bool NativeRestartTimeoutElapsed(unsigned long long startedAt,
                                        unsigned long long now) noexcept
{
    return now >= startedAt && now - startedAt >= 15ULL * 60 * 1000;
}

inline ModelSwitchWatchState ModelSwitchWatchStep(bool sawDisconnect, bool online, bool ready, bool timedOut)
{
    if (timedOut) return ModelSwitchWatchState::TIMED_OUT;
    return sawDisconnect && online && ready ? ModelSwitchWatchState::COMPLETE : ModelSwitchWatchState::WAITING;
}

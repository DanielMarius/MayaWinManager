#pragma once

#include <algorithm>
#include <cstdint>
#include <mfplay.h>

inline const wchar_t *MayaVideoEventStatus(MFP_EVENT_TYPE type) noexcept
{
    switch (type)
    {
    case MFP_EVENT_TYPE_MEDIAITEM_CREATED: return L"Loading video…";
    case MFP_EVENT_TYPE_MEDIAITEM_SET: return L"Video ready.";
    case MFP_EVENT_TYPE_PLAY: return L"Playing video.";
    case MFP_EVENT_TYPE_PAUSE: return L"Video paused.";
    case MFP_EVENT_TYPE_STOP: return L"Video stopped.";
    case MFP_EVENT_TYPE_PLAYBACK_ENDED: return L"Playback ended.";
    case MFP_EVENT_TYPE_ERROR: return L"Video playback failed.";
    default: return nullptr;
    }
}

inline int MayaVideoSeekSlider(int64_t duration, int64_t position, int maximum = 10000)
{
    if (duration <= 0 || maximum <= 0 || position <= 0) return 0;
    if (position >= duration) return maximum;
    return static_cast<int>((static_cast<long double>(position) * maximum) / duration + 0.5L);
}

inline int64_t MayaVideoSeekPosition(int64_t duration, int slider, int maximum = 10000)
{
    if (duration <= 0 || maximum <= 0) return 0;
    slider = std::clamp(slider, 0, maximum);
    return static_cast<int64_t>((static_cast<long double>(duration) * slider) / maximum);
}

inline bool MayaInlineVideoVisible(int x, int y, int width, int height, int parentWidth, int parentHeight) noexcept
{
    return width > 0 && height > 0 && x < parentWidth && y < parentHeight &&
           static_cast<int64_t>(x) + width > 0 && static_cast<int64_t>(y) + height > 0;
}

#pragma once

inline bool ChatScrollAtBottom(int minimum, int maximum, int position, unsigned page)
{
    if (maximum <= minimum) return true;
    if (page == 0) return position >= maximum;
    return position >= maximum - static_cast<int>(page) + 1;
}

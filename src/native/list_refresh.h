#pragma once

#include <string>
#include <vector>

inline bool NativeListContentsChanged(const std::vector<std::wstring> &current,
                                     const std::vector<std::wstring> &updated)
{
    return current != updated;
}

inline bool NativeTextContentsChanged(const std::wstring &current, const std::wstring &updated)
{
    return current != updated;
}

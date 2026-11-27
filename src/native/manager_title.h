#pragma once
#include <string>

inline std::wstring NativeManagerTitle(const std::wstring &version, bool images, long long maxContext)
{
    const auto context = maxContext > 0 ? std::to_wstring(maxContext / 1000) + L"K Context" : L"Context unavailable";
    return L"Strata Engine | Maya " + version + (images ? L" | Vision ON | " : L" | Vision OFF | ") + context;
}

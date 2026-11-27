#pragma once
#include <algorithm>
#include <initializer_list>
#include <vector>

inline std::vector<int> NativeContextStops(int min, int max, std::initializer_list<int> current = {})
{
    if (max < min) return {};
    constexpr int stops[] = {4096, 8192, 16384, 32768, 49152, 65536, 98304, 131072,
                             196608, 262144, 393216, 524288, 786432, 1048576};
    std::vector<int> result;
    for (const int value : stops) if (value >= min && value <= max) result.push_back(value);
    for (const int value : current) if (value > 0) result.push_back(value);
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

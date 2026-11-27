#pragma once
#include <vector>

struct NativeMessageRange
{
    long start, end;
    int message;
};

inline int NativeMessageAt(const std::vector<NativeMessageRange> &ranges, long character)
{
    for (const auto &range : ranges)
        if (character >= range.start && character < range.end)
            return range.message;
    return -1;
}

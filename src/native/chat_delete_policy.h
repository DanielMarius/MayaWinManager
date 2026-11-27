#pragma once
#include <cstddef>

constexpr bool NativeChatUndoAvailable(std::size_t pendingCount) noexcept
{
    return pendingCount != 0;
}

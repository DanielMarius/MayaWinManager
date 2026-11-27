#pragma once
#include <filesystem>
#include <cstddef>
#include <string>

namespace fs = std::filesystem;

enum class NativeClearDataResult { API_KEY_FAILED, CHAT_DATA_FAILED, COMPLETE };

template <class ClearApiKey, class MoveChatData>
inline NativeClearDataResult NativeClearLocalData(ClearApiKey clearApiKey, MoveChatData moveChatData)
{
    if (!clearApiKey()) return NativeClearDataResult::API_KEY_FAILED;
    if (!moveChatData()) return NativeClearDataResult::CHAT_DATA_FAILED;
    return NativeClearDataResult::COMPLETE;
}

inline bool IsNativeChatDataPath(const fs::path &userData, const fs::path &target)
{
    return target.lexically_normal() == (userData / L"MayaXS" / L"WinManager" / L"chats").lexically_normal();
}

inline std::wstring NativeDataPageText(size_t chats, size_t folders, const std::wstring &path)
{
    return L"Stored on this PC: " + std::to_wstring(chats) + L" chats and " + std::to_wstring(folders) + L" folders.\r\n\r\n" +
           L"Location: " + path + L"\r\n\r\n" +
           L"Clearing moves native chats, folders, chat preferences, and attached copies to the Recycle Bin. This manager's theme and colors are kept, and its sidebar returns to open. Maya's model, engine settings, and profile API keys are kept; the local API-key override is cleared. Attached originals remain in their source locations.";
}

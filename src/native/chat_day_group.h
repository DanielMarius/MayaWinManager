#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

inline std::wstring NativeChatDayGroup(const FILETIME &updatedUtc, const SYSTEMTIME &now)
{
    SYSTEMTIME midnight = now, utcMidnight{}, date{};
    midnight.wHour = midnight.wMinute = midnight.wSecond = midnight.wMilliseconds = 0;
    FILETIME startUtc{}, local{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &midnight, &utcMidnight) ||
        !SystemTimeToFileTime(&utcMidnight, &startUtc) ||
        !FileTimeToLocalFileTime(&updatedUtc, &local) || !FileTimeToSystemTime(&local, &date))
        return L"Older chats";
    ULARGE_INTEGER updated{}, start{};
    updated.HighPart = updatedUtc.dwHighDateTime; updated.LowPart = updatedUtc.dwLowDateTime;
    start.HighPart = startUtc.dwHighDateTime; start.LowPart = startUtc.dwLowDateTime;
    constexpr ULONGLONG day = 10'000'000ULL * 86400ULL;
    if (updated.QuadPart >= start.QuadPart) return L"Today";
    const auto age = start.QuadPart - updated.QuadPart;
    if (age <= day) return L"Yesterday";
    if (age <= 7 * day) return L"Previous 7 days";
    if (age <= 30 * day) return L"Previous 30 days";
    wchar_t text[80]{};
    const wchar_t *format = date.wYear == now.wYear ? L"MMMM" : L"MMMM yyyy";
    const int length = GetDateFormatW(LOCALE_USER_DEFAULT, 0, &date, format, text, _countof(text));
    return length > 1 ? std::wstring(text, length - 1) : L"Older chats";
}

inline std::wstring NativeChatDayGroup(const std::filesystem::path &history)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    SYSTEMTIME now{};
    if (!GetFileAttributesExW(history.c_str(), GetFileExInfoStandard, &attributes))
        return L"Older chats";
    GetLocalTime(&now);
    return NativeChatDayGroup(attributes.ftLastWriteTime, now);
}

inline std::wstring NativeChatRelativeTime(const FILETIME &updatedUtc, const SYSTEMTIME &now)
{
    FILETIME local{};
    SYSTEMTIME date{};
    if (!FileTimeToLocalFileTime(&updatedUtc, &local) || !FileTimeToSystemTime(&local, &date))
        return {};
    SYSTEMTIME chatDay = date, today = now;
    chatDay.wHour = chatDay.wMinute = chatDay.wSecond = chatDay.wMilliseconds = 0;
    today.wHour = today.wMinute = today.wSecond = today.wMilliseconds = 0;
    FILETIME chatDayFile{}, todayFile{};
    if (!SystemTimeToFileTime(&chatDay, &chatDayFile) || !SystemTimeToFileTime(&today, &todayFile))
        return {};
    ULARGE_INTEGER chatTicks{}, todayTicks{};
    chatTicks.HighPart = chatDayFile.dwHighDateTime; chatTicks.LowPart = chatDayFile.dwLowDateTime;
    todayTicks.HighPart = todayFile.dwHighDateTime; todayTicks.LowPart = todayFile.dwLowDateTime;
    constexpr ULONGLONG day = 10'000'000ULL * 86400ULL;
    const ULONGLONG age = todayTicks.QuadPart >= chatTicks.QuadPart ? todayTicks.QuadPart - chatTicks.QuadPart : 0;
    if (age < day)
    {
        wchar_t text[80]{};
        const int length = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &date, nullptr, text, _countof(text));
        return length > 1 ? std::wstring(text, length - 1) : std::wstring();
    }
    if (age < 2 * day) return L"Yesterday";
    wchar_t text[80]{};
    const int length = GetDateFormatW(LOCALE_USER_DEFAULT, 0, &date, L"MMM d", text, _countof(text));
    return length > 1 ? std::wstring(text, length - 1) : std::wstring();
}

inline std::wstring NativeChatRelativeTime(const std::filesystem::path &history, const SYSTEMTIME &now)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExW(history.c_str(), GetFileExInfoStandard, &attributes)) return {};
    return NativeChatRelativeTime(attributes.ftLastWriteTime, now);
}

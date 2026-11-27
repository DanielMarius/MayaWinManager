#pragma once

#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>

inline bool NativeEditTextIsHighSurrogate(wchar_t c) noexcept
{
    return c >= 0xD800 && c <= 0xDBFF;
}

inline bool NativeEditTextIsLowSurrogate(wchar_t c) noexcept
{
    return c >= 0xDC00 && c <= 0xDFFF;
}

inline void NativeSetEditTextPreservingView(HWND edit, const std::wstring &updated)
{
    if (!edit) return;
    const int length = GetWindowTextLengthW(edit);
    std::wstring current((size_t)length + 1, L'\0');
    GetWindowTextW(edit, current.data(), length + 1);
    current.resize((size_t)length);
    if (current == updated) return;

    DWORD selectionStart = 0, selectionEnd = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&selectionStart, (LPARAM)&selectionEnd);
    const int firstVisibleLine = (int)SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0);
    const LRESULT firstVisibleAnchor = SendMessageW(edit, EM_LINEINDEX, firstVisibleLine, 0);

    const auto lineStarts = [](const std::wstring &text) {
        std::vector<size_t> starts{0};
        for (size_t i = 0; i < text.size(); ++i)
            if (text[i] == L'\n') starts.push_back(i + 1);
        return starts;
    };
    const auto oldLines = lineStarts(current), newLines = lineStarts(updated);
    struct LineChange { size_t oldStart, oldEnd, newStart, newEnd; int oldRows; };
    std::vector<LineChange> changes;
    bool canUpdateRows = oldLines.size() == newLines.size();
    if (canUpdateRows)
    {
        for (size_t i = 0; i < oldLines.size(); ++i)
        {
            const size_t oldStart = oldLines[i], oldEnd = i + 1 < oldLines.size() ? oldLines[i + 1] : current.size();
            const size_t newStart = newLines[i], newEnd = i + 1 < newLines.size() ? newLines[i + 1] : updated.size();
            if (current.compare(oldStart, oldEnd - oldStart, updated, newStart, newEnd - newStart) == 0) continue;
            if (oldStart == oldEnd || newStart == newEnd) { canUpdateRows = false; break; }
            const int firstRow = (int)SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)oldStart, 0);
            const int lastRow = (int)SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)(oldEnd - 1), 0);
            if (firstRow < 0 || lastRow < firstRow) { canUpdateRows = false; break; }
            changes.push_back({oldStart, oldEnd, newStart, newEnd, lastRow - firstRow + 1});
        }
    }
    if (canUpdateRows && !changes.empty())
    {
        RECT client{};
        GetClientRect(edit, &client);
        const bool redraw = IsWindowVisible(edit) != FALSE;
        if (redraw) SendMessageW(edit, WM_SETREDRAW, FALSE, 0);
        for (auto change = changes.rbegin(); change != changes.rend(); ++change)
        {
            const std::wstring replacement = updated.substr(change->newStart, change->newEnd - change->newStart);
            SendMessageW(edit, EM_SETSEL, (WPARAM)change->oldStart, (LPARAM)change->oldEnd);
            SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)replacement.c_str());
        }
        const auto mapPosition = [&](DWORD position) {
            const size_t original = position;
            long long delta = 0;
            for (const auto &change : changes)
            {
                if (original < change.oldStart) break;
                if (original < change.oldEnd)
                    return change.newStart + std::min(original - change.oldStart, change.newEnd - change.newStart);
                delta += (long long)(change.newEnd - change.newStart) - (long long)(change.oldEnd - change.oldStart);
            }
            return (size_t)std::max<long long>(0, (long long)original + delta);
        };
        const size_t updatedLength = (size_t)std::max(0, GetWindowTextLengthW(edit));
        SendMessageW(edit, EM_SETSEL, (WPARAM)std::min(mapPosition(selectionStart), updatedLength),
                     (LPARAM)std::min(mapPosition(selectionEnd), updatedLength));
        const size_t mappedAnchor = mapPosition((DWORD)std::max<LRESULT>(0, firstVisibleAnchor));
        const LRESULT mappedLine = firstVisibleAnchor >= 0
            ? SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)std::min(mappedAnchor, updatedLength), 0)
            : -1;
        const int targetLine = mappedLine >= 0 ? (int)mappedLine : firstVisibleLine;
        SendMessageW(edit, EM_LINESCROLL, 0,
                     targetLine - (int)SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));

        bool sameLayout = true;
        for (const auto &change : changes)
        {
            const int firstRow = (int)SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)change.newStart, 0);
            const int lastRow = (int)SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)(change.newEnd - 1), 0);
            if (firstRow < 0 || lastRow < firstRow || lastRow - firstRow + 1 != change.oldRows)
            {
                sameLayout = false;
                break;
            }
        }
        if (redraw)
        {
            SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
            ValidateRect(edit, nullptr);
            if (sameLayout)
            {
                HDC dc = GetDC(edit);
                int lineHeight = 16;
                if (dc)
                {
                    const auto font = (HFONT)SendMessageW(edit, WM_GETFONT, 0, 0);
                    const auto previousFont = font ? SelectObject(dc, font) : nullptr;
                    TEXTMETRICW metrics{};
                    if (GetTextMetricsW(dc, &metrics)) lineHeight = std::max<int>(1, (int)(metrics.tmHeight + metrics.tmExternalLeading));
                    if (previousFont && previousFont != HGDI_ERROR) SelectObject(dc, previousFont);
                    ReleaseDC(edit, dc);
                }
                for (const auto &change : changes)
                {
                    const LRESULT start = SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)change.newStart, 0);
                    if (start == -1) continue;
                    const int top = (short)HIWORD(start);
                    const LRESULT end = SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)change.newEnd, 0);
                    int bottom = end == -1 ? top + lineHeight : (short)HIWORD(end);
                    if (bottom <= top)
                    {
                        const size_t lastChar = change.newEnd > change.newStart ? change.newEnd - 1 : change.newStart;
                        const LRESULT last = SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)lastChar, 0);
                        bottom = last == -1 ? top + lineHeight : (short)HIWORD(last) + lineHeight;
                    }
                    if (bottom <= 0 || top >= client.bottom) continue;
                    const RECT dirty{0, std::max(0, top - 1), client.right, std::min<LONG>(client.bottom, bottom + 1)};
                    if (dirty.bottom > dirty.top) InvalidateRect(edit, &dirty, FALSE);
                }
            }
            else
            {
                const LRESULT start = SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)changes.front().newStart, 0);
                if (start == -1) InvalidateRect(edit, nullptr, FALSE);
                else
                {
                    const int top = (short)HIWORD(start);
                    if (top < client.bottom)
                    {
                        const RECT dirty{0, std::max(0, top - 1), client.right, client.bottom};
                        InvalidateRect(edit, &dirty, FALSE);
                    }
                }
            }
        }
        return;
    }

    size_t prefix = 0;
    const size_t common = std::min(current.size(), updated.size());
    while (prefix < common && current[prefix] == updated[prefix]) ++prefix;
    if (prefix && prefix < common)
    {
        if (current[prefix - 1] == L'\r' && current[prefix] == L'\n') --prefix;
        else if (NativeEditTextIsHighSurrogate(current[prefix - 1]) &&
                 NativeEditTextIsLowSurrogate(current[prefix])) --prefix;
    }

    size_t suffix = 0;
    while (suffix < current.size() - prefix && suffix < updated.size() - prefix &&
           current[current.size() - suffix - 1] == updated[updated.size() - suffix - 1]) ++suffix;
    const size_t oldEnd = current.size() - suffix;
    const size_t newEnd = updated.size() - suffix;
    if (suffix && oldEnd > prefix && newEnd > prefix)
    {
        if (current[oldEnd] == L'\n' && updated[newEnd] == L'\n' &&
            current[oldEnd - 1] == L'\r' && updated[newEnd - 1] == L'\r') --suffix;
        else if (NativeEditTextIsLowSurrogate(current[oldEnd]) &&
                 NativeEditTextIsLowSurrogate(updated[newEnd]) &&
                 NativeEditTextIsHighSurrogate(current[oldEnd - 1]) &&
                 NativeEditTextIsHighSurrogate(updated[newEnd - 1])) --suffix;
    }

    const size_t replaceOldEnd = current.size() - suffix;
    const size_t replaceNewEnd = updated.size() - suffix;
    const std::wstring replacement = updated.substr(prefix, replaceNewEnd - prefix);
    const LRESULT prefixPosition = SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)prefix, 0);
    const int prefixX = (short)LOWORD(prefixPosition);
    const int prefixY = (short)HIWORD(prefixPosition);
    RECT client{};
    GetClientRect(edit, &client);
    const bool prefixVisible = prefixX >= 0 && prefixY >= 0 &&
                               prefixX < client.right && prefixY < client.bottom;
    const bool redraw = IsWindowVisible(edit) != FALSE;
    if (redraw) SendMessageW(edit, WM_SETREDRAW, FALSE, 0);
    SendMessageW(edit, EM_SETSEL, (WPARAM)prefix, (LPARAM)replaceOldEnd);
    SendMessageW(edit, EM_REPLACESEL, FALSE, (LPARAM)replacement.c_str());

    const auto mapPosition = [&](DWORD position) {
        const size_t p = std::min<size_t>(position, current.size());
        if (p <= prefix) return p;
        if (p >= replaceOldEnd) return replaceNewEnd + (p - replaceOldEnd);
        return prefix + std::min(p - prefix, replaceNewEnd - prefix);
    };
    const size_t updatedLength = (size_t)std::max(0, GetWindowTextLengthW(edit));
    SendMessageW(edit, EM_SETSEL,
                 (WPARAM)std::min(mapPosition(selectionStart), updatedLength),
                 (LPARAM)std::min(mapPosition(selectionEnd), updatedLength));
    const size_t mappedAnchor = mapPosition((DWORD)std::max<LRESULT>(0, firstVisibleAnchor));
    const LRESULT mappedLine = firstVisibleAnchor >= 0
        ? SendMessageW(edit, EM_LINEFROMCHAR, (WPARAM)std::min(mappedAnchor, updatedLength), 0)
        : -1;
    const int targetLine = mappedLine >= 0 ? (int)mappedLine : firstVisibleLine;
    SendMessageW(edit, EM_LINESCROLL, 0,
                 targetLine - (int)SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0));
    if (redraw)
    {
        SendMessageW(edit, WM_SETREDRAW, TRUE, 0);
        ValidateRect(edit, nullptr);
        if (prefixVisible)
        {
            const RECT dirty{std::max(0, prefixX - 1), std::max(0, prefixY - 2), client.right, client.bottom};
            InvalidateRect(edit, &dirty, FALSE);
        }
        else
            InvalidateRect(edit, nullptr, FALSE);
    }
}

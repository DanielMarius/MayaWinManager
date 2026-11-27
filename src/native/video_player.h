#pragma once

#include <windows.h>
#include <commctrl.h>
#include <mfapi.h>
#include <mfplay.h>
#include <climits>
#include <cwchar>
#include <filesystem>
#include <new>
#include <string>
#include "video_player_policy.h"

namespace maya_video
{
constexpr UINT_PTR TIMER_ID = 1;
constexpr int ID_PLAY = 4101;
constexpr int ID_SEEK = 4102;
constexpr int ID_VOLUME = 4103;
constexpr int ID_TIME = 4104;
constexpr int ID_STATUS = 4105;
constexpr int SEEK_MAX = 10000;

struct State
{
    std::wstring path;
    HWND window = nullptr, surface = nullptr, play = nullptr, seek = nullptr;
    HWND volume = nullptr, time = nullptr, status = nullptr;
    IMFPMediaPlayer *player = nullptr;
    IMFPMediaPlayerCallback *callback = nullptr;
    LONGLONG duration = 0;
    LONG anchor = 0;
    bool mediaFoundationStarted = false, createReturned = false, seeking = false;
    bool inlinePlayer = false;
};

inline HWND &InlineWindowHandle()
{
    static HWND window = nullptr;
    return window;
}

inline State *&InlineStateHandle()
{
    static State *state = nullptr;
    return state;
}

inline void UpdateControls(State *state);

struct PlayerCallback final : IMFPMediaPlayerCallback
{
    explicit PlayerCallback(State *value) : state(value) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, __uuidof(IMFPMediaPlayerCallback)))
        {
            *object = static_cast<IMFPMediaPlayerCallback *>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&references)); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const auto remaining = static_cast<ULONG>(InterlockedDecrement(&references));
        if (!remaining) delete this;
        return remaining;
    }
    void STDMETHODCALLTYPE OnMediaPlayerEvent(MFP_EVENT_HEADER *event) override
    {
        if (!state || !state->status || !IsWindow(state->status) || !event) return;
        if (FAILED(event->hrEvent))
        {
            wchar_t message[96]{};
            swprintf_s(message, L"Video playback failed (0x%08X).", static_cast<unsigned int>(event->hrEvent));
            SetWindowTextW(state->status, message);
            return;
        }
        if (event->eEventType == MFP_EVENT_TYPE_ERROR)
        {
            SetWindowTextW(state->status, L"Video playback failed.");
            return;
        }
        if (const auto *message = MayaVideoEventStatus(event->eEventType))
            SetWindowTextW(state->status, message);
        UpdateControls(state);
    }

    State *state = nullptr;
    volatile LONG references = 1;
};

inline LONGLONG Ticks(const PROPVARIANT &value)
{
    if (value.vt == VT_I8) return value.hVal.QuadPart;
    if (value.vt == VT_UI8 && value.uhVal.QuadPart <= static_cast<ULONGLONG>(LLONG_MAX))
        return static_cast<LONGLONG>(value.uhVal.QuadPart);
    return 0;
}

inline std::wstring TimeText(LONGLONG ticks)
{
    const ULONGLONG seconds = ticks > 0 ? static_cast<ULONGLONG>(ticks / 10000000LL) : 0;
    const auto hours = seconds / 3600;
    const auto minutes = (seconds / 60) % 60;
    const auto remainder = seconds % 60;
    wchar_t text[32]{};
    if (hours)
        swprintf_s(text, L"%llu:%02llu:%02llu", hours, minutes, remainder);
    else
        swprintf_s(text, L"%02llu:%02llu", seconds / 60, remainder);
    return text;
}

inline void UpdateControls(State *state)
{
    if (!state || !state->player) return;
    PROPVARIANT position{};
    if (SUCCEEDED(state->player->GetPosition(MFP_POSITIONTYPE_100NS, &position)))
    {
        const auto ticks = Ticks(position);
        PropVariantClear(&position);
        if (state->duration <= 0)
        {
            PROPVARIANT duration{};
            if (SUCCEEDED(state->player->GetDuration(MFP_POSITIONTYPE_100NS, &duration)))
            {
                state->duration = Ticks(duration);
                PropVariantClear(&duration);
            }
        }
        if (state->duration > 0 && !state->seeking)
        {
            SendMessageW(state->seek, TBM_SETPOS, TRUE,
                         MayaVideoSeekSlider(state->duration, ticks, SEEK_MAX));
            SetWindowTextW(state->time,
                           (TimeText(ticks) + L" / " + TimeText(state->duration)).c_str());
        }
    }
    MFP_MEDIAPLAYER_STATE playback = MFP_MEDIAPLAYER_STATE_EMPTY;
    if (SUCCEEDED(state->player->GetState(&playback)))
        SetWindowTextW(state->play, playback == MFP_MEDIAPLAYER_STATE_PLAYING ? L"Pause" : L"Play");
}

inline void Layout(State *state, int width, int height)
{
    if (!state) return;
    if (state->inlinePlayer)
    {
        const int dpi = GetDpiForWindow(state->window);
        const int pad = MulDiv(8, dpi, 96);
        const int rowY = std::max(0, height - MulDiv(66, dpi, 96));
        if (width >= MulDiv(300, dpi, 96))
        {
            const int playWidth = MulDiv(58, dpi, 96);
            const int timeWidth = MulDiv(82, dpi, 96);
            const int volumeWidth = std::min(MulDiv(72, dpi, 96), std::max(MulDiv(48, dpi, 96), width / 5));
            const int seekX = pad + playWidth + MulDiv(4, dpi, 96);
            const int seekWidth = std::max(MulDiv(40, dpi, 96), width - seekX - timeWidth - volumeWidth - MulDiv(16, dpi, 96));
            const int timeX = seekX + seekWidth + MulDiv(4, dpi, 96);
            const int volumeX = timeX + timeWidth + MulDiv(4, dpi, 96);
            if (state->surface) MoveWindow(state->surface, 0, 0, width, rowY, TRUE);
            if (state->play) MoveWindow(state->play, pad, rowY + MulDiv(4, dpi, 96), playWidth, MulDiv(28, dpi, 96), TRUE);
            if (state->seek) MoveWindow(state->seek, seekX, rowY, seekWidth, MulDiv(36, dpi, 96), TRUE);
            if (state->time) MoveWindow(state->time, timeX, rowY + MulDiv(8, dpi, 96), timeWidth, MulDiv(22, dpi, 96), TRUE);
            if (state->volume) MoveWindow(state->volume, volumeX, rowY, volumeWidth, MulDiv(36, dpi, 96), TRUE);
            if (state->status) MoveWindow(state->status, pad, rowY + MulDiv(40, dpi, 96), std::max(0, width - 2 * pad), MulDiv(20, dpi, 96), TRUE);
        }
        else
        {
            const int controlsTop = std::max(0, height - MulDiv(106, dpi, 96));
            if (state->surface) MoveWindow(state->surface, 0, 0, width, controlsTop, TRUE);
            if (state->play) MoveWindow(state->play, pad, controlsTop + MulDiv(2, dpi, 96), MulDiv(52, dpi, 96), MulDiv(26, dpi, 96), TRUE);
            if (state->time) MoveWindow(state->time, pad + MulDiv(56, dpi, 96), controlsTop + MulDiv(5, dpi, 96), MulDiv(80, dpi, 96), MulDiv(20, dpi, 96), TRUE);
            if (state->seek) MoveWindow(state->seek, pad, controlsTop + MulDiv(30, dpi, 96), std::max(0, width - 2 * pad), MulDiv(30, dpi, 96), TRUE);
            if (state->volume) MoveWindow(state->volume, pad, controlsTop + MulDiv(58, dpi, 96), std::max(0, width - 2 * pad), MulDiv(30, dpi, 96), TRUE);
            if (state->status) MoveWindow(state->status, pad, controlsTop + MulDiv(84, dpi, 96), std::max(0, width - 2 * pad), MulDiv(20, dpi, 96), TRUE);
        }
        return;
    }
    const int barTop = std::max(0, height - 76);
    const int rowY = barTop + 5;
    const int timeX = std::max(184, width - 224);
    const int volumeX = std::max(timeX + 116, width - 108);
    if (state->surface) MoveWindow(state->surface, 0, 0, width, barTop, TRUE);
    if (state->play) MoveWindow(state->play, 10, rowY + 4, 72, 28, TRUE);
    if (state->seek) MoveWindow(state->seek, 86, rowY, std::max(50, timeX - 94), 36, TRUE);
    if (state->time) MoveWindow(state->time, timeX, rowY + 8, 116, 22, TRUE);
    if (state->volume) MoveWindow(state->volume, volumeX, rowY, std::max(52, width - volumeX - 10), 36, TRUE);
    if (state->status) MoveWindow(state->status, 12, barTop + 44, std::max(0, width - 24), 22, TRUE);
}

inline LRESULT CALLBACK SurfaceProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<State *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        state = static_cast<State *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (state) state->surface = window;
    }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (state && state->player)
            state->player->UpdateVideo();
        else
            FillRect(dc, &paint.rcPaint, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_SIZE && state && state->player)
        state->player->UpdateVideo();
    if (message == WM_NCDESTROY) SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return DefWindowProcW(window, message, wParam, lParam);
}

inline LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto *state = reinterpret_cast<State *>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto *create = reinterpret_cast<const CREATESTRUCTW *>(lParam);
        state = static_cast<State *>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        if (state) state->window = window;
    }
    switch (message)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_BAR_CLASSES};
        InitCommonControlsEx(&controls);
        state->surface = CreateWindowExW(0, L"MayaNativeVideoSurface", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                                         0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), state);
        state->play = CreateWindowExW(0, L"BUTTON", L"Pause", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PLAY)), GetModuleHandleW(nullptr), nullptr);
        state->seek = CreateWindowExW(0, TRACKBAR_CLASSW, L"Video playback position",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SEEK)), GetModuleHandleW(nullptr), nullptr);
        state->time = CreateWindowExW(0, L"STATIC", L"00:00 / 00:00", WS_CHILD | WS_VISIBLE | SS_CENTER,
                                      0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TIME)), GetModuleHandleW(nullptr), nullptr);
        state->volume = CreateWindowExW(0, TRACKBAR_CLASSW, L"Volume",
                                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
                                        0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_VOLUME)), GetModuleHandleW(nullptr), nullptr);
        state->status = CreateWindowExW(0, L"STATIC", L"Loading video…", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                        0, 0, 0, 0, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_STATUS)), GetModuleHandleW(nullptr), nullptr);
        if (!state->surface || !state->play || !state->seek || !state->time || !state->volume || !state->status)
            return -1;
        SendMessageW(state->seek, TBM_SETRANGE, TRUE, MAKELONG(0, SEEK_MAX));
        SendMessageW(state->volume, TBM_SETRANGE, TRUE, MAKELONG(0, 100));
        HRESULT hr = MFStartup(MF_VERSION);
        if (SUCCEEDED(hr))
        {
            state->mediaFoundationStarted = true;
            state->callback = new (std::nothrow) PlayerCallback(state);
            hr = state->callback
                ? MFPCreateMediaPlayer(state->path.c_str(), TRUE, 0, state->callback, state->surface, &state->player)
                : E_OUTOFMEMORY;
        }
        if (SUCCEEDED(hr) && state->player)
        {
            float volume = 1.0f;
            state->player->GetVolume(&volume);
            SendMessageW(state->volume, TBM_SETPOS, TRUE, static_cast<LPARAM>(volume * 100.0f));
            SetWindowTextW(state->status, L"Loading video…");
            SetTimer(window, TIMER_ID, 250, nullptr);
        }
        else
            SetWindowTextW(state->status, L"Could not open this video.");
        RECT client{};
        GetClientRect(window, &client);
        Layout(state, client.right, client.bottom);
        return 0;
    }
    case WM_GETMINMAXINFO:
    {
        if (state->inlinePlayer) return 0;
        auto *info = reinterpret_cast<MINMAXINFO *>(lParam);
        info->ptMinTrackSize.x = 560;
        info->ptMinTrackSize.y = 360;
        return 0;
    }
    case WM_SIZE:
        Layout(state, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_PLAY && state->player)
        {
            MFP_MEDIAPLAYER_STATE playback = MFP_MEDIAPLAYER_STATE_EMPTY;
            if (SUCCEEDED(state->player->GetState(&playback)) && playback == MFP_MEDIAPLAYER_STATE_PLAYING)
                state->player->Pause();
            else
                state->player->Play();
            UpdateControls(state);
            return 0;
        }
        break;
    case WM_HSCROLL:
        if (state->player && reinterpret_cast<HWND>(lParam) == state->seek && state->duration > 0)
        {
            const auto code = LOWORD(wParam);
            if (code == TB_THUMBTRACK)
            {
                state->seeking = true;
                return 0;
            }
            if (code != TB_THUMBPOSITION && code != TB_ENDTRACK && code != TB_LINEUP && code != TB_LINEDOWN &&
                code != TB_PAGEUP && code != TB_PAGEDOWN && code != SB_ENDSCROLL) break;
            state->seeking = false;
            PROPVARIANT position{};
            position.vt = VT_I8;
            position.hVal.QuadPart = MayaVideoSeekPosition(state->duration,
                static_cast<int>(SendMessageW(state->seek, TBM_GETPOS, 0, 0)), SEEK_MAX);
            state->player->SetPosition(MFP_POSITIONTYPE_100NS, &position);
            UpdateControls(state);
            return 0;
        }
        if (state->player && reinterpret_cast<HWND>(lParam) == state->volume)
        {
            const int volume = static_cast<int>(SendMessageW(state->volume, TBM_GETPOS, 0, 0));
            state->player->SetVolume(volume / 100.0f);
            return 0;
        }
        break;
    case WM_TIMER:
        if (wParam == TIMER_ID) UpdateControls(state);
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        KillTimer(window, TIMER_ID);
        if (state->inlinePlayer && InlineWindowHandle() == window)
        {
            InlineWindowHandle() = nullptr;
            InlineStateHandle() = nullptr;
        }
        if (state->player)
        {
            state->player->Shutdown();
            state->player->Release();
            state->player = nullptr;
        }
        if (state->callback)
        {
            state->callback->Release();
            state->callback = nullptr;
        }
        if (state->mediaFoundationStarted)
        {
            MFShutdown();
            state->mediaFoundationStarted = false;
        }
        return 0;
    case WM_NCDESTROY:
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        const bool destroyState = state->createReturned;
        const auto result = DefWindowProcW(window, message, wParam, lParam);
        if (destroyState) delete state;
        return result;
    }
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

inline bool RegisterClasses(HINSTANCE instance)
{
    WNDCLASSEXW surface{sizeof(surface)};
    surface.lpfnWndProc = SurfaceProc;
    surface.hInstance = instance;
    surface.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    surface.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    surface.lpszClassName = L"MayaNativeVideoSurface";
    if (!RegisterClassExW(&surface) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    WNDCLASSEXW player{sizeof(player)};
    player.lpfnWndProc = WindowProc;
    player.hInstance = instance;
    player.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    player.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    player.lpszClassName = L"MayaNativeVideoPlayer";
    return RegisterClassExW(&player) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

inline bool IsInlineOpenFor(HWND parent, const std::filesystem::path &path)
{
    const auto window = InlineWindowHandle();
    const auto *state = InlineStateHandle();
    return window && IsWindow(window) && state && state->window == window && GetParent(window) == parent &&
           state->path == path.native();
}

inline void CloseInline()
{
    const auto window = InlineWindowHandle();
    if (window && IsWindow(window)) DestroyWindow(window);
    InlineWindowHandle() = nullptr;
    InlineStateHandle() = nullptr;
}

inline void RepositionInline(HWND parent)
{
    const auto window = InlineWindowHandle();
    auto *state = InlineStateHandle();
    if (!window || !IsWindow(window) || !state || state->window != window || GetParent(window) != parent) return;
    RECT client{};
    GetClientRect(parent, &client);
    POINTL point{};
    SendMessageW(parent, EM_POSFROMCHAR, reinterpret_cast<WPARAM>(&point), state->anchor);
    const int dpi = GetDpiForWindow(parent);
    const int x = std::max(MulDiv(4, dpi, 96), static_cast<int>(point.x));
    const int y = static_cast<int>(point.y) + MulDiv(20, dpi, 96);
    const int width = std::min(MulDiv(360, dpi, 96), static_cast<int>(client.right) - x - MulDiv(4, dpi, 96));
    const int height = MulDiv(264, dpi, 96);
    const bool visible = point.x >= 0 && point.y >= 0 &&
                         MayaInlineVideoVisible(x, y, width, height, client.right, client.bottom);
    if (!visible)
    {
        ShowWindow(window, SW_HIDE);
        return;
    }
    SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

inline bool UpdateInlineAttachment(HWND parent, const std::filesystem::path &path, LONG anchor)
{
    auto *state = InlineStateHandle();
    if (!state || !IsInlineOpenFor(parent, path)) return false;
    state->anchor = anchor;
    RepositionInline(parent);
    return true;
}

inline bool OpenInline(HWND parent, const std::filesystem::path &path, LONG anchor)
{
    if (!parent || !IsWindow(parent) || path.empty()) return false;
    CloseInline();
    const auto instance = GetModuleHandleW(nullptr);
    if (!RegisterClasses(instance)) return false;
    State *state = new (std::nothrow) State;
    if (!state) return false;
    try
    {
        state->path = path.native();
    }
    catch (...)
    {
        delete state;
        return false;
    }
    state->inlinePlayer = true;
    state->anchor = anchor;
    HWND window = CreateWindowExW(0, L"MayaNativeVideoPlayer", L"",
                                  WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                  0, 0, 1, 1, parent, nullptr, instance, state);
    if (!window)
    {
        delete state;
        return false;
    }
    state->createReturned = true;
    InlineWindowHandle() = window;
    InlineStateHandle() = state;
    RepositionInline(parent);
    UpdateWindow(window);
    return true;
}

// shortcut: Uses legacy MFPlay and installed Windows codecs; replace if Windows retires it or codec coverage is insufficient.
inline bool Open(HWND owner, const std::filesystem::path &path)
{
    const auto instance = GetModuleHandleW(nullptr);
    if (!RegisterClasses(instance)) return false;
    State *state = nullptr;
    try
    {
        state = new (std::nothrow) State;
        if (!state) return false;
        state->path = path.wstring();
    }
    catch (...)
    {
        delete state;
        return false;
    }
    const auto title = L"Maya video — " + path.filename().wstring();
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, L"MayaNativeVideoPlayer", title.c_str(),
                                  WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 960, 600,
                                  owner, nullptr, instance, state);
    if (!window)
    {
        delete state;
        return false;
    }
    state->createReturned = true;
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetForegroundWindow(window);
    return true;
}
}

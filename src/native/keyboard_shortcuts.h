#pragma once

enum class NativeShortcut { NONE, SEND, NEWLINE, NEW_CHAT, SEARCH, SETTINGS, TOGGLE_SIDEBAR, ESCAPE };
enum class NativeEscapeAction { NONE, CLOSE_SETTINGS, CLEAR_SEARCH, CANCEL_EDIT, STOP_CHAT };

constexpr bool NativeEnterMaySubmit(bool imeComposing, bool coarsePointer = false) noexcept
{
    return !imeComposing && !coarsePointer;
}

constexpr bool NativePageFocusMustReset(bool belongsToMainWindow, bool visible, bool enabled) noexcept
{
    return belongsToMainWindow && (!visible || !enabled);
}

constexpr bool NativeSearchNeedsChatPage(bool chatPage) noexcept { return !chatPage; }
constexpr bool NativeSearchShouldClose(bool searchFocused) noexcept { return searchFocused; }
constexpr bool NativeEscapeShouldYieldToPopup(bool popupOpen) noexcept { return popupOpen; }
constexpr bool NativeAltShortcutMayContinue(unsigned key, bool composerFocused) noexcept
{
    return key == 13 && composerFocused;
}

inline NativeEscapeAction NativeEscapeForState(bool settingsGroupOpen, bool chatPage, bool searchFocused,
                                               bool searchHasQuery, bool editing, bool chatBusy)
{
    if (settingsGroupOpen) return NativeEscapeAction::CLOSE_SETTINGS;
    if (!chatPage) return NativeEscapeAction::NONE;
    if (searchFocused || searchHasQuery) return NativeEscapeAction::CLEAR_SEARCH;
    if (editing) return NativeEscapeAction::CANCEL_EDIT;
    if (chatBusy) return NativeEscapeAction::STOP_CHAT;
    return NativeEscapeAction::NONE;
}


// Matches the web Thinking slider using Win32 virtual-key values.
constexpr bool NativeThinkingComboUsesCustomNavigation(bool dropdownOpen, bool altDown) noexcept
{
    return !dropdownOpen && !altDown;
}

constexpr int NativeThinkingSelectionAfterKey(int selected, int count, unsigned key) noexcept
{
    if (selected < 0 || selected >= count) return -1;
    if (key == 0x24) return 0; // Home
    if (key == 0x23) return count - 1; // End
    if (key == 0x27 || key == 0x26) return selected + 1 < count ? selected + 1 : count - 1; // Right, Up
    if (key == 0x25 || key == 0x28) return selected > 0 ? selected - 1 : 0; // Left, Down
    return -1;
}

constexpr int NativeSegmentSelectionAfterKey(int selected, int count, unsigned key) noexcept
{
    if (count <= 0) return -1;
    const bool forward = key == 0x27 || key == 0x28; // Right, Down
    const bool backward = key == 0x25 || key == 0x26; // Left, Up
    if (!forward && !backward) return -1;
    if (selected < 0 || selected >= count) return 0;
    if (forward) return selected + 1 == count ? 0 : selected + 1;
    return selected == 0 ? count - 1 : selected - 1;
}

constexpr int NativeModelSettingSelectionAfterKey(int selected, int count, unsigned key, bool segmented) noexcept
{
    return segmented ? NativeSegmentSelectionAfterKey(selected, count, key)
                     : NativeThinkingSelectionAfterKey(selected, count, key);
}

inline NativeShortcut NativeShortcutForKey(unsigned key, bool control, bool shift)
{
    if (key == 27 && !control && !shift) return NativeShortcut::ESCAPE;
    if (key == 13) return shift ? NativeShortcut::NEWLINE : NativeShortcut::SEND;
    if (!control) return NativeShortcut::NONE;
    if (shift && key == 'O') return NativeShortcut::NEW_CHAT;
    if (!shift && key == 'K') return NativeShortcut::SEARCH;
    if (!shift && key == 0xBC) return NativeShortcut::SETTINGS; // VK_OEM_COMMA
    if (shift && key == 'S') return NativeShortcut::TOGGLE_SIDEBAR;
    return NativeShortcut::NONE;
}

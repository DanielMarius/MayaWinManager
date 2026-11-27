#include "accessible_button.h"
#include "chat_menu_policy.h"
#include "chat_sampling.h"
#include "chat_reasoning_control.h"
#include "mcp_tools_control.h"
#include "monitor_report.h"
#include "native_edit_text.h"
#include "native_combo_box.h"
#include "keyboard_shortcuts.h"
#include "model_switch_result.h"
#include "tab_navigation.h"
#include <commctrl.h>
#include <UIAutomation.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>

std::atomic<int> g_accessibleButtonInvocations = 0;
std::atomic<int> g_comboSelectionChanges = 0;
std::atomic<int> g_reasoningButtonClicks = 0;
int g_editRedrawDisabled = 0;
int g_editRedrawEnabled = 0;
int g_editPaints = 0;

LRESULT CALLBACK AccessibleButtonFixtureProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_COMMAND && LOWORD(wParam) == 41 && HIWORD(wParam) == CBN_SELCHANGE)
    {
        ++g_comboSelectionChanges;
        return 0;
    }
    if (message == WM_COMMAND && LOWORD(wParam) == NativeChatReasoningControlId && HIWORD(wParam) == BN_CLICKED)
    {
        ++g_reasoningButtonClicks;
        return 0;
    }
    if (message == WM_COMMAND &&
        (LOWORD(wParam) == NativeGreetingModelButtonId || LOWORD(wParam) == NativeChatModelButtonId) &&
        HIWORD(wParam) == BN_CLICKED)
    {
        const bool hasChat = LOWORD(wParam) == NativeChatModelButtonId;
        if (MayaShouldDispatchButtonCommand(HIWORD(wParam), (HWND)lParam) &&
            NativeChatModelButtonShouldOpen(LOWORD(wParam), true, hasChat, false))
            ++g_accessibleButtonInvocations;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK EditRedrawProbeProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR id, DWORD_PTR)
{
    if (message == WM_SETREDRAW)
    {
        if (wParam) ++g_editRedrawEnabled;
        else ++g_editRedrawDisabled;
    }
    if (message == WM_PAINT) ++g_editPaints;
    if (message == WM_NCDESTROY) RemoveWindowSubclass(hwnd, EditRedrawProbeProc, id);
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

int wmain()
{
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    assert(SUCCEEDED(comResult));

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = AccessibleButtonFixtureProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"MayaAccessibleButtonFixture";
    assert(RegisterClassW(&windowClass) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS);

    HWND parent = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, windowClass.lpszClassName, L"",
                                  WS_OVERLAPPEDWINDOW, -32000, -32000, 300, 150,
                                  nullptr, nullptr, instance, nullptr);
    assert(parent);
    HWND profile = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                  0, 0, 1, 1, parent, (HMENU)(INT_PTR)41, instance, nullptr);
    assert(profile);
    HWND mcpToggle = CreateWindowW(L"BUTTON", L"Allow Maya to call configured MCP tools",
                                   NativeMcpToolsCheckStyle,
                                   0, 0, 90, 24, parent, (HMENU)(INT_PTR)NativeMcpToolsControlId, instance, nullptr);
    assert(mcpToggle);
    for (int i = 0; i < 10; ++i)
        SendMessageW(profile, CB_ADDSTRING, 0, (LPARAM)(i == 0 ? L"DeepSeek V4.1 Flash"
                                                              : i == 1 ? L"DeepSeek V4.1 Flash Abliterated" : L"Profile"));
    const int profileRowHeight = (int)SendMessageW(profile, CB_GETITEMHEIGHT, 0, 0);
    const int profileLayoutHeight = NativeComboLayoutHeight(profile, 20);
    assert(profileRowHeight > 0 && profileLayoutHeight == 20);
    assert(MoveWindow(profile, 0, 0, 90, profileLayoutHeight, FALSE));
    assert(NativeComboLayoutHeight(nullptr, 20) == 20);
    HWND button = CreateWindowExW(0, L"BUTTON", L"Choose model",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                  0, 0, 90, 24, parent, (HMENU)(INT_PTR)NativeChatModelButtonId, instance, nullptr);
    assert(button && AttachMayaAccessibleButton(button));
    assert(NativeComboLayoutHeight(button, 20) == 20);
    HWND disabled = CreateWindowExW(0, L"BUTTON", L"Disabled tab stop",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                    0, 0, 90, 24, parent, (HMENU)(INT_PTR)43, instance, nullptr);
    HWND hidden = CreateWindowExW(0, L"BUTTON", L"Hidden tab stop",
                                  WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                  0, 0, 90, 24, parent, (HMENU)(INT_PTR)44, instance, nullptr);
    HWND last = CreateWindowExW(0, L"BUTTON", L"Last tab stop",
                                WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                0, 0, 90, 24, parent, (HMENU)(INT_PTR)45, instance, nullptr);
    HWND searchLabel = CreateWindowW(L"STATIC", L"Search chats", WS_CHILD, 0, 0, 1, 1,
                                     parent, nullptr, instance, nullptr);
    HWND search = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                  0, 0, 90, 20, parent, (HMENU)(INT_PTR)46, instance, nullptr);
    SendMessageW(search, EM_SETCUEBANNER, FALSE, (LPARAM)L"Search chats");
    HWND chatListLabel = CreateWindowW(L"STATIC", L"Chats", WS_CHILD, 0, 0, 1, 1,
                                       parent, nullptr, instance, nullptr);
    HWND chatList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                  LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 1, 1,
                                  parent, (HMENU)(INT_PTR)47, instance, nullptr);
    HWND modelsLabel = CreateWindowW(L"STATIC", L"INSTALLED MODELS · Enter or double-click for details",
                                     WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, parent, nullptr, instance, nullptr);
    HWND models = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 1, 1,
                                parent, (HMENU)(INT_PTR)48, instance, nullptr);
    HWND catalogLabel = CreateWindowW(L"STATIC", L"MAYA-SUPPORTED DOWNLOADS",
                                      WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, parent, nullptr, instance, nullptr);
    HWND catalog = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                  LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 0, 0, 1, 1,
                                  parent, (HMENU)(INT_PTR)49, instance, nullptr);
    assert(disabled && hidden && last && AttachMayaAccessibleButton(last) && searchLabel && search && chatListLabel && chatList &&
           modelsLabel && models && catalogLabel && catalog);
    assert(MayaShouldDispatchButtonCommand(BN_CLICKED, button));
    assert(!MayaShouldDispatchButtonCommand(BN_DOUBLECLICKED, button));
    assert(!MayaShouldDispatchButtonCommand(BN_SETFOCUS, button));
    assert(!MayaShouldDispatchButtonCommand(BN_KILLFOCUS, button));
    assert(MayaShouldDispatchButtonCommand(CBN_EDITCHANGE, profile));
    assert(MayaShouldDispatchButtonCommand(LBN_DBLCLK, chatList));
    assert(MayaShouldDispatchButtonCommand(0, nullptr));
    EnableWindow(disabled, FALSE);
    ShowWindow(hidden, SW_HIDE);
    ShowWindow(parent, SW_SHOWNOACTIVATE);
    UpdateWindow(parent);
    assert(!NativeComboPopupOpen(parent));
    SendMessageW(profile, CB_SETCURSEL, 0, 0);
    SetFocus(profile);
    assert(GetFocus() == profile);
    SendMessageW(profile, CB_SHOWDROPDOWN, TRUE, 0);
    assert(SendMessageW(profile, CB_GETDROPPEDSTATE, 0, 0));
    assert(NativeComboPopupOpen(parent));
    assert(NativeEscapeShouldYieldToPopup(NativeComboPopupOpen(parent)));
    const int selectionChangesBeforeEscape = g_comboSelectionChanges.load();
    SendMessageW(profile, WM_KEYDOWN, VK_ESCAPE, 0);
    SendMessageW(profile, WM_KEYUP, VK_ESCAPE, 0);
    assert(!NativeComboPopupOpen(parent));
    assert(SendMessageW(profile, CB_GETCURSEL, 0, 0) == 0);
    assert(g_comboSelectionChanges.load() == selectionChangesBeforeEscape);
    SendMessageW(profile, CB_SHOWDROPDOWN, TRUE, 0);
    assert(NativeComboPopupOpen(parent));
    SendMessageW(profile, WM_KEYDOWN, VK_DOWN, 0);
    SendMessageW(profile, WM_KEYUP, VK_DOWN, 0);
    SendMessageW(profile, WM_KEYDOWN, VK_RETURN, 0);
    SendMessageW(profile, WM_KEYUP, VK_RETURN, 0);
    assert(!SendMessageW(profile, CB_GETDROPPEDSTATE, 0, 0));
    assert(!NativeComboPopupOpen(parent));
    assert(!NativeEscapeShouldYieldToPopup(NativeComboPopupOpen(parent)));
    assert(SendMessageW(profile, CB_GETCURSEL, 0, 0) == 1);
    assert(g_comboSelectionChanges > 0);
    wchar_t selectedProfile[64]{};
    SendMessageW(profile, CB_GETLBTEXT, 1, (LPARAM)selectedProfile);
    assert(lstrcmpW(selectedProfile, L"DeepSeek V4.1 Flash Abliterated") == 0);
    SendMessageW(button, BM_CLICK, 0, 0);
    assert(g_accessibleButtonInvocations == 1);
    assert(MayaNextTabStop(parent, nullptr, FALSE) == profile);
    assert(MayaNextTabStop(parent, nullptr, TRUE) == catalog);
    assert(MayaNextTabStop(parent, profile, FALSE) == mcpToggle);
    assert(MayaNextTabStop(parent, mcpToggle, FALSE) == button);
    assert(MayaNextTabStop(parent, profile, TRUE) == catalog);
    assert(MayaNextTabStop(parent, mcpToggle, TRUE) == profile);
    assert(MayaNextTabStop(parent, disabled, FALSE) == profile);
    assert(MayaNextTabStop(parent, disabled, TRUE) == catalog);
    assert(MayaNextTabStop(parent, button, FALSE) == last);
    assert(MayaNextTabStop(parent, last, FALSE) == chatList);
    assert(MayaNextTabStop(parent, chatList, FALSE) == models);
    assert(MayaNextTabStop(parent, chatList, TRUE) == last);
    assert(MayaNextTabStop(parent, button, TRUE) == mcpToggle);
    assert(MayaNextTabStop(parent, last, TRUE) == button);
    SetFocus(mcpToggle);
    assert(GetFocus() == mcpToggle);
    assert(SendMessageW(mcpToggle, BM_GETCHECK, 0, 0) == BST_UNCHECKED);
    SendMessageW(mcpToggle, BM_CLICK, 0, 0);
    assert(SendMessageW(mcpToggle, BM_GETCHECK, 0, 0) == BST_CHECKED);
    SetFocus(button);
    assert(GetFocus() == button);
    const HWND nextFocus = MayaNextTabStop(parent, GetFocus(), FALSE);
    assert(nextFocus == last);
    SetFocus(nextFocus);
    assert(GetFocus() == last);
    ShowWindow(button, SW_HIDE);
    assert(MayaNextTabStop(parent, nullptr, FALSE) == profile);
    assert(MayaNextTabStop(parent, nullptr, TRUE) == catalog);
    assert(MayaNextTabStop(parent, button, FALSE) == profile);
    assert(MayaNextTabStop(parent, button, TRUE) == catalog);
    ShowWindow(button, SW_SHOW);

    HWND greetingModel = CreateWindowExW(0, L"BUTTON", L"Choose model for new chat",
                                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                         0, 30, 180, 24, parent,
                                         (HMENU)(INT_PTR)NativeGreetingModelButtonId, instance, nullptr);
    assert(greetingModel && AttachMayaAccessibleButton(greetingModel));
    SendMessageW(greetingModel, BM_CLICK, 0, 0);
    assert(g_accessibleButtonInvocations == 2);

    HWND reasoningLabel = CreateWindowW(L"STATIC", L"Reasoning:", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                        0, 112, 110, 24, parent, nullptr, instance, nullptr);
    HWND reasoning = CreateWindowExW(0, L"BUTTON", L"Max",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     0, 112, 130, 34, parent,
                                     (HMENU)(INT_PTR)NativeChatReasoningControlId, instance, nullptr);
    assert(reasoningLabel && reasoning);
    assert(AttachMayaAccessibleButton(reasoning));
    const auto reasoningEfforts = ChatSamplingEfforts(ChatSamplingFamily("deepseek-v4.1-flash-abliterated"));
    assert(reasoningEfforts.size() == 4);
    SendMessageW(reasoning, BM_CLICK, 0, 0);
    assert(g_reasoningButtonClicks == 1);
    assert(NativeChatReasoningMenuIndexFor(NativeChatReasoningMenuCommandBase + 3, reasoningEfforts.size()) == 3);
    assert(reasoningEfforts[(size_t)NativeChatReasoningMenuIndexFor(
               NativeChatReasoningMenuCommandBase + 3, reasoningEfforts.size())] == "max");
    assert(NativeChatReasoningMenuIndexFor(NativeChatReasoningMenuCommandBase + 4, reasoningEfforts.size()) == -1);
    std::string chatEffort = "high", chatModel = "previous-model";
    bool failedSaveSawSelection = false;
    assert(!CommitChatReasoningChoice(chatEffort, chatModel, "max", "deepseek-v4.1-flash",
                                      [&] {
                                          failedSaveSawSelection = chatEffort == "max" && chatModel == "deepseek-v4.1-flash";
                                          return false;
                                      }));
    assert(failedSaveSawSelection && chatEffort == "high" && chatModel == "previous-model");
    assert(CommitChatReasoningChoice(chatEffort, chatModel, "low", "deepseek-v4.1-flash",
                                     [&] { return chatEffort == "low" && chatModel == "deepseek-v4.1-flash"; }));
    assert(chatEffort == "low" && chatModel == "deepseek-v4.1-flash");
    const auto miniMaxEfforts = ChatSamplingEfforts(ChatSamplingFamily("minimax-m3.1-flash"));
    assert(miniMaxEfforts.size() == 7 &&
           miniMaxEfforts[(size_t)NativeChatReasoningMenuIndexFor(
               NativeChatReasoningMenuCommandBase + 6, miniMaxEfforts.size())] == "max");
    IUIAutomation *automation = nullptr;
    const HRESULT automationResult = CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                                       IID_PPV_ARGS(&automation));
    assert(SUCCEEDED(automationResult) && automation);
    IUIAutomationElement *modelElement = nullptr;
    assert(SUCCEEDED(automation->ElementFromHandle(greetingModel, &modelElement)));
    IUIAutomationInvokePattern *modelInvoke = nullptr;
    assert(SUCCEEDED(modelElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&modelInvoke))));


    modelInvoke->Release();
    modelElement->Release();
    IUIAutomationElement *reasoningElement = nullptr;
    assert(SUCCEEDED(automation->ElementFromHandle(reasoning, &reasoningElement)));
    CONTROLTYPEID reasoningType = 0;
    assert(SUCCEEDED(reasoningElement->get_CurrentControlType(&reasoningType)) && reasoningType == UIA_ButtonControlTypeId);
    BSTR reasoningName = nullptr;
    assert(SUCCEEDED(reasoningElement->get_CurrentName(&reasoningName)));
    assert(reasoningName && wcscmp(reasoningName, L"Max") == 0);
    SysFreeString(reasoningName);
    IUIAutomationInvokePattern *reasoningInvoke = nullptr;
    assert(SUCCEEDED(reasoningElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&reasoningInvoke))));


    reasoningInvoke->Release();
    reasoningElement->Release();
    automation->Release();
    assert(NativeChatModelMenuActionFor(3) == NativeChatModelMenuAction::MODELS);
    assert(NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, true, true));
    assert(!NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, true, false));
    assert(NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, true, false));
    assert(!NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, true, true));
    assert(NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, false, false));
    assert(!NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, false, false));
    assert(!NativeChatModelButtonShouldOpen(NativeChatModelButtonId, false, false, false));
    const Json savedSampling = {{"deepseek", {{"temperature", 0.3}, {"reasoning_effort", "low"}}},
                                {"qwen", {{"reasoning_effort", "xhigh"}}}};
    const auto changedReasoning = SaveChatReasoningEffort(savedSampling, "deepseek-v4.1-flash", "max");
    assert(changedReasoning["deepseek"]["reasoning_effort"] == "max");
    assert(changedReasoning["deepseek"]["temperature"] == 0.3);
    assert(changedReasoning["qwen"]["reasoning_effort"] == "xhigh");
    const Json switchableModel = {{"ok", true}, {"running", false}};
    assert(ModelSwitchEnabled(switchableModel, false, false));
    assert(!ModelSwitchEnabled(switchableModel, true, false));
    assert(NativeChatModelMenuModelIndexFor(NativeChatModelMenuCommandBase, 2) == 0);
    assert(NativeChatModelMenuModelIndexFor(NativeChatModelMenuCommandBase + 1, 2) == 1);
    assert(NativeChatModelMenuModelIndexFor(NativeChatModelMenuCommandBase + 2, 2) == -1);
    const std::vector<std::string> installedModels{"maya-ds4-abliterated.json", "maya-ds4.json"};
    const int selectedModel = NativeChatModelMenuModelIndexFor(NativeChatModelMenuCommandBase + 1, installedModels.size());
    assert(selectedModel >= 0 && installedModels[(size_t)selectedModel] == "maya-ds4.json");
    assert(!NativeModelSwitchBlockedByWork(false, false, false));
    assert(!NativeModelSwitchBlockedByWork(true, true, false));
    assert(!NativeModelSwitchBlockedByWork(true, false, true));
    assert(NativeModelSwitchBlockedByWork(true, false, false));
    assert(NativeQueuedModelRefreshShouldRun(true, true, true));
    assert(!NativeQueuedModelRefreshShouldRun(false, true, true));
    assert(!NativeQueuedModelRefreshShouldRun(true, false, true));
    assert(!NativeQueuedModelRefreshShouldRun(true, true, false));
    assert(NativeChatModelMenuReopenAfterLoad(true, 0));
    assert(!NativeChatModelMenuReopenAfterLoad(false, 0));
    assert(!NativeChatModelMenuReopenAfterLoad(true, 1));

    const Json beforeMonitor = { {"live", {{"state", "idle"}}}, {"tiers", {{"now", 0}}} };
    const auto unchangedMonitor = MonitorPageRefreshChangesFor(beforeMonitor, beforeMonitor);
    assert(!unchangedMonitor.throughput && !unchangedMonitor.rightNow && !unchangedMonitor.context &&
           !unchangedMonitor.tiers && !unchangedMonitor.gpu && !unchangedMonitor.hardware);
    assert(MonitorPageRefreshRegionsFor(264, 150, 800, 1, false, unchangedMonitor.throughput,
                                        unchangedMonitor.rightNow, unchangedMonitor.context,
                                        unchangedMonitor.tiers, unchangedMonitor.gpu,
                                        unchangedMonitor.hardware).empty());
    const Json liveMonitor = { {"live", {{"state", "reading"}}}, {"tiers", {{"now", 0}}} };
    const auto liveRefresh = MonitorPageRefreshChangesFor(beforeMonitor, liveMonitor);
    assert(liveRefresh.throughput && liveRefresh.rightNow && liveRefresh.context &&
           !liveRefresh.tiers && !liveRefresh.gpu && !liveRefresh.hardware);
    assert(MonitorPageRefreshRegionsFor(264, 150, 800, 1, false, liveRefresh.throughput,
                                        liveRefresh.rightNow, liveRefresh.context,
                                        liveRefresh.tiers, liveRefresh.gpu,
                                        liveRefresh.hardware).size() == 3);

    Json hardwareBefore{{"hardware", {{"cpu", 15}, {"ram_used", 12000}}},
                        {"gpu_map", Json::array({Json{{"used", 1}}})}};
    auto hardwareAfter = hardwareBefore;
    hardwareAfter["hardware"]["cpu"] = 16;
    const auto hardwareRefresh = MonitorPageRefreshChangesFor(hardwareBefore, hardwareAfter);
    assert(!hardwareRefresh.gpu && hardwareRefresh.hardware);
    const auto hardwareRegions = MonitorPageRefreshRegionsFor(
        264, 150, 800, 1, false, false, false, false, false,
        hardwareRefresh.gpu, hardwareRefresh.hardware);
    const int hardwareTop = MonitorGpuMapTop(150, 800) + MonitorGpuPanelHeight(1) + 8;
    assert(hardwareRegions.size() == 1 && hardwareRegions[0].top == hardwareTop &&
           hardwareRegions[0].bottom == hardwareTop + MonitorHardwareHistoryHeight);

    auto gpuMapAfter = hardwareBefore;
    gpuMapAfter["gpu_map"][0]["used"] = 2;
    const auto gpuMapRefresh = MonitorPageRefreshChangesFor(hardwareBefore, gpuMapAfter);
    assert(gpuMapRefresh.gpu && !gpuMapRefresh.hardware);
    const auto gpuMapRegions = MonitorPageRefreshRegionsFor(
        264, 150, 800, 1, false, false, false, false, false,
        gpuMapRefresh.gpu, gpuMapRefresh.hardware);
    const auto expectedGpuMap = MonitorGpuPanelRegionFor(264, 150, 800, 1, false);
    assert(gpuMapRegions.size() == 1 && gpuMapRegions[0].top == expectedGpuMap.top &&
           gpuMapRegions[0].bottom == expectedGpuMap.bottom);
    assert(MonitorPageOwnsBackground(true) && !MonitorPageOwnsBackground(false));

    std::wstring reportText;
    for (int i = 0; i < 20; ++i)
        reportText += L"line " + std::to_wstring(i / 10) + std::to_wstring(i % 10) + L"\r\n";
    HWND report = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                  WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                  0, 0, 250, 60, parent, (HMENU)(INT_PTR)51, instance, nullptr);
    assert(report && SetWindowSubclass(report, EditRedrawProbeProc, 1, 0));
    SetWindowTextW(report, reportText.c_str());
    SendMessageW(report, EM_LINESCROLL, 0, 8);
    const int reportFirstLine = (int)SendMessageW(report, EM_GETFIRSTVISIBLELINE, 0, 0);
    assert(reportFirstLine > 0);
    const size_t selectedLine = reportText.find(L"line 12");
    assert(selectedLine != std::wstring::npos);
    SendMessageW(report, EM_SETSEL, (WPARAM)selectedLine, (LPARAM)(selectedLine + 7));
    UpdateWindow(report);
    g_editPaints = 0;
    const size_t changedLine = reportText.find(L"line 09");
    assert(changedLine != std::wstring::npos);
    reportText.replace(changedLine, 7, L"line 09 updated");
    const int restoreRedrawDisabledBefore = g_editRedrawDisabled;
    const int restoreRedrawEnabledBefore = g_editRedrawEnabled;
    NativeSetEditTextPreservingView(report, reportText);
    assert(g_editRedrawDisabled == restoreRedrawDisabledBefore + 1 &&
           g_editRedrawEnabled == restoreRedrawEnabledBefore + 1);

    std::wstring manyLines;
    for (int i = 0; i < 60; ++i)
        manyLines += L"line " + std::to_wstring(i / 10) + std::to_wstring(i % 10) + L" value 0\r\n";
    HWND manyReport = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                      0, 0, 120, 60, parent, (HMENU)(INT_PTR)53, instance, nullptr);
    assert(manyReport && SetWindowSubclass(manyReport, EditRedrawProbeProc, 1, 0));
    SetWindowTextW(manyReport, manyLines.c_str());
    UpdateWindow(manyReport);
    for (int i = 0; i < 30; ++i)
    {
        const auto start = manyLines.find(L"value 0", (size_t)i * 20);
        assert(start != std::wstring::npos);
        manyLines[start + 6] = L'1';
    }
    const int redrawDisabledBefore = g_editRedrawDisabled;
    const int redrawEnabledBefore = g_editRedrawEnabled;
    NativeSetEditTextPreservingView(manyReport, manyLines);
    assert(g_editRedrawDisabled == redrawDisabledBefore + 1 && g_editRedrawEnabled == redrawEnabledBefore + 1);
    HRGN manyDirty = CreateRectRgn(0, 0, 0, 0);
    assert(manyDirty && GetUpdateRgn(manyReport, manyDirty, FALSE) != ERROR);
    DeleteObject(manyDirty);
    std::wstring actualManyLines((size_t)GetWindowTextLengthW(manyReport) + 1, L'\0');
    GetWindowTextW(manyReport, actualManyLines.data(), (int)actualManyLines.size());
    actualManyLines.resize(manyLines.size());
    assert(actualManyLines == manyLines);
    RECT reportClient{};
    GetClientRect(report, &reportClient);
    HRGN dirtyRegion = CreateRectRgn(0, 0, 0, 0);
    assert(dirtyRegion && GetUpdateRgn(report, dirtyRegion, FALSE) != ERROR);
    assert(!PtInRegion(dirtyRegion, reportClient.right - 2, reportClient.bottom - 2));
    DeleteObject(dirtyRegion);
    std::wstring actualReport((size_t)GetWindowTextLengthW(report) + 1, L'\0');
    GetWindowTextW(report, actualReport.data(), (int)actualReport.size());
    actualReport.resize(reportText.size());
    assert(actualReport == reportText);
    assert((int)SendMessageW(report, EM_GETFIRSTVISIBLELINE, 0, 0) == reportFirstLine);
    DWORD selectionStart = 0, selectionEnd = 0;
    SendMessageW(report, EM_GETSEL, (WPARAM)&selectionStart, (LPARAM)&selectionEnd);
    assert(actualReport.substr(selectionStart, selectionEnd - selectionStart) == L"line 12");
    UpdateWindow(report);
    assert(g_editPaints > 0);
    const int finalRedrawDisabledBefore = g_editRedrawDisabled;
    const int finalRedrawEnabledBefore = g_editRedrawEnabled;
    NativeSetEditTextPreservingView(report, reportText);
    assert(g_editRedrawDisabled == finalRedrawDisabledBefore &&
           g_editRedrawEnabled == finalRedrawEnabledBefore);

    std::wstring anchoredText;
    for (int i = 0; i < 20; ++i)
        anchoredText += L"row " + std::to_wstring(i / 10) + std::to_wstring(i % 10) + L"\r\n";
    HWND anchoredReport = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                          WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                          0, 0, 250, 60, parent, (HMENU)(INT_PTR)54, instance, nullptr);
    assert(anchoredReport);
    SetWindowTextW(anchoredReport, anchoredText.c_str());
    SendMessageW(anchoredReport, EM_LINESCROLL, 0, 8);
    const int anchoredFirstLine = (int)SendMessageW(anchoredReport, EM_GETFIRSTVISIBLELINE, 0, 0);
    assert(anchoredFirstLine > 0);
    const auto firstVisibleText = [](HWND edit) {
        const int firstLine = (int)SendMessageW(edit, EM_GETFIRSTVISIBLELINE, 0, 0);
        const LRESULT start = SendMessageW(edit, EM_LINEINDEX, firstLine, 0);
        assert(start >= 0);
        std::wstring text((size_t)GetWindowTextLengthW(edit) + 1, L'\0');
        GetWindowTextW(edit, text.data(), (int)text.size());
        text.resize((size_t)GetWindowTextLengthW(edit));
        const size_t end = text.find_first_of(L"\r\n", (size_t)start);
        return text.substr((size_t)start, end == std::wstring::npos ? end : end - (size_t)start);
    };
    assert(firstVisibleText(anchoredReport) == L"row 08");
    std::wstring withHeader = L"status\r\n";
    withHeader += anchoredText;
    NativeSetEditTextPreservingView(anchoredReport, withHeader);
    assert((int)SendMessageW(anchoredReport, EM_GETFIRSTVISIBLELINE, 0, 0) == anchoredFirstLine + 1);
    assert(firstVisibleText(anchoredReport) == L"row 08");
    NativeSetEditTextPreservingView(anchoredReport, anchoredText);
    assert((int)SendMessageW(anchoredReport, EM_GETFIRSTVISIBLELINE, 0, 0) == anchoredFirstLine);
    assert(firstVisibleText(anchoredReport) == L"row 08");

    HWND hiddenReport = CreateWindowExW(0, L"EDIT", L"before",
                                       WS_CHILD | ES_MULTILINE | ES_READONLY,
                                       0, 0, 120, 60, parent, (HMENU)(INT_PTR)52, instance, nullptr);
    assert(hiddenReport && !IsWindowVisible(hiddenReport));
    NativeSetEditTextPreservingView(hiddenReport, L"after");
    assert(!IsWindowVisible(hiddenReport));

    const LRESULT objectId = SendMessageW(button, WM_GETOBJECT, 0, OBJID_CLIENT);
    assert(objectId);
    IAccessible *accessible = nullptr;
    assert(SUCCEEDED(ObjectFromLresult(objectId, IID_IAccessible, 0,
                                       reinterpret_cast<void **>(&accessible))));
    assert(accessible);

    VARIANT child;
    VariantInit(&child);
    V_VT(&child) = VT_I4;
    V_I4(&child) = CHILDID_SELF;
    VARIANT role;
    VariantInit(&role);
    assert(SUCCEEDED(accessible->get_accRole(child, &role)));
    assert(V_VT(&role) == VT_I4 && V_I4(&role) == ROLE_SYSTEM_PUSHBUTTON);
    VariantClear(&role);
    VARIANT state;
    VariantInit(&state);
    assert(SUCCEEDED(accessible->get_accState(child, &state)));
    assert(V_VT(&state) == VT_I4 && (V_I4(&state) & STATE_SYSTEM_FOCUSABLE));
    VariantClear(&state);
    BSTR msaaName = nullptr;
    assert(SUCCEEDED(accessible->get_accName(child, &msaaName)));
    assert(msaaName && wcscmp(msaaName, L"Choose model") == 0);
    SysFreeString(msaaName);
    accessible->Release();

    MSG pending{};
    PeekMessageW(&pending, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    const DWORD uiThread = GetCurrentThreadId();
    bool uiAutomationPassed = false;
    std::thread uiAutomationThread([&] {
        const HRESULT threadComResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        assert(SUCCEEDED(threadComResult));
        IUIAutomation *automation = nullptr;
        assert(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&automation))));
        auto invokeAndWait = [&](HWND control, std::atomic<int> &clicks) {
            IUIAutomationElement *controlElement = nullptr;
            assert(SUCCEEDED(automation->ElementFromHandle(control, &controlElement)));
            IUIAutomationInvokePattern *pattern = nullptr;
            assert(SUCCEEDED(controlElement->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&pattern))));
            const int before = clicks.load();
            const HRESULT result = pattern->Invoke();
            const ULONGLONG deadline = GetTickCount64() + 2000;
            while (SUCCEEDED(result) && clicks.load() == before && GetTickCount64() < deadline) Sleep(10);
            assert(SUCCEEDED(result) && clicks.load() == before + 1);
            pattern->Release();
            controlElement->Release();
        };
        invokeAndWait(greetingModel, g_accessibleButtonInvocations);
        invokeAndWait(reasoning, g_reasoningButtonClicks);
        IUIAutomationElement *element = nullptr;
        assert(SUCCEEDED(automation->ElementFromHandle(button, &element)));
        CONTROLTYPEID controlType = 0;
        assert(SUCCEEDED(element->get_CurrentControlType(&controlType)));
        assert(controlType == UIA_ButtonControlTypeId);
        BOOL keyboardFocusable = FALSE;
        assert(SUCCEEDED(element->get_CurrentIsKeyboardFocusable(&keyboardFocusable)));
        assert(keyboardFocusable);
        BSTR uiaName = nullptr;
        assert(SUCCEEDED(element->get_CurrentName(&uiaName)));
        assert(uiaName && wcscmp(uiaName, L"Choose model") == 0);
        SysFreeString(uiaName);
        IUIAutomationElement *searchElement = nullptr;
        assert(SUCCEEDED(automation->ElementFromHandle(search, &searchElement)));
        BSTR searchName = nullptr;
        assert(SUCCEEDED(searchElement->get_CurrentName(&searchName)));
        assert(searchName && wcscmp(searchName, L"Search chats") == 0);
        SysFreeString(searchName);
        searchElement->Release();
        IUIAutomationElement *chatListElement = nullptr;
        assert(SUCCEEDED(automation->ElementFromHandle(chatList, &chatListElement)));
        CONTROLTYPEID chatListType = 0;
        assert(SUCCEEDED(chatListElement->get_CurrentControlType(&chatListType)));
        assert(chatListType == UIA_ListControlTypeId);
        BSTR chatListName = nullptr;
        assert(SUCCEEDED(chatListElement->get_CurrentName(&chatListName)));
        assert(chatListName && wcscmp(chatListName, L"Chats") == 0);
        SysFreeString(chatListName);
        chatListElement->Release();
        IUIAutomationElement *modelsElement = nullptr;
        assert(SUCCEEDED(automation->ElementFromHandle(models, &modelsElement)));
        CONTROLTYPEID modelsType = 0;
        assert(SUCCEEDED(modelsElement->get_CurrentControlType(&modelsType)));
        assert(modelsType == UIA_ListControlTypeId);
        BSTR modelsName = nullptr;
        assert(SUCCEEDED(modelsElement->get_CurrentName(&modelsName)));
        assert(modelsName && wcscmp(modelsName, L"INSTALLED MODELS · Enter or double-click for details") == 0);
        SysFreeString(modelsName);
        modelsElement->Release();
        IUIAutomationElement *catalogElement = nullptr;
        assert(SUCCEEDED(automation->ElementFromHandle(catalog, &catalogElement)));
        BSTR catalogName = nullptr;
        assert(SUCCEEDED(catalogElement->get_CurrentName(&catalogName)));
        assert(catalogName && wcscmp(catalogName, L"MAYA-SUPPORTED DOWNLOADS") == 0);
        SysFreeString(catalogName);
        catalogElement->Release();
        IUIAutomationInvokePattern *invoke = nullptr;
        const HRESULT patternResult = element->GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(&invoke));
        assert(SUCCEEDED(patternResult) && invoke);
        const int clicksBeforeInvoke = g_accessibleButtonInvocations.load();
        const HRESULT invokeResult = invoke->Invoke();
        const ULONGLONG deadline = GetTickCount64() + 2000;
        while (SUCCEEDED(invokeResult) && g_accessibleButtonInvocations.load() == clicksBeforeInvoke &&
               GetTickCount64() < deadline)
            Sleep(10);
        assert(SUCCEEDED(invokeResult) && g_accessibleButtonInvocations.load() == clicksBeforeInvoke + 1);
        invoke->Release();
        element->Release();
        automation->Release();
        uiAutomationPassed = true;
        CoUninitialize();
        assert(PostThreadMessageW(uiThread, WM_QUIT, 0, 0));
    });
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    uiAutomationThread.join();
    assert(uiAutomationPassed && g_accessibleButtonInvocations.load() == 4 && g_reasoningButtonClicks.load() == 2);

    DestroyWindow(parent);
    CoUninitialize();
    std::cout << "Existing-chat and greeting model UIA Invoke; reasoning UIA Invoke; model index mapping; multi-row Monitor repaint and tab order passed.\n";
    return 0;
}

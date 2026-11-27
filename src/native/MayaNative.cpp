#include <winsock2.h>
#include <iphlpapi.h>
#include "../client.h"
#include <limits>
#include <unordered_map>
#include "chat_metadata.h"
#include "chat_menu_policy.h"
#include "chat_delete_policy.h"
#include "chat_preferences.h"
#include "chat_sampling.h"
#include "chat_reasoning_control.h"
#include "chat_export.h"
#include "chat_history.h"
#include "chat_context.h"
#include "chat_answer.h"
#include "chat_tools.h"
#include "mcp_tools_control.h"
#include "connect_info.h"
#include "local_data.h"
#include "monitor_report.h"
#include "http.h"
#include "pull_status.h"
#include "settings_flow.h"
#include "settings_layout.h"
#include "model_delete.h"
#include "model_settings.h"
#include "model_settings_tabs.h"
#include "model_leftover.h"
#include "model_details.h"
#include "model_catalog.h"
#include "model_switch_result.h"
#include "model_switch_watch.h"
#include "thought_display.h"
#include "savings_report.h"
#include "savings_price.h"
#include "about_report.h"
#include "keyboard_shortcuts.h"
#include "native_combo_box.h"
#include "native_edit_text.h"
#include "tab_navigation.h"
#include "startup_question.h"
#include "chat_scroll.h"
#include "chat_markdown.h"
#include "chat_code_actions.h"
#include "code_viewer_policy.h"
#include "chat_attachments.h"
#include "video_player.h"
#include "chat_message_ranges.h"
#include "chat_clipboard.h"
#include "image_runtime.h"
#include "chat_day_group.h"
#include "chat_ideas.h"
#include "composer_layout.h"
#include "chat_search_preview.h"
#include "status_poll.h"
#include "context_stops.h"
#include "context_warning.h"
#include "theme_policy.h"
#include "update_preflight.h"
#include "update_watch.h"
#include "turbo_native.h"
#include "windows_startup_policy.h"
#include "list_refresh.h"
#include "status_card.h"
#include "support_link.h"
#include "manager_title.h"
#include "accessible_button.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <commctrl.h>
#include <commdlg.h>
#include <cstdlib>
#include <cwctype>
#include <dwmapi.h>
#include <fstream>
#include <gdiplus.h>
#include <iomanip>
#include <imm.h>
#include <limits>
#include <mutex>
#include <optional>
#include <set>
#include <richedit.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <sstream>
#include <thread>
#include <windows.h>
#include <windowsx.h>
#include <uxtheme.h>

namespace fs = std::filesystem;

fs::path FindMayaRoot(fs::path candidate)
{
    for (int depth = 0; depth < 16; ++depth) {
        if (fs::is_regular_file(candidate / L"maya.py") &&
            fs::is_directory(candidate / L"serve" / L"web")) return candidate;
        const auto parent = candidate.parent_path();
        if (parent.empty() || parent == candidate) break;
        candidate = parent;
    }
    return {};
}

using Clock = std::chrono::steady_clock;
using namespace Gdiplus;
COLORREF C_BG = RGB(25, 25, 29), C_SIDE = RGB(17, 17, 20), C_CARD = RGB(34, 34, 39),
         C_CARD2 = RGB(42, 42, 48), C_CARD3 = RGB(52, 52, 59), C_OPEN = RGB(36, 36, 39),
         C_LINE = RGB(46, 46, 53), C_CARD_EDGE = RGB(58, 103, 238), C_TEXT = RGB(236, 236, 241),
         C_SOFT = RGB(201, 201, 211), C_MUTED = RGB(157, 157, 170), C_PURPLE = RGB(139, 124, 255),
         C_PURPLE_TEXT = RGB(180, 170, 255), C_CYAN = RGB(56, 189, 248), C_CYAN_TEXT = RGB(125, 211, 252),
         C_GREEN = RGB(52, 211, 153), C_GREEN_TEXT = RGB(110, 231, 183), C_WARN = RGB(252, 211, 77),
         C_RED = RGB(251, 113, 133), C_RED_TEXT = RGB(253, 164, 175);
constexpr wchar_t NativeRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t NativeStartupKey[] = L"Software\\MayaXS\\WinControl";
constexpr auto NativeStartupValue = MayaWindowsStartup::ManagerStartupRunValue;
constexpr UINT ATTACHMENT_OPEN_BASE = 1000, ATTACHMENT_REMOVE_BASE = 32000, ATTACHMENT_REMOVE_ALL = 62000;
constexpr UINT NativeModelSheetOverviewId = 9001, NativeModelSheetEngineId = 9002,
               NativeModelSheetStorageId = 9003, NativeModelSheetEditEngineId = 9004,
               NativeModelSheetCloseId = 9005;
constexpr UINT NativeModelSidebarId = 9006, NativeSupportSidebarId = 9007,
               NativeSupportAboutId = 9008, NativeAuthorGithubId = 9009,
               NativeAuthorHuggingFaceId = 9010;
constexpr wchar_t NativeAuthorGithubUrl[] = L"https://github.com/DanielMarius";
constexpr wchar_t NativeAuthorHuggingFaceUrl[] = L"https://huggingface.co/DanielMarius";
constexpr UINT NativeMayaCreatorGithubId = 9011, NativeMayaCreatorHuggingFaceId = 9012,
               NativeMayaComaintainerGithubId = 9013, NativeMayaVersionReleaseId = 9014;
constexpr wchar_t NativeMayaCreatorGithubUrl[] = L"https://github.com/mw00";
constexpr wchar_t NativeMayaCreatorHuggingFaceUrl[] = L"https://huggingface.co/peasantsmith";
constexpr wchar_t NativeMayaComaintainerGithubUrl[] = L"https://github.com/needmorevram";
constexpr UINT NativeHeaderLayerId = 9015;
constexpr UINT NativeApiAddressId = 9016;
constexpr UINT NativeCompactToggleId = 219, NativeTrayOpenId = 224, NativeTrayCompactId = 225,
               NativeTrayExitId = 226, NativeTrayMessageId = WM_APP + 20, NativeRestartTimerId = 14;
constexpr int NativeCompactClientWidth = 900, NativeCompactClientHeight = 154;
constexpr DWORD NativeThemedDropdownStyle = CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
static_assert(752 + 120 + 28 == NativeCompactClientWidth);
static_assert(ATTACHMENT_OPEN_BASE + 30000 <= ATTACHMENT_REMOVE_BASE &&
              ATTACHMENT_REMOVE_BASE + 30000 == ATTACHMENT_REMOVE_ALL);
constexpr UINT WM_DONE = WM_APP + 10, WM_STREAM = WM_APP + 11, WM_LIVE_METRICS = WM_APP + 12;
enum Page
{
    CHAT,
    MONITOR,
    MODELS,
    SETTINGS,
    SAVINGS,
    CHAT_DEFAULTS,
    CONNECT,
    ABOUT,
    DATA
};
enum Job
{
    J_NONE,
    J_STATUS,
    J_REFRESH,
    J_START,
    J_STOP,
    J_RESTART,
    J_UPDATE_CHECK,
    J_UPDATE_START,
    J_MODELS,
    J_CATALOG,
    J_CONTEXT,
    J_MCP,
    J_SETTINGS,
    J_SHARED_SAVE,
    J_TURBO_INFO,
    J_TURBO_SET,
    J_CHAT,
    J_SWITCH,
    J_LOAD,
    J_UNLOAD,
    J_PULL,
    J_PULL_STATUS,
    J_PULL_CANCEL,
    J_MODEL_DELETE_PLAN,
    J_MODEL_DELETE,
    J_MODEL_SETTINGS_PREVIEW,
    J_MODEL_SETTINGS_SAVE,
    J_MODEL_LEFTOVER_DELETE,
    J_SAVINGS,
    J_SAVINGS_RESET,
    J_SAVINGS_SAVE_PRICE,
    J_SAVINGS_DEFAULT_PRICE,
    J_SAVE_SETTINGS,
    J_BUDGET_SAVE,
    J_TOKENS,
    J_REPORT
};
struct Message
{
    std::string role, text, reasoning, error;
    std::string jobId;
    std::string meta;
    std::vector<fs::path> files;
    std::vector<std::string> attachmentData;
    Json tools = Json::array();
    Json usage = Json::object(), timings = Json::object();
    int toolLimit = 0;
    double thinkSecs = 0.0;
    bool stopped = false;
};
struct StreamPart
{
    enum Kind { TEXT, REASONING, MCP, STATS, JOB } kind;
    std::wstring text;
    Json tool;
};
struct Chat
{
    std::wstring id, title;
    std::wstring folder;
    std::string system, thinking = "high", thinkingFor;
    std::string pendingJob;
    bool pinned = false, titleManual = false;
    bool hasAblate = false, ablate = false;
    std::string ablateFor;
    std::vector<Message> messages;
    bool thinkingPicked = false;
};
struct Folder
{
    std::wstring id, name;
    bool collapsed = false;
};

std::wstring W(const std::string &s);
void SetText(HWND h, const std::wstring &s);
void SetTextPreservingEditView(HWND h, const std::wstring &s);

bool ChatTitleIsAuto(const Chat &chat)
{
    std::wstring firstUserText;
    for (const auto &message : chat.messages)
        if (message.role == "user" && !message.text.empty())
        {
            firstUserText = W(message.text.substr(0, 80));
            break;
        }
    return NativeChatTitleIsAuto(chat.title, chat.titleManual, firstUserText);
}

std::wstring ChatDisplayTitle(const Chat &chat)
{
    if (!ChatTitleIsAuto(chat)) return chat.title;
    for (const auto &message : chat.messages)
        if (message.role == "user" && (!message.text.empty() || !message.files.empty()))
        {
            std::vector<std::wstring> files;
            for (const auto &file : message.files) files.push_back(file.filename().wstring());
            return ChatTitleFrom(W(message.text), files);
        }
    return L"New chat";
}

struct ModelSettingField
{
    enum Kind { BOOLEAN, NUMBER, STRING, CHOICE, GPU_LIST } kind;
    std::string key;
    int group;
    int id;
    HWND label{}, control{};
    bool changed = false;
};
struct VisibleChatRow
{
    int chat = -1;
    int folder = -1;
    std::wstring snippet;
};
struct Done
{
    Job job;
    Json value;
    std::string error;
    bool backgroundPoll = false;
};
HINSTANCE g_inst{};
ULONG_PTR g_gdiplusToken{};
HWND g_hwnd{};
UINT g_statusPollIntervalMs = NativeStatusPollIntervalMs(false);
UINT g_metricsPollIntervalMs = NativeMetricsPollIntervalMs(false, false, false);
Page g_page = CHAT;
Page g_settingsReturnPage = CHAT;
Job g_busy = J_NONE;
bool g_online = false, g_images = false, g_useTools = true, g_pullActive = false, g_budgetSupported = false;
int g_budgetRequest = 0;
NativeApiConnection g_apiConnection = NativeApiConnection::CONNECTING;
unsigned g_metricPollFailures = 0;
bool g_switchRestartWatching = false, g_switchRestartDown = false, g_modelSwitchRequestInFlight = false;
bool g_modelSwitchRestartAfterChat = false;
bool g_modelDeleteRestartWatching = false, g_modelDeleteOutcomePending = false;
bool g_refreshModelsAfterChat = false;
ULONGLONG g_switchStartedAt = 0;
ULONGLONG g_turboRestartStartedAt = 0;
std::string g_switchTarget;
std::string g_seenModelDeleteOutcome;
bool g_resumingPendingJob = false;
ULONGLONG g_pendingChatRetryAt = 0;
bool g_showThinking = true;
bool g_windowsStartupLaunch = false;
bool g_autoStartMayaAttempted = false;
bool g_loadingChatDefaults = false;
bool g_sharedDefaultsOn = false;
bool g_sharedDefaultsSavePending = false;
bool g_mcpLoadAttempted = false;
std::atomic_bool g_mcpPollInFlight = false;
bool g_sidebarOpen = true;
bool g_deferLayoutRedraw = false;
int SidebarWidth() noexcept { return g_sidebarOpen ? 240 : 72; }
int SidebarContentLeft() noexcept { return g_sidebarOpen ? 264 : 96; }
bool g_compactMode = false, g_minimizeToTray = false, g_trayIconAdded = false, g_fullWindowPlacementValid = false;
WINDOWPLACEMENT g_fullWindowPlacement{sizeof(WINDOWPLACEMENT)};
UINT g_taskbarCreatedMessage = 0;
ULONGLONG g_restartStartedAt = 0;
std::wstring g_profile = L"maya-ds4-abliterated.json", g_userData, g_chatDir;
std::wstring g_startupQuestion;
std::string g_defaultSystem;
std::string g_savedModelId;
std::string g_themeChoice = "dark", g_schemeChoice = "violet";
bool g_systemLightTheme = false;
Json g_samplingPrefs = Json::object();
Json g_mcpData = Json::object();
Json g_turboSnapshot = nullptr, g_turboData = Json::object(), g_turboRequest = Json::object();
bool g_turboInfoInFlight = false;
bool g_turboRequestOn = false, g_turboRestartWatching = false, g_turboRestartDown = false;
Json g_sharedRequest = nullptr;
std::set<std::string> g_expandedThoughts;
std::set<std::string> g_collapsedThoughts;
std::vector<std::pair<CHARRANGE, std::string>> g_thoughtLinks;
std::set<std::string> g_expandedTools;
std::vector<std::pair<CHARRANGE, std::string>> g_toolLinks;
std::vector<std::pair<CHARRANGE, std::wstring>> g_markdownLinks;
std::vector<std::pair<CHARRANGE, fs::path>> g_attachmentLinks;
fs::path g_inlineVideoPath;
struct NativeInlineImagePreview
{
    LONG position = 0;
    fs::path file;
    std::unique_ptr<Image> thumbnail;
    bool attempted = false;
};
std::vector<NativeInlineImagePreview> g_inlineImagePreviews;
struct NativeCodeBlock { CHARRANGE range{}; std::wstring language, source; bool complete = false; };
std::vector<NativeCodeBlock> g_codeBlocks;
struct NativeLiveAnswerState
{
    bool active = false;
    LONG start = 0, tailStart = 0;
    size_t frozenUntil = 0;
    size_t baseToolLinks = 0, baseMarkdownLinks = 0, baseCodeBlocks = 0;
    size_t tailToolLinks = 0, tailMarkdownLinks = 0, tailCodeBlocks = 0;
};
NativeLiveAnswerState g_liveAnswer;
struct NativeLiveThoughtState
{
    bool active = false, expanded = false;
    std::string key;
    LONG bodyEnd = 0;
    size_t linkIndex = 0;
};
NativeLiveThoughtState g_liveThought;
std::vector<NativeMessageRange> g_userMessageRanges, g_assistantMessageRanges;
fs::path g_root, g_worker;
std::unique_ptr<MayaHttp> g_api;
    HWND g_headerLayer{}, g_nav[9]{}, g_sidebarToggle{}, g_navTooltip{}, g_savedLine{}, g_profileBox{}, g_start{}, g_stop{}, g_restart{}, g_update{}, g_compactToggle{}, g_status{}, g_detail{}, g_chatList{},
    g_modelSidebar{}, g_supportSidebar{}, g_supportAbout{},
    g_chatSearch{}, g_newChat{}, g_folderMenu{}, g_undoDelete{}, g_turboButton{}, g_transcript{}, g_input{}, g_send{}, g_attach{}, g_attachmentMenu{}, g_reason{}, g_reasonValue{}, g_ablate{}, g_stopAnswer{},
    g_greeting{}, g_greetingModel{}, g_chatModelMenu{}, g_ideas[4]{},
    g_editLast{}, g_regenerate{}, g_cancelEdit{}, g_answerOptions{}, g_chatOptions{}, g_contextMeter{}, g_jumpLatest{},
    g_monitorRequests{}, g_monitorRequestsHeading{}, g_models{}, g_catalog{}, g_modelsHeading{}, g_catalogHeading{}, g_catalogDescription{}, g_modelAddButton{}, g_pullProgress{}, g_modelAction{}, g_modelDelete{}, g_modelDownload{}, g_modelCancel{}, g_modelRefresh{}, g_modelLoad{},
    g_modelSettingsButton{}, g_modelSettingsEdit{}, g_modelSettingsPreview{}, g_modelSettingsPreviewButton{}, g_modelSettingsResetButton{},
    g_modelSettingsSave{}, g_modelSettingsCancel{}, g_modelSettingsModeButton{}, g_modelSettingsEnvButton{},
    g_modelSettingsEnvInfo{}, g_modelSettingsEnvList{}, g_modelSettingsEnvNameLabel{}, g_modelSettingsEnvName{},
    g_modelSettingsEnvValueLabel{}, g_modelSettingsEnvValue{}, g_modelSettingsEnvAdd{}, g_modelSettingsEnvRemove{},
    g_modelSettingsHint{}, g_modelSetupCommand{},
    g_modelSettingsTabs[4]{}, g_modelSettingsWarning{},
    g_modelSheetHeading{}, g_modelSheetTabs[3]{}, g_modelSheetContent{}, g_modelSheetEditEngine{}, g_modelSheetClose{},
    g_leftoverList{}, g_leftoverDetails{}, g_leftoverDelete{}, g_leftoverClose{}, g_leftoverOpenButton{},
    g_reportButton{}, g_requestToggle{},
     g_settings{}, g_contextLabel{}, g_contextEdit{}, g_portLabel{}, g_portEdit{}, g_budgetLabel{}, g_budgetEdit{}, g_budgetSave{}, g_budgetHint{}, g_toolsCheck{}, g_mcpDetails{}, g_settingsSave{}, g_settingsStatus{},
    g_defaultInstructionsLabel{}, g_defaultInstructions{}, g_saveDefaultInstructions{}, g_showThinkingCheck{},
    g_windowsStartupCheck{}, g_autoStartMayaCheck{}, g_minimizeToTrayCheck{}, g_saveWindowsStartup{}, g_shortcutsButton{},
    g_themeLabel{}, g_schemeLabel{}, g_themeBox{}, g_schemeBox{},
    g_connectTitle[3]{}, g_connectValue[3]{}, g_connectCopy[3]{}, g_connectAuth{}, g_connectInfo{},
    g_connectKeyLabel{}, g_connectKey{}, g_connectKeySave{}, g_connectKeyClear{},
    g_connectExampleLabel{}, g_connectExamples{}, g_connectExample{}, g_connectCopyExample{},
    g_samplingLabels[9]{}, g_samplingFields[9]{}, g_samplingEffort{}, g_samplingPreset{}, g_samplingSave{},
    g_samplingReset{}, g_samplingStatus{}, g_sharedDefaultsCheck{}, g_sharedDefaultsSummary{}, g_clearDataButton{},
    g_aboutFacts{}, g_aboutReport{}, g_aboutReportCopy{}, g_updateSummary{}, g_updateNotes{}, g_updateInstall{}, g_dataInfo{},
    g_authorCredit{}, g_authorGithub{}, g_authorHuggingFace{},
    g_mayaProjectCredit{}, g_mayaCreatorGithub{}, g_mayaCreatorHuggingFace{}, g_mayaComaintainerGithub{},
    g_mayaVersionLink{}, g_releaseNotes{},
    g_savings{}, g_savingsRefresh{}, g_savingsReset{}, g_priceModels{}, g_priceInput{}, g_priceCached{},
    g_priceOutput{}, g_pricePeak{}, g_priceSave{}, g_priceCancel{}, g_priceDefault{}, g_priceLabels[5]{};
HWND g_monitorGpuButtons[16]{};
HWND g_hoverButton{};
Json g_savingsData = Json::object();
Json g_priceRequest = Json::object();
std::string g_savingsModel;
std::vector<std::string> g_priceOriginal;
struct NativeSavingsPriceDraft { std::vector<std::string> values, saved; };
std::unordered_map<std::string, NativeSavingsPriceDraft> g_priceDrafts;
std::string g_modelSettingsFile, g_modelSettingsPreviewText;
std::string g_modelSheetFile, g_modelSheetPlanFile, g_modelSheetPlanError;
Json g_modelSheetPlan = Json::object();
int g_modelSheetTab = 0;
bool g_modelSheetOpen = false, g_modelSheetPlanPreview = false;
std::string g_updateSummaryText;
std::string g_latestReleaseNotesText =
    "Latest release · v1.1.73\r\n"
    "• Inkling 2 Mini now supports 8 GB NVIDIA cards, V100 and RTX 20 GPUs, plus AMD on Linux.\r\n"
    "• Optional drafting measures whether the official drafter is faster on this PC; accepted answers stay identical.\r\n"
    "• The looped-layer gate step is faster.\r\n"
    "• DeepSeek now preserves the configured VRAM reserve for the whole run.\r\n"
    "• Hy4-Flash uses safe host-memory transfers when two-GPU direct copies are unsafe.";
std::wstring g_mayaReleaseUrl;
std::wstring g_updateNotesUrl = L"https://github.com/mw00/project-maya/releases/tag/v1.1.73";
std::string g_updateTarget;
Json g_updateCheckResult = Json::object();
bool g_updateAvailable = false, g_updateWatching = false, g_updateDown = false,
     g_updateRecovering = false, g_updateRecoveryAttempted = false, g_updateChecked = false,
     g_updateCheckInFlight = false, g_updateInstallVisible = false, g_updateCanInstall = false,
     g_updateFreshInstall = false, g_updateStartAfterCheck = false,
     g_updateAutoStartAfterCheck = false;
int g_aboutUpdateCardHeight = 220;
std::string g_leftoverPath;
Json g_modelSettingsRequest = Json::object();
Json g_modelSettingsChoices = Json::object();
Json g_modelSettingsContextRange = Json::object();
Json g_modelSettingsSetup;
std::vector<ModelSettingField> g_modelSettingFields;
int g_modelSettingsTab = 0;
bool g_modelSettingsOpen = false, g_modelSettingsPreviewValid = false, g_modelSettingsAdvanced = false,
     g_modelSettingsEnvironmentOpen = false, g_leftoverOpen = false;
NativeModelSettingsTabMarker g_modelSettingsTabMarkers[4]{};
bool g_modelSettingsEnvironmentChanged = false;
std::vector<std::pair<std::wstring, std::wstring>> g_modelSettingsEnvironmentRows;
Job g_modelSettingsRequestInFlight = J_NONE;
bool g_modelSettingsRestartAfterChat = false;
bool g_modelSettingsRestartWatching = false, g_modelSettingsRestartDown = false;
ULONGLONG g_modelSettingsRestartStartedAt = 0;
bool g_refreshModelsWhenIdle = false, g_chatModelMenuOpen = false, g_chatModelMenuReopen = false,
     g_allRequests = false, g_aboutLogVisible = false,
     g_aboutCopyRequested = false, g_aboutReportRequest = false;
bool g_monitorVramHitFresh = false;
HFONT g_font{}, g_bold{}, g_brandFont{}, g_small{}, g_mono{};
    HICON g_mayaIconLarge{}, g_mayaIconSmall{}, g_deepseekIcon{};
    HFONT g_sidebarIconFont{};
HBRUSH g_bgBrush{}, g_sideBrush{}, g_cardBrush{}, g_fieldBrush{};
std::wstring g_monitorCellText;
constexpr const wchar_t *SidebarNavLabels[9] = {L"Chat", L"Monitor", L"Models", L"Settings", L"Savings",
    L"Chat defaults", L"Connect your tools", L"About and updates", L"Data"};
constexpr const wchar_t *SidebarNavGlyphs[9] = {L"\xE8BD", L"\xE770", L"\xE8AB", L"\xE713", L"\xE8C7",
    L"\xE734", L"\xE71B", L"\xE946", L"\xE7C3"};
void DrawThemedCombo(HWND h, HDC dc)
{
    RECT client{};
    GetClientRect(h, &client);
    const bool enabled = IsWindowEnabled(h) != FALSE;
    HBRUSH brush = CreateSolidBrush(enabled ? C_CARD : C_SIDE);
    FillRect(dc, &client, brush);
    DeleteObject(brush);
    HPEN pen = CreatePen(PS_SOLID, GetFocus() == h ? 2 : 1, GetFocus() == h ? C_PURPLE : C_LINE);
    HGDIOBJ oldPen = pen ? SelectObject(dc, pen) : nullptr;
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, client.left, client.top, client.right, client.bottom, 8, 8);
    SelectObject(dc, oldBrush);
    if (oldPen && oldPen != HGDI_ERROR) SelectObject(dc, oldPen);
    if (pen) DeleteObject(pen);
    wchar_t label[256]{};
    const LRESULT index = SendMessageW(h, CB_GETCURSEL, 0, 0);
    if (index >= 0) SendMessageW(h, CB_GETLBTEXT, index, (LPARAM)label);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, enabled ? C_TEXT : C_MUTED);
    HGDIOBJ oldFont = g_font ? SelectObject(dc, g_font) : nullptr;
    RECT text = client;
    text.left += 12;
    text.right -= 30;
    DrawTextW(dc, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (oldFont && oldFont != HGDI_ERROR) SelectObject(dc, oldFont);
    HPEN arrow = CreatePen(PS_SOLID, 2, enabled ? C_PURPLE_TEXT : C_MUTED);
    if (arrow)
    {
        HGDIOBJ previous = SelectObject(dc, arrow);
        const int x = client.right - 15, y = (client.top + client.bottom) / 2;
        MoveToEx(dc, x - 3, y - 1, nullptr);
        LineTo(dc, x, y + 2);
        LineTo(dc, x + 3, y - 1);
        SelectObject(dc, previous);
        DeleteObject(arrow);
    }
}
LRESULT CALLBACK ThemedComboProc(HWND h, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR)
{
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(h, &paint);
        DrawThemedCombo(h, dc);
        EndPaint(h, &paint);
        return 0;
    }
    if (message == WM_PRINT || message == WM_PRINTCLIENT)
    {
        DrawThemedCombo(h, (HDC)w);
        return 0;
    }
    if (message == WM_NCDESTROY)
    {
        const LRESULT result = DefSubclassProc(h, message, w, l);
        RemoveWindowSubclass(h, ThemedComboProc, id);
        return result;
    }
    return DefSubclassProc(h, message, w, l);
}
void ApplyNativeEditRegion(HWND h)
{
    if (!h) return;
    RECT client{};
    GetClientRect(h, &client);
    HRGN region = CreateRoundRectRgn(0, 0, client.right + 1, client.bottom + 1, 12, 12);
    if (region && !SetWindowRgn(h, region, TRUE)) DeleteObject(region);
}
void DrawNativeEditBorder(HWND h)
{
    RECT client{};
    GetClientRect(h, &client);
    HDC dc = GetDC(h);
    if (!dc) return;
    const bool focused = GetFocus() == h;
    HPEN pen = CreatePen(PS_SOLID, focused ? 2 : 1, focused ? C_CYAN : C_LINE);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, 0, 0, client.right, client.bottom, 12, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
    ReleaseDC(h, dc);
}
LRESULT CALLBACK NativeRoundedEditProc(HWND h, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR)
{
    if (message == WM_SIZE)
    {
        const auto result = DefSubclassProc(h, message, w, l);
        ApplyNativeEditRegion(h);
        return result;
    }
    if (message == WM_PAINT)
    {
        const auto result = DefSubclassProc(h, message, w, l);
        DrawNativeEditBorder(h);
        return result;
    }
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
    {
        const auto result = DefSubclassProc(h, message, w, l);
        InvalidateRect(h, nullptr, FALSE);
        return result;
    }
    if (message == WM_NCDESTROY)
    {
        const auto result = DefSubclassProc(h, message, w, l);
        RemoveWindowSubclass(h, NativeRoundedEditProc, id);
        return result;
    }
    return DefSubclassProc(h, message, w, l);
}
void StyleNativeEdit(HWND h)
{
    if (!h) return;
    SetWindowSubclass(h, NativeRoundedEditProc, 1, 0);
    ApplyNativeEditRegion(h);
}
void DrawManagerHeader(HDC dc, int width, int height)
{
    RECT bounds{0, 0, width, height};
    FillRect(dc, &bounds, g_bgBrush);
    const int bottom = std::min(120, height - 4);
    if (width <= 16 || bottom <= 12) return;
    HBRUSH headerBrush = CreateSolidBrush(C_CARD);
    HPEN headerPen = CreatePen(PS_SOLID, 2, C_CARD_EDGE);
    HGDIOBJ oldBrush = SelectObject(dc, headerBrush);
    HGDIOBJ oldPen = SelectObject(dc, headerPen);
    RoundRect(dc, 8, 8, width - 8, bottom, 18, 18);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(headerBrush);
    DeleteObject(headerPen);
    HBRUSH accent = CreateSolidBrush(C_CARD_EDGE);
    RECT accentRect{16, 28, 19, std::min(103, bottom - 8)};
    FillRect(dc, &accentRect, accent);
    DeleteObject(accent);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_TEXT);
    if (g_compactMode)
    {
        if (g_mayaIconLarge || g_mayaIconSmall)
            DrawIconEx(dc, 20, 20, g_mayaIconLarge ? g_mayaIconLarge : g_mayaIconSmall,
                       36, 36, 0, nullptr, DI_NORMAL);
        return;
    }
    if (g_sidebarOpen)
    {
        if (g_mayaIconLarge || g_mayaIconSmall)
            DrawIconEx(dc, 20, 21, g_mayaIconLarge ? g_mayaIconLarge : g_mayaIconSmall,
                       36, 36, 0, nullptr, DI_NORMAL);
        SelectObject(dc, g_brandFont ? g_brandFont : g_bold);
        RECT brand{68, 25, 200, 56};
        DrawTextW(dc, L"Maya", -1, &brand, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        SelectObject(dc, g_small);
        SetTextColor(dc, C_CYAN_TEXT);
        RECT sub{69, 54, 200, 74};
        DrawTextW(dc, L"LOCAL AI  /  WINDOWS", -1, &sub, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    else if (g_mayaIconLarge || g_mayaIconSmall)
        DrawIconEx(dc, 18, 20, g_mayaIconLarge ? g_mayaIconLarge : g_mayaIconSmall,
                   36, 36, 0, nullptr, DI_NORMAL);
}
std::vector<Chat> g_chats;
struct PendingChatDelete
{
    Chat chat;
    ULONGLONG expiresAt = 0;
};
std::vector<PendingChatDelete> g_pendingChatDeletes;
std::vector<Folder> g_folders;
std::vector<VisibleChatRow> g_visibleChats;
 int g_chat = -1, g_modelSel = -1, g_catalogSel = -1, g_editMessage = -1;
std::vector<int> g_catalogRowToModel;
int g_inputHeight = 82;
bool g_modelsAddOpen = false, g_catalogFetchPending = false, g_pullRequestInFlight = false, g_catalogUnavailable = false;
std::wstring g_savedCurrentChat;
std::wstring g_contextMeterChatId;
int g_monitorGpuOpen = -1, g_monitorGpuSelected = -1;
int g_monitorScrollY = 0;
int g_catalogScrollY = 0, g_pageScrollLimit = 0;
bool g_pageScrollDragging = false;
int g_pageScrollDragOffset = 0;
std::string g_monitorGpuOpenKey;
MonitorGpuSegmentHit g_monitorGpuHover;
std::wstring g_monitorGpuHoverText;
POINT g_monitorGpuHoverPoint{};
bool g_monitorMouseTracking = false;
MonitorPrefillPeakState g_monitorPrefillPeakState;
MonitorPrefillPeak g_monitorPrefillPeak;
std::wstring g_modelActionError;
Json g_health = Json::object(), g_metrics = Json::object(), g_modelData = Json::object(),
     g_catalogData = Json::object(), g_contextData = Json::object(), g_modelLiveContext = Json::object(),
     g_settingsData = Json::object();
std::vector<fs::path> g_attachments;
std::vector<std::string> g_attachmentSnapshots;
std::mutex g_streamMutex, g_jobIdMutex;
std::vector<StreamPart> g_streamParts;
std::atomic<HINTERNET> g_streamHandle{nullptr};
std::atomic_bool g_cancelled = false, g_cancelSent = false;
std::atomic_uint64_t g_contextSeq{0};
std::atomic_bool g_metricsInFlight{false};
uint64_t g_contextTokens = 0;
bool g_contextTokensKnown = false, g_contextTokensApproximate = false;
std::string g_jobId;
std::string g_deleteFile;
std::string g_deleteThen;
bool g_deleteFiles = false;
std::wstring g_jobChatId;
Message g_answer;
Clock::time_point g_actionAt;

bool SystemUsesLightTheme()
{
    HKEY key{};
    DWORD value = 1, size = sizeof(value), type = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS)
    {
        if (RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, &type, (BYTE *)&value, &size) != ERROR_SUCCESS ||
            type != REG_DWORD || size != sizeof(value))
            value = 1;
        RegCloseKey(key);
    }
    return value != 0;
}

LRESULT CALLBACK CheckboxPaintSubclassProc(HWND control, UINT message, WPARAM wParam, LPARAM lParam,
                                           UINT_PTR subclassId, DWORD_PTR)
{
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_PAINT)
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(control, &paint);
        RECT client{};
        GetClientRect(control, &client);
        FillRect(dc, &client, g_cardBrush);

        const bool enabled = IsWindowEnabled(control) != FALSE;
        const bool checked = SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const int controlHeight = (int)(client.bottom - client.top);
        const int size = std::min(18, std::max(14, controlHeight - 6));
        const int left = 3;
        const int top = (client.bottom - size) / 2;
        RECT box{left, top, left + size, top + size};
        HBRUSH boxBrush = CreateSolidBrush(checked ? C_PURPLE : C_CARD2);
        HPEN boxPen = CreatePen(PS_SOLID, 1, checked ? C_PURPLE : C_LINE);
        HGDIOBJ oldBrush = SelectObject(dc, boxBrush);
        HGDIOBJ oldPen = SelectObject(dc, boxPen);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, 5, 5);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(boxBrush);
        DeleteObject(boxPen);
        if (checked)
        {
            HPEN markPen = CreatePen(PS_SOLID, 2, enabled ? C_TEXT : C_MUTED);
            oldPen = SelectObject(dc, markPen);
            MoveToEx(dc, box.left + 4, box.top + size / 2, nullptr);
            LineTo(dc, box.left + size / 2 - 1, box.bottom - 4);
            LineTo(dc, box.right - 3, box.top + 4);
            SelectObject(dc, oldPen);
            DeleteObject(markPen);
        }

        wchar_t label[512]{};
        GetWindowTextW(control, label, (int)std::size(label));
        HFONT font = (HFONT)SendMessageW(control, WM_GETFONT, 0, 0);
        oldBrush = SelectObject(dc, font ? font : g_font);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, enabled ? C_TEXT : C_MUTED);
        RECT text{box.right + 8, 0, std::max(box.right + 8, client.right - 2), client.bottom};
        DrawTextW(dc, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        if (GetFocus() == control) DrawFocusRect(dc, &client);
        SelectObject(dc, oldBrush);
        EndPaint(control, &paint);
        return 0;
    }
    if (message == WM_NCDESTROY)
        RemoveWindowSubclass(control, CheckboxPaintSubclassProc, subclassId);
    return DefSubclassProc(control, message, wParam, lParam);
}

void ApplyCheckboxTheme(HWND control)
{
    if (!control) return;
    SetWindowSubclass(control, CheckboxPaintSubclassProc, 1, 0);
    InvalidateRect(control, nullptr, TRUE);
}

void RefreshCheckboxThemes()
{
    for (HWND control : {g_toolsCheck, g_showThinkingCheck, g_windowsStartupCheck,
                         g_autoStartMayaCheck, g_minimizeToTrayCheck, g_sharedDefaultsCheck, g_ablate})
        ApplyCheckboxTheme(control);
    for (const auto &field : g_modelSettingFields)
        if (field.kind == ModelSettingField::BOOLEAN)
            ApplyCheckboxTheme(field.control);
}

void ApplyAppearance()
{
    g_systemLightTheme = SystemUsesLightTheme();
    const bool systemLight = g_themeChoice == "system" && g_systemLightTheme;
    const bool light = g_themeChoice == "light" || systemLight;
    const bool violet = g_schemeChoice == "violet";
    const auto colors = NativeThemeColorsFor(light, violet);
    const auto color = [](std::uint32_t rgb) {
        return RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    };
    C_BG = color(colors.background); C_SIDE = color(colors.sidebar); C_CARD = color(colors.card);
    C_CARD2 = color(colors.fill); C_CARD3 = color(colors.surface3); C_OPEN = color(colors.open);
    C_LINE = color(colors.line); C_TEXT = color(colors.text);
    C_SOFT = color(colors.softText); C_MUTED = color(colors.mutedText);
    C_WARN = color(colors.warningText); C_PURPLE = color(colors.accent); C_PURPLE_TEXT = color(colors.accentText);
    C_CYAN = color(colors.info); C_CYAN_TEXT = color(colors.infoText); C_GREEN = color(colors.ok);
    C_GREEN_TEXT = color(colors.okText); C_RED = color(colors.danger); C_RED_TEXT = color(colors.dangerText);
    if (g_bgBrush) DeleteObject(g_bgBrush);
    if (g_sideBrush) DeleteObject(g_sideBrush);
    if (g_cardBrush) DeleteObject(g_cardBrush);
    if (g_fieldBrush) DeleteObject(g_fieldBrush);
    g_bgBrush = CreateSolidBrush(C_BG);
    g_sideBrush = CreateSolidBrush(C_SIDE);
    g_cardBrush = CreateSolidBrush(C_CARD);
    g_fieldBrush = CreateSolidBrush(C_CARD2);
    RefreshCheckboxThemes();
    if (g_hwnd)
    {
        const BOOL darkTitlebar = NativeTitlebarUsesDarkMode(light) ? TRUE : FALSE;
        if (FAILED(DwmSetWindowAttribute(g_hwnd, 20, &darkTitlebar, sizeof(darkTitlebar))))
            DwmSetWindowAttribute(g_hwnd, 19, &darkTitlebar, sizeof(darkTitlebar));
        SetClassLongPtrW(g_hwnd, GCLP_HBRBACKGROUND, (LONG_PTR)g_bgBrush);
        RedrawWindow(g_hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    }
}

std::wstring W(const std::string &s)
{
    if (s.empty())
        return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring r(n, L' ');
    if (n)
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n);
    return r;
}

std::wstring NativeStartupCommand()
{
    std::wstring path(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), (DWORD)path.size());
    if (!length || length >= path.size()) throw std::runtime_error("Could not resolve Maya Manager's executable path.");
    path.resize(length);
    return L"\"" + path + L"\" --windows-startup";
}

std::wstring NativeRegisteredStartup()
{
    wchar_t value[32768]{}; DWORD bytes = sizeof(value);
    const auto status = RegGetValueW(HKEY_CURRENT_USER, NativeRunKey, NativeStartupValue, RRF_RT_REG_SZ,
                                     nullptr, value, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) return {};
    if (status != ERROR_SUCCESS) throw std::runtime_error("Could not read this user's Windows startup entry.");
    return value;
}

bool NativeAutoStartMaya()
{
    DWORD value = 0, bytes = sizeof(value);
    const auto status = RegGetValueW(HKEY_CURRENT_USER, NativeStartupKey, L"AutoStartMaya", RRF_RT_REG_DWORD,
                                     nullptr, &value, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) return false;
    if (status != ERROR_SUCCESS) throw std::runtime_error("Could not read Maya's Windows startup setting.");
    return value != 0;
}

bool NativeMinimizeToTray()
{
    DWORD value = 0, bytes = sizeof(value);
    const auto status = RegGetValueW(HKEY_CURRENT_USER, NativeStartupKey, L"MinimizeToTray", RRF_RT_REG_DWORD,
                                     nullptr, &value, &bytes);
    if (status == ERROR_FILE_NOT_FOUND) return false;
    if (status != ERROR_SUCCESS) throw std::runtime_error("Could not read the minimize-to-tray setting.");
    return value != 0;
}

void AddManagerTrayIcon(HWND window)
{
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window;
    icon.uID = 1;
    icon.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    icon.uCallbackMessage = NativeTrayMessageId;
    icon.hIcon = g_mayaIconSmall ? g_mayaIconSmall : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(icon.szTip, L"Maya Manager");
    g_trayIconAdded = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
    if (g_trayIconAdded)
    {
        icon.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &icon);
    }
}

void LoadWindowsStartupControls()
{
    try
    {
        const auto registered = NativeRegisteredStartup();
        const auto expected = NativeStartupCommand();
        const bool manager = !registered.empty() && CompareStringOrdinal(registered.c_str(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL;
        SendMessageW(g_windowsStartupCheck, BM_SETCHECK, manager ? BST_CHECKED : BST_UNCHECKED, 0);
        const bool autoStart = NativeAutoStartMaya();
        SendMessageW(g_autoStartMayaCheck, BM_SETCHECK, manager && autoStart ? BST_CHECKED : BST_UNCHECKED, 0);
        EnableWindow(g_autoStartMayaCheck, manager);
        g_minimizeToTray = NativeMinimizeToTray();
        SendMessageW(g_minimizeToTrayCheck, BM_SETCHECK, g_minimizeToTray ? BST_CHECKED : BST_UNCHECKED, 0);
        SetText(g_settingsStatus, registered.empty() || manager
            ? L"Windows startup settings apply to this Windows user at next sign-in."
            : L"A different Maya controller path owns the startup entry; it is left untouched.");
        if (g_minimizeToTray && !g_trayIconAdded)
            SetText(g_settingsStatus, L"Windows could not add the notification icon, so minimize-to-tray will stay off.");
    }
    catch (const std::exception &e)
    {
        SetText(g_settingsStatus, W(e.what()));
    }
}

void SaveWindowsStartupControls()
{
    const bool manager = SendMessageW(g_windowsStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    const bool autoStart = MayaWindowsStartup::AutoStartRequiresManager(
        manager, SendMessageW(g_autoStartMayaCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
    const auto expected = NativeStartupCommand();
    const auto registered = NativeRegisteredStartup();
    if (!registered.empty() && CompareStringOrdinal(registered.c_str(), -1, expected.c_str(), -1, TRUE) != CSTR_EQUAL)
        throw std::runtime_error("A different Maya controller path owns the startup entry; it was left unchanged.");
    if (manager)
    {
        if (RegSetKeyValueW(HKEY_CURRENT_USER, NativeRunKey, NativeStartupValue, REG_SZ, expected.c_str(),
                            (DWORD)((expected.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS)
            throw std::runtime_error("Windows could not save the manager sign-in setting.");
    }
    else if (!registered.empty())
    {
        HKEY key{};
        if (RegOpenKeyExW(HKEY_CURRENT_USER, NativeRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
            throw std::runtime_error("Windows could not open the manager sign-in setting.");
        const auto status = RegDeleteValueW(key, NativeStartupValue);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS) throw std::runtime_error("Windows could not remove the manager sign-in setting.");
    }
    const DWORD value = autoStart ? 1 : 0;
    if (RegSetKeyValueW(HKEY_CURRENT_USER, NativeStartupKey, L"AutoStartMaya", REG_DWORD, &value, sizeof(value)) != ERROR_SUCCESS)
        throw std::runtime_error("Windows could not save Maya's automatic start setting.");
    const DWORD minimize = SendMessageW(g_minimizeToTrayCheck, BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
    if (RegSetKeyValueW(HKEY_CURRENT_USER, NativeStartupKey, L"MinimizeToTray", REG_DWORD, &minimize, sizeof(minimize)) != ERROR_SUCCESS)
        throw std::runtime_error("Windows could not save the minimize-to-tray setting.");
    g_minimizeToTray = minimize != 0;
    SetText(g_settingsStatus, L"Windows startup settings saved for this user.");
    EnableWindow(g_autoStartMayaCheck, manager);
}
std::string U(const std::wstring &s)
{
    if (s.empty())
        return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string r(n, ' ');
    if (n)
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
    return r;
}
std::wstring ChatThinking(const Chat &chat)
{
    const auto model = g_health.value("model", std::string());
    const auto fallback = ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()))
                              .value("reasoning_effort", std::string("high"));
    return W(ChatSamplingEffortForChat(chat.thinking, chat.thinkingFor, model, fallback));
}
bool SaveChatMetadata(const Chat &c);
void ScheduleContextMeter();
bool SaveChatPreferences(const std::string &system, bool showThinking);
void ApplyChatAblation()
{
    if (!g_ablate) return;
    const bool visible = g_page == CHAT && g_chat >= 0 && g_chat < (int)g_chats.size() &&
                         NativeChatAblationAvailable(g_health);
    ShowWindow(g_ablate, visible ? SW_SHOW : SW_HIDE);
    if (!visible) return;
    const auto &chat = g_chats[g_chat];
    const auto value = NativeChatAblationRequestValue(g_health, ChatModelIdentity(g_health),
                                                       chat.hasAblate, chat.ablate, chat.ablateFor);
    SendMessageW(g_ablate, BM_SETCHECK, value.is_boolean() && value.get<bool>() ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(g_ablate, value.is_boolean());
}
void ChooseChatAblation()
{
    if (!NativeChatAblationAvailable(g_health) || g_chat < 0 || g_chat >= (int)g_chats.size())
    {
        ApplyChatAblation();
        return;
    }
    auto &chat = g_chats[g_chat];
    const bool wasSet = chat.hasAblate, previous = chat.ablate;
    const auto previousModel = chat.ablateFor;
    chat.hasAblate = true;
    chat.ablate = SendMessageW(g_ablate, BM_GETCHECK, 0, 0) == BST_CHECKED;
    chat.ablateFor = ChatModelIdentity(g_health);
    if (!chat.messages.empty() && !SaveChatMetadata(chat))
    {
        chat.hasAblate = wasSet;
        chat.ablate = previous;
        chat.ablateFor = previousModel;
        ApplyChatAblation();
        return;
    }
    SetText(g_status, chat.ablate ? L"Refusal ablation is on for this chat." : L"Refusal ablation is off for this chat.");
    ApplyChatAblation();
}
void ApplyChatThinking()
{
    if (!g_reason) return;
    const auto model = g_health.value("model", std::string());
    auto value = ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()))
                     .value("reasoning_effort", std::string("high"));
    if (g_chat >= 0 && g_chat < (int)g_chats.size()) value = U(ChatThinking(g_chats[g_chat]));
    const auto efforts = ChatSamplingEfforts(ChatSamplingFamily(model));
    EnableWindow(g_reason, efforts.size() > 1);
    SetText(g_reason, W(ChatSamplingEffortLabel(value)));
    SetText(g_reasonValue, L"Reasoning:");
    ApplyChatAblation();
}
void SaveChatReasoningChoice(const std::string &selected)
{
    if (selected.empty()) return;
    const auto model = g_health.value("model", std::string());
    const auto current = g_chat >= 0 && g_chat < (int)g_chats.size()
        ? U(ChatThinking(g_chats[g_chat]))
        : ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()))
              .value("reasoning_effort", std::string("high"));
    if (current == selected)
    {
        if (g_chat >= 0 && g_chat < (int)g_chats.size() && !g_chats[g_chat].thinkingPicked)
        {
            auto &chat = g_chats[g_chat];
            chat.thinkingPicked = true;
            if (!SaveChatMetadata(chat))
            {
                chat.thinkingPicked = false;
                ApplyChatThinking();
                return;
            }
            SetText(g_status, L"This reasoning level is now selected for this chat.");
        }
        return;
    }
    if (g_chat >= 0 && g_chat < (int)g_chats.size())
    {
        auto &chat = g_chats[g_chat];
        const bool previousPicked = chat.thinkingPicked;
        chat.thinkingPicked = true;
        if (!CommitChatReasoningChoice(chat.thinking, chat.thinkingFor, selected, model,
                                       [&chat] { return SaveChatMetadata(chat); }))
        {
            chat.thinkingPicked = previousPicked;
            ApplyChatThinking();
            return;
        }
        ScheduleContextMeter();
    }
    else
    {
        const auto previousSampling = g_samplingPrefs;
        g_samplingPrefs = SaveChatReasoningEffort(g_samplingPrefs, model, selected);
        if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
        {
            g_samplingPrefs = previousSampling;
            ApplyChatThinking();
            SetText(g_status, L"Could not save the default reasoning choice; the previous value was restored.");
            return;
        }
    }
    ApplyChatThinking();
}
void ShowChatReasoningMenu(HWND owner)
{
    if (!g_reason || !IsWindowEnabled(g_reason)) return;
    const auto model = g_health.value("model", std::string());
    const auto efforts = ChatSamplingEfforts(ChatSamplingFamily(model));
    if (efforts.size() < 2) return;
    const auto current = g_chat >= 0 && g_chat < (int)g_chats.size()
        ? U(ChatThinking(g_chats[g_chat]))
        : ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()))
              .value("reasoning_effort", std::string("high"));
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (size_t i = 0; i < efforts.size(); ++i)
    {
        const UINT flags = MF_STRING | (efforts[i] == current ? MF_CHECKED : 0);
        const auto label = W(ChatSamplingEffortLabel(efforts[i]));
        AppendMenuW(menu, flags, NativeChatReasoningMenuCommandBase + (UINT)i, label.c_str());
    }
    RECT bounds{};
    if (!GetWindowRect(g_reason, &bounds))
    {
        DestroyMenu(menu);
        return;
    }
    const int selected = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_WORKAREA,
                                             bounds.left, bounds.top, 0, owner, nullptr);
    DestroyMenu(menu);
    const int index = NativeChatReasoningMenuIndexFor(selected, efforts.size());
    if (index >= 0) SaveChatReasoningChoice(efforts[(size_t)index]);
}
std::wstring JsonText(const Json &j, const char *key, const char *fallback = "")
{
    if (!j.is_object() || !j.contains(key) || j[key].is_null())
        return W(fallback);
    if (j[key].is_string())
        return W(j[key].get<std::string>());
    return W(j[key].dump());
}
std::wstring Quote(const std::wstring &s)
{
    std::wstring r = L"\"";
    size_t n = 0;
    for (auto c : s)
    {
        if (c == L'\\')
        {
            ++n;
            continue;
        }
        if (c == L'\"')
            r.append(n * 2 + 1, L'\\');
        else
            r.append(n, L'\\');
        r += c;
        n = 0;
    }
    r.append(n * 2, L'\\');
    return r + L"\"";
}
std::wstring Utf8Path(const fs::path &p)
{
    return p.wstring();
}
std::wstring NewId()
{
    GUID id{};
    CoCreateGuid(&id);
    wchar_t s[40]{};
    StringFromGUID2(id, s, 40);
    std::wstring r = s;
    r.erase(std::remove(r.begin(), r.end(), L'{'), r.end());
    r.erase(std::remove(r.begin(), r.end(), L'}'), r.end());
    return r;
}
std::string ErrorJson(const std::string &s)
{
    return s.empty() ? "request failed" : s;
}
void SetText(HWND h, const std::wstring &s)
{
    if (!h) return;
    const int length = GetWindowTextLengthW(h);
    if (length == (int)s.size())
    {
        std::wstring current((size_t)length + 1, L'\0');
        GetWindowTextW(h, current.data(), length + 1);
        current.resize((size_t)length);
        if (!NativeTextContentsChanged(current, s)) return;
    }
    SetWindowTextW(h, s.c_str());
}
void SetTextPreservingEditView(HWND h, const std::wstring &s)
{
    NativeSetEditTextPreservingView(h, s);
}
void SetUpdateAvailable(bool available)
{
    const bool changed = g_updateAvailable != available;
    g_updateAvailable = available;
    if (g_update)
    {
        SetWindowTextW(g_update, available ? L"Update" : L"Check updates");
        InvalidateRect(g_update, nullptr, FALSE);
    }
    if (!changed || !g_nav[7]) return;
    SetWindowTextW(g_nav[7], available ? L"About and updates. A new version is available" : L"About and updates");
    InvalidateRect(g_nav[7], nullptr, TRUE);
}
bool SetListContentsIfChanged(HWND list, const std::vector<std::wstring> &updated)
{
    const int count = (int)SendMessageW(list, LB_GETCOUNT, 0, 0);
    std::vector<std::wstring> current;
    current.reserve(std::max(0, count));
    const int selected = (int)SendMessageW(list, LB_GETCURSEL, 0, 0);
    std::wstring selectedText;
    for (int i = 0; i < count; ++i)
    {
        const int length = (int)SendMessageW(list, LB_GETTEXTLEN, i, 0);
        if (length < 0) return false;
        std::wstring text((size_t)length + 1, L'\0');
        if (SendMessageW(list, LB_GETTEXT, i, (LPARAM)text.data()) == LB_ERR) return false;
        text.resize((size_t)length);
        if (i == selected) selectedText = text;
        current.push_back(std::move(text));
    }
    if (!NativeListContentsChanged(current, updated)) return false;
    SendMessageW(list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    int restore = -1;
    for (size_t i = 0; i < updated.size(); ++i)
    {
        SendMessageW(list, LB_ADDSTRING, 0, (LPARAM)updated[i].c_str());
        if (!selectedText.empty() && updated[i] == selectedText) restore = (int)i;
    }
    if (restore >= 0) SendMessageW(list, LB_SETCURSEL, restore, 0);
    else if (selected >= 0 && selected < (int)updated.size()) SendMessageW(list, LB_SETCURSEL, selected, 0);
    SendMessageW(list, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(list, nullptr, nullptr, RDW_INVALIDATE | RDW_NOERASE);
    return true;
}
void SetCatalogHorizontalExtent(const std::vector<std::wstring> &rows)
{
    HDC dc = GetDC(g_catalog);
    if (!dc) return;
    const auto font = (HFONT)SendMessageW(g_catalog, WM_GETFONT, 0, 0);
    const HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
    int width = 0;
    for (const auto &row : rows)
    {
        if (row.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) continue;
        SIZE extent{};
        if (GetTextExtentPoint32W(dc, row.c_str(), static_cast<int>(row.size()), &extent))
            width = std::max(width, static_cast<int>(extent.cx));
    }
    if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
    ReleaseDC(g_catalog, dc);
    SendMessageW(g_catalog, LB_SETHORIZONTALEXTENT, width + 2 * GetSystemMetrics(SM_CXEDGE) + 8, 0);
}
std::string ControlText(HWND h)
{
    const int n = GetWindowTextLengthW(h);
    std::wstring value(n + 1, L'\0');
    GetWindowTextW(h, value.data(), n + 1);
    value.resize(n);
    return U(value);
}
void UpdateBudgetSave()
{
    if (!g_budgetSave) return;
    const int current = g_contextData.value("budget", Json::object()).value("budget", 0);
    bool changed = true, valid = false;
    try
    {
        const int draft = NativeSettingsInteger(W(ControlText(g_budgetEdit)));
        valid = NativeThinkingBudgetValid(draft);
        changed = draft != current;
    }
    catch (...) {}
    ShowWindow(g_budgetSave, g_page == SETTINGS && g_budgetSupported && changed ? SW_SHOW : SW_HIDE);
    SetText(g_budgetSave, g_busy == J_BUDGET_SAVE ? L"Saving…" : L"Save");
    EnableWindow(g_budgetSave, g_busy == J_NONE && g_budgetSupported && changed && valid);
    SetText(g_budgetHint, L"Use 0 (no limit) or 256 to 262144 tokens.");
    ShowWindow(g_budgetHint, g_page == SETTINGS && g_budgetSupported && changed && !valid ? SW_SHOW : SW_HIDE);
}
std::wstring PriceDisplay(const Json &value, bool peak = false)
{
    return W(SavingsPriceEditorText(value, peak));
}
void UpdatePriceDraftControls();
void LoadPriceFields()
{
    g_priceOriginal.clear();
    if (!g_priceModels || !g_savingsData.contains("price_list") || !g_savingsData["price_list"].is_array()) return;
    const int index = (int)SendMessageW(g_priceModels, CB_GETCURSEL, 0, 0);
    const auto &list = g_savingsData["price_list"];
    if (index < 0 || index >= (int)list.size()) return;
    const auto &p = list[index];
    g_savingsModel = SavingsString(p, "model");
    SetText(g_priceInput, PriceDisplay(p.value("input", Json())));
    SetText(g_priceCached, PriceDisplay(p.value("cached", Json())));
    SetText(g_priceOutput, PriceDisplay(p.value("output", Json())));
    const auto defaults = p.value("default", Json::object());
    const bool peak = defaults.is_object() && defaults.contains("peak") && defaults["peak"].is_number();
    SetText(g_pricePeak, peak ? PriceDisplay(p.value("peak", Json()), true) : L"");
    EnableWindow(g_pricePeak, peak);
    EnableWindow(g_priceDefault, p.value("custom", false));
    std::vector<std::string> saved{ControlText(g_priceInput), ControlText(g_priceCached), ControlText(g_priceOutput)};
    if (peak) saved.push_back(ControlText(g_pricePeak));
    const auto draft = g_priceDrafts.find(g_savingsModel);
    if (draft != g_priceDrafts.end())
    {
        if (SavingsPriceDraftCanRestore(draft->second.values, draft->second.saved, saved))
        {
            SetText(g_priceInput, W(draft->second.values[0]));
            SetText(g_priceCached, W(draft->second.values[1]));
            SetText(g_priceOutput, W(draft->second.values[2]));
            if (peak) SetText(g_pricePeak, W(draft->second.values[3]));
        }
        else
            g_priceDrafts.erase(draft);
    }
    g_priceOriginal = std::move(saved);
    UpdatePriceDraftControls();
}
void UpdatePriceDraftControls()
{
    if (!g_priceCancel) return;
    std::vector<std::string> draft{ControlText(g_priceInput), ControlText(g_priceCached), ControlText(g_priceOutput)};
    const bool peak = IsWindowEnabled(g_pricePeak) != FALSE;
    if (peak) draft.push_back(ControlText(g_pricePeak));
    const bool changed = !g_savingsModel.empty() && !g_priceOriginal.empty() &&
                         SavingsPriceDraftChanged(draft, g_priceOriginal);
    if (!g_savingsModel.empty() && !g_priceOriginal.empty())
    {
        if (changed) g_priceDrafts[g_savingsModel] = {draft, g_priceOriginal};
        else g_priceDrafts.erase(g_savingsModel);
    }
    bool valid = false;
    if (changed)
    {
        try { (void)SavingsPriceRequest(g_savingsModel, draft, peak); valid = true; }
        catch (...) {}
    }
    EnableWindow(g_priceSave, changed && valid);
    ShowWindow(g_priceCancel, changed && g_page == SAVINGS ? SW_SHOW : SW_HIDE);
}
void UpdateModelLoadButton()
{
    if (!g_modelLoad) return;
    const bool loaded = g_modelData.value("loaded", false);
    bool selectedRunning = false;
    if (g_modelSel >= 0 && g_modelData.contains("models") && g_modelSel < (int)g_modelData["models"].size())
        selectedRunning = g_modelData["models"][g_modelSel].value("running", false);
    SetText(g_modelLoad, NativeModelLoadButtonText(loaded, g_busy == J_LOAD, g_busy == J_UNLOAD));
    EnableWindow(g_modelLoad, NativeModelLoadButtonEnabled(selectedRunning,
        g_modelData.value("can_unload", false), g_busy != J_NONE));
}
bool ModelSwitchBlockedByWork()
{
    return NativeModelSwitchBlockedByWork(g_busy != J_NONE, g_busy == J_CHAT,
                                          g_busy == J_STATUS || g_busy == J_REFRESH);
}
void UpdateModelSwitchButton()
{
    if (!g_modelAction) return;
    bool enabled = false;
    if (g_modelSel >= 0 && g_modelData.contains("models") && g_modelData["models"].is_array() &&
        g_modelSel < (int)g_modelData["models"].size())
        enabled = ModelSwitchEnabled(g_modelData["models"][g_modelSel],
            ModelSwitchBlockedByWork(), g_modelSwitchRequestInFlight || g_switchRestartWatching);
    EnableWindow(g_modelAction, enabled);
}
void UpdateModelDeleteButton()
{
    if (!g_modelDelete) return;
    bool enabled = false;
    if (g_modelSel >= 0 && g_modelData.contains("models") && g_modelData["models"].is_array() &&
        g_modelSel < (int)g_modelData["models"].size())
        enabled = ModelDeleteEnabled(g_modelData["models"][g_modelSel], g_modelData["models"],
                                     g_modelData.value("restarts", false));
    EnableWindow(g_modelDelete, enabled && g_busy == J_NONE);
}
void UpdateModelSettingsButton()
{
    const bool enabled = g_modelSel >= 0 && g_modelData.contains("models") && g_modelData["models"].is_array() &&
        g_modelSel < (int)g_modelData["models"].size() && ModelSettingsButtonEnabled(g_modelData["models"][g_modelSel]);
    if (g_modelSettingsButton) EnableWindow(g_modelSettingsButton, enabled);
}
int ModelSheetModelIndex()
{
    if (g_modelSheetFile.empty() || !g_modelData.contains("models") || !g_modelData["models"].is_array()) return -1;
    const auto &models = g_modelData["models"];
    for (size_t i = 0; i < models.size(); ++i)
        if (models[i].is_object() && models[i].contains("file") && models[i]["file"].is_string() &&
            models[i]["file"].get<std::string>() == g_modelSheetFile)
            return (int)i;
    return -1;
}
void UpdateModelSheetView()
{
    if (!g_modelSheetOpen) return;
    const int index = ModelSheetModelIndex();
    if (index < 0)
    {
        g_modelSheetOpen = false;
        return;
    }
    g_modelSel = index;
    if (g_models) SendMessageW(g_models, LB_SETCURSEL, index, 0);
    const auto &model = g_modelData["models"][index];
    const auto title = ModelDisplayName(model.value("model", std::string("Model"))) + "  " +
                       model.value("quant", std::string());
    SetText(g_modelSheetHeading, W(title));
    const wchar_t *labels[] = {L"Overview", L"Engine", L"Storage"};
    for (int i = 0; i < 3; ++i)
        SetText(g_modelSheetTabs[i], (i == g_modelSheetTab ? L"●  " : L"") + std::wstring(labels[i]));
    const bool engineEnabled = ModelSettingsButtonEnabled(model);
    EnableWindow(g_modelSheetTabs[1], engineEnabled);
    if (!engineEnabled && g_modelSheetTab == 1) g_modelSheetTab = 0;
    EnableWindow(g_modelSheetEditEngine, engineEnabled);
    UpdateModelDeleteButton();
    if (g_modelSheetTab == 0)
        SetTextPreservingEditView(g_modelSheetContent, W(ModelOverviewReport(model, g_modelData.value("loaded", false))));
    else if (g_modelSheetTab == 1)
        SetTextPreservingEditView(g_modelSheetContent, W(ModelEngineReport(model)));
    else if (!g_modelSheetPlanError.empty())
        SetTextPreservingEditView(g_modelSheetContent, W(g_modelSheetPlanError));
    else if (g_modelSheetPlanPreview && g_deleteFile == g_modelSheetFile)
        SetTextPreservingEditView(g_modelSheetContent, L"Looking at this model's files…");
    else if (g_modelSheetPlanFile == g_modelSheetFile && g_modelSheetPlan.is_object())
        SetTextPreservingEditView(g_modelSheetContent, W(ModelDeletePlanReport(g_modelSheetPlan)));
    else
        SetTextPreservingEditView(g_modelSheetContent, L"Open Storage to inspect the files Maya can remove.");
}
void UpdateCatalogDownloadButton()
{
    if (!g_modelDownload) return;
    bool canDownload = false;
    if (g_catalogSel >= 0 && g_catalogData.contains("models") && g_catalogData["models"].is_array() &&
        g_catalogSel < (int)g_catalogData["models"].size())
        canDownload = CatalogCanStartPull(g_catalogData["models"][g_catalogSel], g_pullActive,
                                          g_pullRequestInFlight, g_catalogUnavailable);
    EnableWindow(g_modelDownload, canDownload);
}
void ScheduleContextMeter();
void UpdateComposerHeight();
struct PreviewImage
{
    std::unique_ptr<Bitmap> bitmap;
    bool ownedByWindow = false;
};
LRESULT CALLBACK PreviewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    auto *preview = reinterpret_cast<PreviewImage *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE)
    {
        preview = static_cast<PreviewImage *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(preview));
    }
    if (m == WM_ERASEBKGND) return 1;
    if (m == WM_PAINT)
    {
        if (g_compactMode)
        {
            PAINTSTRUCT compactPaint{};
            HDC compactDc = BeginPaint(h, &compactPaint);
            RECT client{};
            GetClientRect(h, &client);
            FillRect(compactDc, &client, g_bgBrush);
            HBRUSH panel = CreateSolidBrush(C_CARD);
            HPEN border = CreatePen(PS_SOLID, 1, C_LINE);
            HGDIOBJ oldBrush = SelectObject(compactDc, panel);
            HGDIOBJ oldPen = SelectObject(compactDc, border);
            RoundRect(compactDc, 10, 10, client.right - 10, client.bottom - 10, 20, 20);
            SelectObject(compactDc, oldBrush);
            SelectObject(compactDc, oldPen);
            DeleteObject(panel);
            DeleteObject(border);
            HBRUSH accent = CreateSolidBrush(C_PURPLE);
            RECT accentRect{12, 28, 16, client.bottom - 28};
            FillRect(compactDc, &accentRect, accent);
            DeleteObject(accent);
            if (g_mayaIconLarge || g_mayaIconSmall)
                DrawIconEx(compactDc, 22, 19, g_mayaIconLarge ? g_mayaIconLarge : g_mayaIconSmall,
                           38, 38, 0, nullptr, DI_NORMAL);
            SetBkMode(compactDc, TRANSPARENT);
            SetTextColor(compactDc, C_TEXT);
            SelectObject(compactDc, g_brandFont ? g_brandFont : g_bold);
            RECT brand{66, 17, 132, 42};
            DrawTextW(compactDc, L"Maya", -1, &brand, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SelectObject(compactDc, g_small);
            SetTextColor(compactDc, C_PURPLE_TEXT);
            RECT caption{66, 42, 134, 61};
            DrawTextW(compactDc, L"CONTROL", -1, &caption, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            EndPaint(h, &compactPaint);
            return 0;
        }
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(h, &ps);
        Graphics graphics(dc);
        graphics.Clear(Color(255, 25, 25, 29));
        if (preview && preview->bitmap)
        {
            RECT client{};
            GetClientRect(h, &client);
            const auto iw = (double)preview->bitmap->GetWidth(), ih = (double)preview->bitmap->GetHeight();
            const double scale = std::min((client.right - 32) / iw, (client.bottom - 32) / ih);
            if (scale > 0)
            {
                const auto dw = (INT)(iw * scale), dh = (INT)(ih * scale);
                graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
                graphics.DrawImage(preview->bitmap.get(), (client.right - dw) / 2, (client.bottom - dh) / 2, dw, dh);
            }
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_NCDESTROY)
    {
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        if (preview && preview->ownedByWindow) delete preview;
        return DefWindowProcW(h, m, w, l);
    }
    return DefWindowProcW(h, m, w, l);
}
bool ShowImagePreview(HWND owner, const fs::path &path)
{
    if (!MayaEnsureGdiplus(g_gdiplusToken)) return false;
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PreviewProc;
        wc.hInstance = g_inst;
        wc.lpszClassName = L"MayaAttachmentImagePreview";
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        registered = true;
    }
    auto preview = std::make_unique<PreviewImage>();
    preview->bitmap = std::make_unique<Bitmap>(path.c_str(), FALSE);
    if (preview->bitmap->GetLastStatus() != Ok || !preview->bitmap->GetWidth() || !preview->bitmap->GetHeight() ||
        (uint64_t)preview->bitmap->GetWidth() * preview->bitmap->GetHeight() > 60000000ULL)
        return false;
    const auto title = L"Image preview — " + path.filename().wstring();
    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int width = 1000, height = 700;
    const int x = (int)ownerRect.left + std::max(0, ((int)ownerRect.right - (int)ownerRect.left - width) / 2);
    const int y = (int)ownerRect.top + std::max(0, ((int)ownerRect.bottom - (int)ownerRect.top - height) / 2);
    auto *data = preview.release();
    if (!CreateWindowExW(WS_EX_TOOLWINDOW, L"MayaAttachmentImagePreview", title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         x, y, width, height, owner, nullptr, g_inst, data))
    {
        delete data;
        return false;
    }
    data->ownedByWindow = true;
    return true;
}
struct CodeViewerState
{
    std::wstring language, source;
    HWND wrap{}, edit{};
    bool wrapped = true;
    bool ownedByWindow = false;
};
HWND CreateCodeViewerEdit(HWND owner, CodeViewerState *state, DWORD selectionStart = 0, DWORD selectionEnd = 0)
{
    const DWORD style = MayaCodeViewerEditStyle(state->wrapped);
    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, 12, 48, 780, 480, owner,
                                (HMENU)2, g_inst, nullptr);
    if (!edit) return nullptr;
    SendMessageW(edit, EM_SETLIMITTEXT, (WPARAM)std::min<size_t>(state->source.size(), 0x7ffffffe), 0);
    SetWindowTextW(edit, state->source.c_str());
    SendMessageW(edit, WM_SETFONT, (WPARAM)g_mono, TRUE);
    SendMessageW(edit, EM_SETSEL, selectionStart, selectionEnd);
    return edit;
}
LRESULT CALLBACK CodeViewerProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    auto *state = reinterpret_cast<CodeViewerState *>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (m == WM_NCCREATE)
    {
        state = static_cast<CodeViewerState *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (m == WM_CREATE)
    {
        const int dpi = GetDpiForWindow(h);
        const auto px = [dpi](int value) { return MulDiv(value, dpi, 96); };
        std::wstring label = L"Code block";
        if (!state->language.empty()) label += L" · " + state->language;
        HWND language = CreateWindowW(L"STATIC", label.c_str(), WS_CHILD | WS_VISIBLE, px(14), px(14), px(310), px(24), h,
                                      nullptr, g_inst, nullptr);
        state->wrap = CreateWindowW(L"BUTTON", L"Wrap long lines", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    BS_AUTOCHECKBOX, px(330), px(10), px(160), px(30), h, (HMENU)1, g_inst, nullptr);
        ApplyCheckboxTheme(state->wrap);
        SendMessageW(language, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(state->wrap, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(state->wrap, BM_SETCHECK, state->wrapped ? BST_CHECKED : BST_UNCHECKED, 0);
        state->edit = CreateCodeViewerEdit(h, state);
        if (!language || !state->wrap || !state->edit) return -1;
        SetFocus(state->edit);
        return 0;
    }
    if (m == WM_SIZE && state && state->edit)
    {
        const int dpi = GetDpiForWindow(h);
        const int margin = MulDiv(12, dpi, 96);
        const int top = MulDiv(48, dpi, 96);
        const int footer = MulDiv(10, dpi, 96);
        const int width = std::max(0, (int)LOWORD(l) - margin * 2);
        const int height = std::max(0, (int)HIWORD(l) - top - footer);
        SetWindowPos(state->edit, nullptr, margin, top, width, height, SWP_NOZORDER);
        return 0;
    }
    if (m == WM_COMMAND && LOWORD(w) == 1 && HIWORD(w) == BN_CLICKED && state && state->edit)
    {
        state->wrapped = SendMessageW(state->wrap, BM_GETCHECK, 0, 0) == BST_CHECKED;
        DWORD selectionStart = 0, selectionEnd = 0;
        SendMessageW(state->edit, EM_GETSEL, (WPARAM)&selectionStart, (LPARAM)&selectionEnd);
        HWND replacement = CreateCodeViewerEdit(h, state, selectionStart, selectionEnd);
        if (replacement)
        {
            DestroyWindow(state->edit);
            state->edit = replacement;
            SetFocus(state->edit);
        }
        else
        {
            state->wrapped = !state->wrapped;
            SendMessageW(state->wrap, BM_SETCHECK, state->wrapped ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        return 0;
    }
    if (m == WM_CTLCOLOREDIT || m == WM_CTLCOLORSTATIC || m == WM_CTLCOLORBTN)
    {
        HDC dc = (HDC)w;
        SetTextColor(dc, C_TEXT);
        SetBkColor(dc, m == WM_CTLCOLOREDIT ? C_CARD : C_BG);
        return (LRESULT)(m == WM_CTLCOLOREDIT ? g_cardBrush : g_bgBrush);
    }
    if (m == WM_NCDESTROY)
    {
        SetWindowLongPtrW(h, GWLP_USERDATA, 0);
        const auto result = DefWindowProcW(h, m, w, l);
        if (state && state->ownedByWindow) delete state;
        return result;
    }
    return DefWindowProcW(h, m, w, l);
}
bool ShowCodeViewer(HWND owner, const std::wstring &language, const std::wstring &source)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = CodeViewerProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = g_bgBrush;
        wc.lpszClassName = L"MayaNativeCodeViewer";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        registered = true;
    }
    auto state = std::make_unique<CodeViewerState>();
    state->language = language;
    state->source = source;
    std::wstring title = L"Maya code block";
    if (!language.empty()) title += L" — " + language;
    HWND rootOwner = GetAncestor(owner, GA_ROOT);
    if (!rootOwner) rootOwner = owner;
    RECT ownerRect{};
    GetWindowRect(rootOwner, &ownerRect);
    const int dpi = GetDpiForWindow(rootOwner);
    const int width = MulDiv(920, dpi, 96), height = MulDiv(640, dpi, 96);
    auto *data = state.release();
    HWND window = CreateWindowExW(WS_EX_TOOLWINDOW, L"MayaNativeCodeViewer", title.c_str(),
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  (ownerRect.left + ownerRect.right - width) / 2,
                                  (ownerRect.top + ownerRect.bottom - height) / 2,
                                  width, height, rootOwner, nullptr, g_inst, data);
    if (!window)
    {
        delete data;
        return false;
    }
    data->ownedByWindow = true;
    return true;
}
void DrawInlineImagePreviews(HWND h)
{
    if (h != g_transcript || g_page != CHAT || g_inlineImagePreviews.empty()) return;
    if (!MayaEnsureGdiplus(g_gdiplusToken)) return;
    RECT client{};
    GetClientRect(h, &client);
    if (client.right < 32 || client.bottom < 32) return;
    HDC dc = GetDC(h);
    if (!dc) return;
    {
    Graphics graphics(dc);
    graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    const int boxWidth = MulDiv(152, GetDpiForWindow(h), 96);
    const int boxHeight = MulDiv(92, GetDpiForWindow(h), 96);
    const int lineHeight = MulDiv(20, GetDpiForWindow(h), 96);
    std::vector<bool> visible(g_inlineImagePreviews.size(), false);
    size_t cached = 0;
    for (const auto &image : g_inlineImagePreviews) cached += image.thumbnail != nullptr;
    for (size_t i = 0; i < g_inlineImagePreviews.size(); ++i)
    {
        auto &image = g_inlineImagePreviews[i];
        POINTL point{};
        SendMessageW(h, EM_POSFROMCHAR, (WPARAM)&point, image.position);
        const int x = std::max(4, (int)point.x), y = point.y + lineHeight;
        if (point.x < 0 || y + boxHeight <= 0 || y >= client.bottom || x >= client.right) continue;
        visible[i] = true;
        if (!image.thumbnail && !image.attempted)
        {
            image.attempted = true;
            Bitmap source(image.file.c_str(), FALSE);
            if (source.GetLastStatus() == Ok && source.GetWidth() && source.GetHeight() &&
                (uint64_t)source.GetWidth() * source.GetHeight() <= 40000000ULL)
            {
                const auto size = MayaInlineThumbnailSize(source.GetWidth(), source.GetHeight());
                std::unique_ptr<Image> thumbnail(source.GetThumbnailImage(size.first, size.second, nullptr, nullptr));
                if (thumbnail && thumbnail->GetLastStatus() == Ok)
                {
                    image.thumbnail = std::move(thumbnail);
                    ++cached;
                }
            }
        }
        const int width = std::min(boxWidth, (int)client.right - x - 4);
        if (width <= 0) continue;
        const Rect box(x, y, width, boxHeight);
        SolidBrush background(Color(255, GetRValue(C_CARD), GetGValue(C_CARD), GetBValue(C_CARD)));
        graphics.FillRectangle(&background, box);
        if (image.thumbnail)
        {
            const int drawX = x + (width - (int)image.thumbnail->GetWidth()) / 2;
            graphics.DrawImage(image.thumbnail.get(), drawX, y + 4,
                               (INT)image.thumbnail->GetWidth(), (INT)image.thumbnail->GetHeight());
        }
        Pen border(Color(255, GetRValue(C_LINE), GetGValue(C_LINE), GetBValue(C_LINE)));
        graphics.DrawRectangle(&border, box);
    }
    if (cached > 8)
        for (size_t i = 0; i < g_inlineImagePreviews.size() && cached > 8; ++i)
            if (!visible[i] && g_inlineImagePreviews[i].thumbnail)
            {
                g_inlineImagePreviews[i].thumbnail.reset();
                --cached;
            }
    }
    ReleaseDC(h, dc);
}
LRESULT CALLBACK TranscriptSubclassProc(HWND h, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data)
{
    (void)data;
    const auto result = DefSubclassProc(h, message, w, l);
    if (message == WM_PAINT) DrawInlineImagePreviews(h);
    if (message == WM_PAINT || message == WM_VSCROLL || message == WM_MOUSEWHEEL || message == WM_SIZE || message == WM_KEYDOWN)
        maya_video::RepositionInline(h);
    if (message == WM_NCDESTROY)
    {
        maya_video::CloseInline();
        g_inlineVideoPath.clear();
        RemoveWindowSubclass(h, TranscriptSubclassProc, id);
    }
    return result;
}
bool OpenAttachmentFile(HWND owner, const fs::path &path)
{
    const auto kind = MayaAttachmentType(path);
    if (kind == MayaAttachmentKind::VIDEO) return maya_video::Open(owner, path);
    if (kind == MayaAttachmentKind::IMAGE && ShowImagePreview(owner, path)) return true;
    return (INT_PTR)ShellExecuteW(owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}
void ShowAttachmentMenu(HWND owner)
{
    if (g_attachments.empty())
    {
        SetText(g_status, L"No attachments are queued.");
        return;
    }
    if (g_attachments.size() > 30000)
    {
        SetText(g_status, L"Too many attachments to show in the menu.");
        return;
    }
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (size_t i = 0; i < g_attachments.size(); ++i)
    {
        auto label = g_attachments[i].filename().wstring();
        for (size_t at = 0; (at = label.find(L'&', at)) != std::wstring::npos; at += 2)
            label.insert(at, 1, L'&');
        HMENU item = CreatePopupMenu();
        const auto kind = MayaAttachmentType(g_attachments[i]);
        const bool image = kind == MayaAttachmentKind::IMAGE;
        const wchar_t *openLabel = image ? L"Preview image" : kind == MayaAttachmentKind::VIDEO ? L"Play video" : L"Open with Windows app";
        if (!item || !AppendMenuW(item, MF_STRING, ATTACHMENT_OPEN_BASE + (UINT)i, openLabel) ||
            !AppendMenuW(item, MF_STRING, ATTACHMENT_REMOVE_BASE + (UINT)i, L"Remove from message") ||
            !AppendMenuW(menu, MF_POPUP, (UINT_PTR)item, label.c_str()))
        {
            if (item) DestroyMenu(item);
            DestroyMenu(menu);
            return;
        }
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ATTACHMENT_REMOVE_ALL, L"Remove all attachments");
    RECT r{};
    GetWindowRect(g_attachmentMenu, &r);
    const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, r.left, r.bottom, 0, owner, nullptr);
    DestroyMenu(menu);
    if (selected >= ATTACHMENT_OPEN_BASE && selected < ATTACHMENT_OPEN_BASE + g_attachments.size())
    {
        const auto &path = g_attachments[selected - ATTACHMENT_OPEN_BASE];
        const auto kind = MayaAttachmentType(path);
        const bool image = kind == MayaAttachmentKind::IMAGE;
        const bool video = kind == MayaAttachmentKind::VIDEO;
        const bool opened = OpenAttachmentFile(owner, path);
        SetText(g_status, opened ? (image ? L"Image preview opened." : video ? L"Video player opened." : L"Opened attachment with its Windows default app.") :
                                   L"Windows could not preview or open that attachment.");
    }
    else if (selected >= ATTACHMENT_REMOVE_BASE && selected < ATTACHMENT_REMOVE_BASE + g_attachments.size())
    {
        const auto index = selected - ATTACHMENT_REMOVE_BASE;
        const auto name = g_attachments[index].filename().wstring();
        g_attachments.erase(g_attachments.begin() + index);
        if (index < g_attachmentSnapshots.size()) g_attachmentSnapshots.erase(g_attachmentSnapshots.begin() + index);
        SetText(g_status, L"Removed attachment: " + name);
        UpdateComposerHeight();
        ScheduleContextMeter();
    }
    else if (selected == ATTACHMENT_REMOVE_ALL)
    {
        g_attachments.clear();
        g_attachmentSnapshots.clear();
        SetText(g_status, L"Removed all queued attachments.");
        UpdateComposerHeight();
        ScheduleContextMeter();
    }
}
void SetPullProgress(const Json &pull)
{
    if (!g_pullProgress) return;
    if (!pull.is_object())
    {
        ShowWindow(g_pullProgress, SW_HIDE);
        return;
    }
    const bool active = PullStatusActive(pull);
    const bool failed = pull.value("state", std::string()) == "error";
    ShowWindow(g_pullProgress, (active || failed) && g_page == MODELS && !g_modelSettingsOpen && !g_leftoverOpen
                                   ? SW_SHOW : SW_HIDE);
    if (!active && !failed) return;
    SendMessageW(g_pullProgress, PBM_SETPOS, PullProgressBarPosition(pull), 0);
}
std::wstring ByteSize(uint64_t bytes)
{
    std::wostringstream out;
    if (bytes >= 1000000000ULL) out << std::fixed << std::setprecision(bytes < 10000000000ULL ? 1 : 0) << (bytes / 1e9) << L" GB";
    else if (bytes >= 1000000ULL) out << std::fixed << std::setprecision(0) << (bytes / 1e6) << L" MB";
    else if (bytes >= 1000ULL) out << std::fixed << std::setprecision(0) << (bytes / 1e3) << L" KB";
    else out << bytes << L" bytes";
    return out.str();
}
void PopulateLeftovers()
{
    if (!g_leftoverList) return;
    const auto now = std::time(nullptr);
    SendMessageW(g_leftoverList, LB_RESETCONTENT, 0, 0);
    const auto leftovers = g_modelData.contains("leftovers") && g_modelData["leftovers"].is_object()
        ? g_modelData["leftovers"] : Json::object();
    const auto items = leftovers.contains("items") && leftovers["items"].is_array()
        ? leftovers["items"] : Json::array();
    if (items.is_array())
        for (const auto &item : items)
        {
            const auto name = item.value("name", std::string("Unknown item"));
            std::wstring row = W(name) + L"  ·  " + ByteSize(item.value("bytes", uint64_t(0)));
            if (item.contains("modified") && item["modified"].is_number())
                row += L"  ·  changed " + W(ModelRelativeTime(item["modified"].get<double>(), now));
            if (item.value("folder", false)) row += L"  [folder]";
            if (item.value("pack", false)) row += L"  [Maya pack]";
            if (item.value("hub", false)) row += L"  [Hugging Face]";
            SendMessageW(g_leftoverList, LB_ADDSTRING, 0, (LPARAM)row.c_str());
        }
    SetText(g_leftoverDetails, items.is_array() && items.empty()
        ? L"No unused files were found. Maya rechecks before every deletion."
        : L"Select an item to inspect its exact path, type, and size.");
    EnableWindow(g_leftoverDelete, FALSE);
}
void UpdateLeftoverDetails()
{
    if (!g_leftoverList || !g_modelData.contains("leftovers")) return;
    const auto items = g_modelData["leftovers"].is_object() && g_modelData["leftovers"].contains("items") &&
                       g_modelData["leftovers"]["items"].is_array()
        ? g_modelData["leftovers"]["items"] : Json::array();
    const int index = (int)SendMessageW(g_leftoverList, LB_GETCURSEL, 0, 0);
    if (!items.is_array() || index < 0 || index >= (int)items.size())
    {
        EnableWindow(g_leftoverDelete, FALSE);
        return;
    }
    const auto &item = items[index];
    std::wstring text = W(item.value("name", std::string("Unused file")));
    text += item.value("folder", false) ? L"\r\nType: folder and its contents" : L"\r\nType: file";
    text += L"\r\nSize: " + ByteSize(item.value("bytes", uint64_t(0)));
    text += L"\r\nPath: " + W(item.value("path", std::string()));
    if (item.contains("modified") && item["modified"].is_number())
        text += L"\r\nChanged: " + W(ModelRelativeTime(item["modified"].get<double>()));
    if (item.value("pack", false)) text += L"\r\nContains a Maya-built model pack.";
    if (item.value("hub", false)) text += L"\r\nContains a Hugging Face download.";
    text += L"\r\nMaya rechecks that no model uses it before deleting.";
    SetText(g_leftoverDetails, text);
    EnableWindow(g_leftoverDelete, ModelLeftoverDeleteEnabled(true, g_busy != J_NONE,
        g_modelSwitchRequestInFlight, g_switchRestartWatching, g_modelSettingsRestartWatching));
}
void Place(HWND h, int x, int y, int w, int z)
{
    if (h)
        MoveWindow(h, x, y, w, NativeComboLayoutHeight(h, z), !g_deferLayoutRedraw);
}
void RaiseHeaderControls()
{
    if (g_headerLayer && IsWindow(g_headerLayer))
        SetWindowPos(g_headerLayer, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    for (auto h : {g_profileBox, g_sidebarToggle, g_start, g_stop, g_restart, g_update,
                   g_compactToggle, g_status, g_detail, g_undoDelete})
        if (h && IsWindow(h)) SetWindowPos(h, HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
HFONT MakeFont(int pt, bool bold = false, const wchar_t *face = L"Segoe UI Variable")
{
    LOGFONTW f{};
    f.lfHeight = -MulDiv(pt, GetDpiForSystem(), 72);
    f.lfWeight = bold ? FW_SEMIBOLD : FW_NORMAL;
    f.lfQuality = CLEARTYPE_QUALITY;
    wcscpy_s(f.lfFaceName, face);
    return CreateFontIndirectW(&f);
}
std::wstring StatusText()
{
    if (g_apiConnection == NativeApiConnection::API_KEY || g_apiConnection == NativeApiConnection::UNREACHABLE)
        return NativeApiConnectionText(g_apiConnection);
    if (g_apiConnection == NativeApiConnection::CONNECTING && g_online)
        return NativeApiConnectionText(g_apiConnection);
    return g_online ? L"Connected" : L"Maya is offline";
}
void SetLifecycleButtons(bool online, bool busy = false)
{
    busy = busy || g_switchRestartWatching || g_modelSwitchRequestInFlight;
    SetText(g_start, g_updateWatching ? L"Updating…" : busy ? L"Working…" : online ? L"Running" : L"Start");
    if (g_start)
        EnableWindow(g_start, !busy && !online && !g_updateWatching);
    if (g_stop)
        EnableWindow(g_stop, !busy && online && !g_updateWatching);
    if (g_restart)
        EnableWindow(g_restart, !busy && online && !g_updateWatching);
    UpdateModelSwitchButton();
}
MayaHttp Api()
{
    return MayaHttp(Configuration(g_root, g_profile));
}
Json MessageRecord(const Message &m)
{
    Json j = {{"role", m.role}, {"text", m.text}, {"reasoning", m.reasoning}, {"error", m.error},
              {"meta", m.meta}, {"job_id", m.jobId},
              {"usage", m.usage}, {"timings", m.timings}, {"stopped", m.stopped}, {"think_secs", m.thinkSecs}};
    Json files = Json::array();
    for (const auto &f : m.files)
        files.push_back(U(f.wstring()));
    j["files"] = std::move(files);
    j["tools"] = m.tools;
    j["tool_limit"] = m.toolLimit;
    return j;
}
bool StoreChatAttachment(const Chat &chat, const fs::path &source, const std::string &snapshot,
                         fs::path &stored, std::string &error)
{
    const auto sourcePath = source.lexically_normal();
    if (MayaPathIsWithin(fs::path(g_chatDir) / L"attachments" / chat.id, sourcePath))
    {
        std::string current;
        if (MayaReadFileBytes(sourcePath, snapshot.size(), current) && current == snapshot)
        {
            stored = sourcePath;
            return true;
        }
    }
    std::error_code ec;
    stored = MayaStoredAttachmentPath(g_chatDir, chat.id, NewId(), source.filename());
    fs::create_directories(stored.parent_path(), ec);
    if (ec)
    {
        error = ec.message();
        return false;
    }
    HANDLE out = CreateFileW(stored.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (out == INVALID_HANDLE_VALUE)
    {
        error = "could not create stored attachment";
        return false;
    }
    DWORD written = 0;
    bool ok = snapshot.empty() || (snapshot.size() <= MAXDWORD &&
        WriteFile(out, snapshot.data(), (DWORD)snapshot.size(), &written, nullptr) && written == snapshot.size());
    if (ok) ok = FlushFileBuffers(out) != FALSE;
    CloseHandle(out);
    if (!ok)
    {
        error = "could not write stored attachment";
        return false;
    }
    return true;
}
bool AppendHistoryRecord(const Chat &c, const Json &record)
{
    auto path = fs::path(g_chatDir) / (c.id + L".jsonl");
    HANDLE h = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    auto line = record.dump(-1, ' ', false, Json::error_handler_t::replace) + "\n";
    DWORD n = 0;
    const BOOL ok = WriteFile(h, line.data(), (DWORD)line.size(), &n, nullptr);
    CloseHandle(h);
    return ok && n == line.size();
}
bool AppendLog(const Chat &c, const Message &m) { return AppendHistoryRecord(c, MessageRecord(m)); }
bool SaveChatMetadata(const Chat &c)
{
    const auto path = fs::path(g_chatDir) / (c.id + L".meta.json");
    const auto temp = fs::path(g_chatDir) / (c.id + L"." + NewId() + L".tmp");
    const auto bytes = SerializeChatMetadata({U(c.title), c.system, U(c.folder), c.thinking, c.thinkingFor, c.pinned,
                                               c.pendingJob, c.titleManual, c.hasAblate, c.ablate, c.ablateFor,
                                               c.thinkingPicked}).dump(2);
    {
        std::ofstream f(temp, std::ios::binary | std::ios::out);
        if (!f || !f.write(bytes.data(), (std::streamsize)bytes.size()) || !f.flush())
        {
            SetText(g_status, L"Could not save chat details; the message history remains intact.");
            return false;
        }
    }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        SetText(g_status, L"Could not save chat details; the message history remains intact.");
        return false;
    }
    return true;
}
bool SaveFolders()
{
    std::vector<FolderMetadata> values;
    for (const auto &folder : g_folders)
        values.push_back({U(folder.id), U(folder.name), folder.collapsed});
    const auto data = SerializeFolders(values);
    const auto path = fs::path(g_chatDir) / L"folders.json";
    const auto temp = fs::path(g_chatDir) / (L"folders." + NewId() + L".tmp");
    const auto bytes = data.dump(2);
    {
        std::ofstream f(temp, std::ios::binary | std::ios::out);
        if (!f || !f.write(bytes.data(), (std::streamsize)bytes.size()) || !f.flush())
        {
            SetText(g_status, L"Could not save chat folders.");
            return false;
        }
    }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        SetText(g_status, L"Could not save chat folders.");
        return false;
    }
    return true;
}
void LoadFolders()
{
    g_folders.clear();
    std::ifstream f(fs::path(g_chatDir) / L"folders.json", std::ios::binary);
    if (!f)
        return;
    const auto data = Json::parse(f, nullptr, false);
    for (const auto &folder : ParseFolders(data))
        g_folders.push_back({W(folder.id), W(folder.name), folder.collapsed});
}
void LoadChatPreferences()
{
    g_useTools = true;
    g_sidebarOpen = true;
    std::ifstream in(fs::path(g_chatDir) / L"preferences.json", std::ios::binary);
    if (in) try
    {
        Json prefs;
        in >> prefs;
        g_profile = ChatSavedProfile(prefs);
        g_defaultSystem = ChatDefaultInstructions(prefs);
        g_showThinking = ChatShowThinking(prefs);
        g_useTools = ChatMcpEnabled(prefs);
        g_monitorGpuOpenKey = ChatMonitorGpuOpenKey(prefs);
        g_sidebarOpen = ChatSidebarOpen(prefs);
        g_savedCurrentChat = ChatCurrentChatId(prefs);
        g_savedModelId = ChatSavedModelId(prefs);
        const std::string theme = prefs.is_object() ? prefs.value("theme", std::string("dark")) : std::string("dark");
        g_themeChoice = theme == "system" || theme == "light" ? theme : "dark";
        g_schemeChoice = ChatSavedScheme(prefs);
        g_samplingPrefs = prefs.is_object() && prefs.contains("sampling") && prefs["sampling"].is_object()
                              ? prefs["sampling"]
                              : Json::object();
        g_turboSnapshot = prefs.is_object() && prefs.contains("turbo_snapshot") ? prefs["turbo_snapshot"] : Json(nullptr);
    }
    catch (...)
    {
        g_profile = L"maya-ds4-abliterated.json";
        g_defaultSystem.clear();
        g_monitorGpuOpenKey.clear();
        g_sidebarOpen = true;
        g_savedCurrentChat.clear();
    }
    SendMessageW(g_profileBox, CB_SETCURSEL, ChatProfileComboIndex(g_profile), 0);
    if (g_defaultInstructions) SetText(g_defaultInstructions, W(g_defaultSystem));
    if (g_showThinkingCheck)
        SendMessageW(g_showThinkingCheck, BM_SETCHECK, g_showThinking ? BST_CHECKED : BST_UNCHECKED, 0);
    if (g_toolsCheck)
        SendMessageW(g_toolsCheck, BM_SETCHECK, g_useTools ? BST_CHECKED : BST_UNCHECKED, 0);
    const wchar_t *themes[] = {L"System", L"Dark", L"Light"};
    for (const auto *theme : themes) SendMessageW(g_themeBox, CB_ADDSTRING, 0, (LPARAM)theme);
    SendMessageW(g_themeBox, CB_SETCURSEL, ChatThemeComboIndex(g_themeChoice), 0);
    const wchar_t *schemes[] = {L"Default", L"Violet"};
    for (const auto *scheme : schemes) SendMessageW(g_schemeBox, CB_ADDSTRING, 0, (LPARAM)scheme);
    SendMessageW(g_schemeBox, CB_SETCURSEL, ChatSchemeComboIndex(g_schemeChoice), 0);
    ApplyAppearance();
}
bool SaveChatPreferences(const std::string &system, bool showThinking)
{
    const auto path = fs::path(g_chatDir) / L"preferences.json";
    const auto temp = fs::path(g_chatDir) / (L"preferences." + NewId() + L".tmp");
    const auto bytes = ChatPreferences(system, showThinking, g_samplingPrefs, g_themeChoice, g_schemeChoice,
                                       g_turboSnapshot, g_useTools, g_monitorGpuOpenKey, g_sidebarOpen,
                                       g_savedCurrentChat, g_savedModelId, Utf8(g_profile)).dump(2);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::out);
        if (!out || !out.write(bytes.data(), (std::streamsize)bytes.size()) || !out.flush())
        {
            SetText(g_settingsStatus, L"Could not save manager preferences.");
            return false;
        }
    }
    if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        SetText(g_settingsStatus, L"Could not commit manager preferences; the previous settings are intact.");
        return false;
    }
    return true;
}
bool SaveCurrentChatSelection()
{
    if (g_chat < 0 || g_chat >= (int)g_chats.size()) return false;
    g_savedCurrentChat = g_chats[g_chat].id;
    return SaveChatPreferences(g_defaultSystem, g_showThinking);
}
bool SetMonitorGpuOpen(int index)
{
    const auto rows = MonitorGpuRows(g_metrics);
    if (index < 0 || index >= (int)rows.size()) return false;
    const int previousIndex = g_monitorGpuOpen;
    const auto previousKey = g_monitorGpuOpenKey;
    g_monitorGpuOpen = previousIndex == index ? -1 : index;
    g_monitorGpuOpenKey = g_monitorGpuOpen < 0 ? std::string() : MonitorGpuRowKey(rows[index]);
    if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
    {
        g_monitorGpuOpen = previousIndex;
        g_monitorGpuOpenKey = previousKey;
        SetText(g_status, L"Could not save the GPU map expansion preference.");
        return false;
    }
    return true;
}
void Layout();
void SetPageControls();
void SelectModelSettingsTab(int tab);
void UpdateModelSettingsTabs();
class NativeLayoutTransition
{
    bool previous_;
    bool hasDirtyRect_;
    RECT dirtyRect_{};
public:
    explicit NativeLayoutTransition(const RECT *dirtyRect = nullptr)
        : previous_(g_deferLayoutRedraw), hasDirtyRect_(dirtyRect != nullptr)
    {
        if (dirtyRect) dirtyRect_ = *dirtyRect;
        g_deferLayoutRedraw = true;
    }
    ~NativeLayoutTransition()
    {
        g_deferLayoutRedraw = previous_;
        if (!previous_ && g_hwnd)
            RedrawWindow(g_hwnd, hasDirtyRect_ ? &dirtyRect_ : nullptr, nullptr,
                         RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_NOERASE);
    }
};
void ToggleSidebar()
{
    const bool previous = g_sidebarOpen;
    g_sidebarOpen = !previous;
    if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
    {
        g_sidebarOpen = previous;
        SetText(g_status, L"Could not save the sidebar choice; the previous layout was restored.");
    }
    NativeLayoutTransition transition;
    Layout();
}
const char *SamplingKeys[] = {"temperature", "top_p", "top_k", "min_p", "max_tokens", "seed",
                              "repetition_penalty", "frequency_penalty", "presence_penalty"};
const wchar_t *SamplingLabels[] = {L"Temperature", L"Top-p", L"Top-k", L"Min-p", L"Max tokens", L"Seed",
                                    L"Repetition penalty", L"Frequency penalty", L"Presence penalty"};
void SetSamplingFilterControls(bool enabled)
{
    for (int i = 1; i <= 3; ++i) EnableWindow(g_samplingFields[i], enabled);
}
void UpdateSamplingFilterControls()
{
    const auto text = ControlText(g_samplingFields[0]);
    char *end = nullptr;
    const double temperature = std::strtod(text.c_str(), &end);
    const bool valid = end != text.c_str() && *end == '\0' && std::isfinite(temperature);
    SetSamplingFilterControls(valid && ChatSamplingUsesFilters(Json{{"temperature", temperature}}));
}
void LoadChatDefaultsControls()
{
    if (!g_samplingEffort || !g_samplingPreset) return;
    g_loadingChatDefaults = true;
    const auto model = g_health.value("model", std::string());
    auto values = ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()));
    const auto family = ChatSamplingFamily(model);
    const auto efforts = ChatSamplingEfforts(family);
    values["family"] = family;
    SendMessageW(g_samplingEffort, CB_RESETCONTENT, 0, 0);
    int selectedEffort = 0;
    for (size_t i = 0; i < efforts.size(); ++i)
    {
        SendMessageW(g_samplingEffort, CB_ADDSTRING, 0, (LPARAM)W(ChatSamplingEffortLabel(efforts[i])).c_str());
        if (efforts[i] == values.value("reasoning_effort", std::string())) selectedEffort = (int)i;
    }
    SendMessageW(g_samplingEffort, CB_SETCURSEL, selectedEffort, 0);
    for (size_t i = 0; i < 9; ++i)
    {
        const auto &v = values[SamplingKeys[i]];
        SetText(g_samplingFields[i], v.is_null() ? L"" : W(v.dump()));
    }
    SendMessageW(g_samplingPreset, CB_RESETCONTENT, 0, 0);
    for (const auto *name : {L"Precise", L"Balanced", L"Creative", L"Custom"})
        SendMessageW(g_samplingPreset, CB_ADDSTRING, 0, (LPARAM)name);
    int preset = 3;
    const auto temp = values.value("temperature", 0.6);
    const auto topP = values.value("top_p", 0.95);
    const auto topK = values.value("top_k", 20);
    const auto minP = values.value("min_p", 0.0);
    if (temp == 0) preset = 0;
    else if (temp == 0.6 && topP == 0.95 && topK == 20 && minP == 0) preset = 1;
    else if (temp == 1.0 && topP == 0.95 && topK == 0 && minP == 0.05) preset = 2;
    SendMessageW(g_samplingPreset, CB_SETCURSEL, preset, 0);
    SetSamplingFilterControls(ChatSamplingUsesFilters(values));
    g_loadingChatDefaults = false;
}
void UpdateTurboButton()
{
    if (!g_turboButton) return;
    if (g_busy == J_TURBO_SET) SetText(g_turboButton, L"Switching Turbo…");
    else if (g_turboData.value("unavailable", false)) SetText(g_turboButton, L"Turbo unavailable");
    else if (!g_turboData.contains("on")) SetText(g_turboButton, L"Turbo: Check");
    else SetText(g_turboButton, g_turboData.value("on", false) ? L"Turbo: ON" : L"Turbo: OFF");
    EnableWindow(g_turboButton, g_busy != J_TURBO_SET && !g_turboData.value("unavailable", false) &&
                               g_turboData.contains("on"));
}
bool RestoreNativeTurboPrefs()
{
    if (!g_turboSnapshot.is_object()) return true;
    std::string thinking;
    const auto current = g_chat >= 0 && g_chat < (int)g_chats.size() ? g_chats[g_chat].thinking : std::string();
    g_samplingPrefs = NativeTurboLeave(g_samplingPrefs, g_turboSnapshot, current, thinking);
    if (!thinking.empty() && g_chat >= 0 && g_chat < (int)g_chats.size())
    {
        g_chats[g_chat].thinking = thinking;
        SaveChatMetadata(g_chats[g_chat]);
    }
    g_turboSnapshot = nullptr;
    const bool saved = SaveChatPreferences(g_defaultSystem, g_showThinking);
    LoadChatDefaultsControls();
    ApplyChatThinking();
    return saved;
}
Json ReadChatDefaultsControls()
{
    Json values = Json::object();
    const auto model = g_health.value("model", std::string());
    values["family"] = ChatSamplingFamily(model);
    const int effortIndex = (int)SendMessageW(g_samplingEffort, CB_GETCURSEL, 0, 0);
    const auto efforts = ChatSamplingEfforts(values["family"].get<std::string>());
    if (effortIndex < 0 || effortIndex >= (int)efforts.size()) throw std::runtime_error("Choose a reasoning level.");
    values["reasoning_effort"] = efforts[effortIndex];
    for (size_t i = 0; i < 9; ++i)
    {
        const auto text = ControlText(g_samplingFields[i]);
        if (text.empty()) values[SamplingKeys[i]] = nullptr;
        else
        {
            size_t at = 0;
            const auto number = std::stod(text, &at);
            if (at != text.size() || !std::isfinite(number))
                throw std::runtime_error(std::string(SamplingKeys[i]) + " must be numeric.");
            values[SamplingKeys[i]] = number;
        }
    }
    return ValidateChatSampling(values);
}
void ScheduleSharedDefaultsSave(const Json &values)
{
    if (!g_sharedDefaultsOn || !g_hwnd) return;
    g_sharedRequest = SharedChatDefaults(values);
    g_sharedDefaultsSavePending = true;
    KillTimer(g_hwnd, 12);
    SetTimer(g_hwnd, 12, 600, nullptr);
}
bool SaveSamplingDefaultsFromControls(bool updateEmptyChatThinking)
{
    try
    {
        const auto values = ReadChatDefaultsControls();
        const auto updated = SaveChatSampling(g_samplingPrefs, g_health.value("model", std::string()), values);
        if (!NativeSamplingAutosaveNeeded(g_loadingChatDefaults, updated != g_samplingPrefs, true)) return true;
        const auto previous = g_samplingPrefs;
        g_samplingPrefs = updated;
        if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
        {
            g_samplingPrefs = previous;
            return false;
        }
        if (updateEmptyChatThinking && g_chat >= 0 && g_chat < (int)g_chats.size() &&
            g_chats[g_chat].messages.empty())
        {
            g_chats[g_chat].thinking = values.value("reasoning_effort", std::string("high"));
            g_chats[g_chat].thinkingFor = g_health.value("model", std::string());
            g_chats[g_chat].thinkingPicked = false;
            SaveChatMetadata(g_chats[g_chat]);
            ApplyChatThinking();
        }
        if (g_sharedDefaultsOn) ScheduleSharedDefaultsSave(values);
        SetText(g_samplingStatus, g_sharedDefaultsOn
            ? L"Chat defaults saved; shared API defaults will follow shortly."
            : L"Chat defaults saved for this model family.");
        return true;
    }
    catch (const std::exception &e)
    {
        SetText(g_samplingStatus, W(e.what()));
        return false;
    }
}
void LoadChats()
{
    g_chats.clear();
    std::error_code ec;
    for (auto &e : fs::directory_iterator(g_chatDir, ec))
    {
        if (ec || e.path().extension() != L".jsonl")
            continue;
        Chat c;
        c.id = e.path().stem().wstring();
        c.title = L"New chat";
        {
            std::ifstream meta(fs::path(g_chatDir) / (c.id + L".meta.json"), std::ios::binary);
            if (meta)
            {
                const auto saved = ParseChatMetadata(Json::parse(meta, nullptr, false));
                if (!saved.title.empty())
                    c.title = W(saved.title);
                c.system = saved.system;
                c.folder = W(saved.folder);
                c.thinking = saved.thinking;
                c.thinkingFor = saved.thinkingFor;
                c.hasAblate = saved.hasAblate;
                c.ablate = saved.ablate;
                c.ablateFor = saved.ablateFor;
                c.thinkingPicked = saved.thinkingPicked;
                c.pinned = saved.pinned;
                c.pendingJob = saved.pendingJob;
                c.titleManual = saved.titleManual;
            }
        }
        std::ifstream f(e.path(), std::ios::binary);
        std::string line;
        std::vector<Json> records;
        while (std::getline(f, line))
        {
            auto j = Json::parse(line, nullptr, false);
            if (!j.is_discarded())
                ApplyChatHistoryRecord(records, j);
        }
        for (const auto &j : records)
        {
            Message m;
            m.role = j.value("role", std::string());
            m.text = j.value("text", std::string());
            m.reasoning = j.value("reasoning", std::string());
            m.error = j.value("error", std::string());
            m.jobId = j.value("job_id", std::string());
            m.thinkSecs = j.contains("think_secs") && j["think_secs"].is_number()
                              ? j["think_secs"].get<double>()
                              : 0.0;
            m.meta = j.value("meta", std::string());
            m.usage = j.value("usage", Json::object());
            m.timings = j.value("timings", Json::object());
            m.stopped = j.value("stopped", false);
            m.tools = j.value("tools", Json::array());
            m.toolLimit = j.value("tool_limit", 0);
            if (j.contains("files") && j["files"].is_array())
                for (auto &x : j["files"])
                    if (x.is_string())
                        m.files.emplace_back(W(x.get<std::string>()));
            if (c.title == L"New chat" && m.role == "user")
                c.title = W(m.text.substr(0, 80));
            c.messages.push_back(std::move(m));
        }
        if (!c.pendingJob.empty() && !c.messages.empty() &&
            NativeChatLoadedJobShouldClearPending(c.messages.back().role == "assistant",
                c.messages.back().jobId == c.pendingJob, !c.messages.back().error.empty(), c.messages.back().stopped))
        {
            c.pendingJob.clear();
            SaveChatMetadata(c);
        }
        g_chats.push_back(std::move(c));
    }
    std::sort(g_chats.begin(), g_chats.end(), [](const Chat &a, const Chat &b) {
        std::error_code ea, eb;
        auto ta = fs::last_write_time(fs::path(g_chatDir) / (a.id + L".jsonl"), ea),
             tb = fs::last_write_time(fs::path(g_chatDir) / (b.id + L".jsonl"), eb);
        return !ea && !eb && ta > tb;
    });
    std::vector<std::wstring> ids;
    ids.reserve(g_chats.size());
    for (const auto &chat : g_chats) ids.push_back(chat.id);
    g_chat = ChatPreferredIndex(ids, g_savedCurrentChat);
    if (g_chat >= 0) g_savedCurrentChat = g_chats[g_chat].id;
    ApplyChatThinking();
}
void RenderChat();
void UpdateComposerHeight()
{
    if (!g_input || !g_hwnd) return;
    const int length = GetWindowTextLengthW(g_input);
    std::wstring text((size_t)length + 1, L'\0');
    GetWindowTextW(g_input, text.data(), length + 1);
    text.resize((size_t)length);
    const int lines = (int)SendMessageW(g_input, EM_GETLINECOUNT, 0, 0);
    int lineHeight = 18;
    HDC dc = GetDC(g_input);
    if (dc)
    {
        HGDIOBJ oldFont = SelectObject(dc, g_font);
        TEXTMETRICW metrics{};
        if (GetTextMetricsW(dc, &metrics)) lineHeight = metrics.tmHeight + 2;
        if (oldFont) SelectObject(dc, oldFont);
        ReleaseDC(g_input, dc);
    }
    RECT client{};
    GetClientRect(g_hwnd, &client);
    const int height = NativeComposerHeight(lines, text.find(L'\n') != std::wstring::npos,
                                            !g_attachments.empty(), lineHeight, client.bottom);
    if (height != g_inputHeight)
    {
        g_inputHeight = height;
        Layout();
    }
}
void UpdateGreeting();
void LoadChatDefaultsControls();
void Layout();
void BeginJob(Job job, const wchar_t *action = nullptr, bool backgroundPoll = false);
void RequestModelsWhenIdle(bool background);
void CheckForUpdatesIfNeeded();
void UpdateTurboButton();
void UpdateSavingsSidebar()
{
    if (!g_savedLine) return;
    const auto label = SavingsSidebarLabel(g_metrics);
    if (!label.empty()) SetText(g_savedLine, W(label));
    ShowWindow(g_savedLine, g_sidebarOpen && !label.empty() ? SW_SHOW : SW_HIDE);
}
void UpdateMetricsPollTimer(bool minimized, bool foreground)
{
    if (!g_hwnd) return;
    const bool offline = g_apiConnection == NativeApiConnection::API_KEY ||
                        g_apiConnection == NativeApiConnection::UNREACHABLE;
    const UINT interval = NativeMetricsPollIntervalMs(minimized, g_page == MONITOR, offline, foreground);
    if (interval == g_metricsPollIntervalMs) return;
    g_metricsPollIntervalMs = interval;
    SetTimer(g_hwnd, 2, interval, nullptr);
}
void ScheduleContextMeter();
void ShowModelSettingsFields(bool show);
void RequestModelsWhenIdle(bool background)
{
    if (g_busy == J_NONE) BeginJob(J_MODELS, nullptr, background);
    else if (g_busy == J_CHAT) g_refreshModelsAfterChat = true;
    else if (g_busy != J_MODELS) g_refreshModelsWhenIdle = true;
}
void StartQueuedModelRefreshWhenIdle()
{
    const bool idle = g_busy == J_NONE;
    const bool visible = g_page == MODELS || g_chatModelMenuOpen;
    const bool run = NativeQueuedModelRefreshShouldRun(g_refreshModelsWhenIdle, idle, visible);
    if (!idle || !g_refreshModelsWhenIdle) return;
    g_refreshModelsWhenIdle = false;
    if (run)
        BeginJob(J_MODELS, nullptr, true);
}
bool IsSettingsPage(Page page)
{
    return page == SETTINGS || page == CHAT_DEFAULTS || page == CONNECT || page == ABOUT;
}
void ShowPage(Page page)
{
    const Page previousPage = g_page;
    const HWND focused = GetFocus();
    if (IsSettingsPage(page) && !IsSettingsPage(g_page)) g_settingsReturnPage = g_page;
    if (page == MONITOR && g_page != MONITOR) g_monitorScrollY = 0;
    if (page == MODELS && g_page != MODELS) g_catalogScrollY = 0;
    g_page = page;
    UpdateMetricsPollTimer(IsIconic(g_hwnd) != FALSE, GetForegroundWindow() == g_hwnd);
    if (previousPage != page)
    {
        InvalidateRect(g_nav[(int)previousPage], nullptr, FALSE);
        InvalidateRect(g_nav[(int)page], nullptr, FALSE);
    }
    {
        RECT client{};
        GetClientRect(g_hwnd, &client);
        RECT dirty{std::min(SidebarWidth(), (int)client.right), std::min(128, (int)client.bottom),
                   client.right, client.bottom};
        NativeLayoutTransition transition(&dirty);
        Layout();
        if (focused && focused != g_hwnd &&
            NativePageFocusMustReset(GetAncestor(focused, GA_ROOT) == g_hwnd, IsWindowVisible(focused) != FALSE,
                                     IsWindowEnabled(focused) != FALSE))
        {
            const HWND first = MayaNextTabStop(g_hwnd, nullptr, FALSE);
            SetFocus(first ? first : g_sidebarToggle);
        }
    }
    if (previousPage != page && g_headerLayer && IsWindow(g_headerLayer))
    {
        RECT headerClient{};
        GetClientRect(g_headerLayer, &headerClient);
        const RECT titleDirty{std::min(SidebarContentLeft(), (int)headerClient.right),
                              std::min(56, (int)headerClient.bottom),
                              (int)headerClient.right, std::min(120, (int)headerClient.bottom)};
        RedrawWindow(g_headerLayer, &titleDirty, nullptr,
                     RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE);
    }
    if (page == MONITOR)
        BeginJob(J_REFRESH);
    if (page == MODELS)
        RequestModelsWhenIdle(false);
    if (page == SETTINGS)
        BeginJob(J_CONTEXT);
    if (page == SAVINGS)
        BeginJob(J_SAVINGS);
    if (page == CHAT_DEFAULTS)
    {
        LoadChatDefaultsControls();
        UpdateTurboButton();
        BeginJob(J_CONTEXT);
        BeginJob(J_TURBO_INFO, nullptr, true);
    }
    if (page == ABOUT) CheckForUpdatesIfNeeded();
}

void CheckForUpdatesIfNeeded()
{
    if (!MayaUpdateCheckEligible(g_updateChecked, g_updateCheckInFlight, g_updateWatching, g_updateRecovering)) return;
    if (g_busy == J_STATUS || g_busy == J_START || g_busy == J_STOP || g_busy == J_RESTART ||
        g_busy == J_UPDATE_START || g_modelSettingsRequestInFlight != J_NONE) return;
    if (g_page == ABOUT) SetText(g_updateSummary, L"Checking for Maya updates…");
    BeginJob(J_UPDATE_CHECK, L"update-check", true);
}

bool TranscriptAtBottom(HWND h)
{
    SCROLLINFO si{sizeof(si), SIF_ALL};
    if (!GetScrollInfo(h, SB_VERT, &si)) return true;
    return ChatScrollAtBottom(si.nMin, si.nMax, si.nPos, si.nPage);
}
void UpdateJumpLatest()
{
    if (g_jumpLatest) ShowWindow(g_jumpLatest, g_page == CHAT && !TranscriptAtBottom(g_transcript) ? SW_SHOW : SW_HIDE);
}
void RichAppend(HWND h, const std::wstring &text, COLORREF color, bool bold = false, bool link = false, bool code = false, bool strike = false, bool italic = false, int listDepth = -2, int fontTwips = 220, int spaceBeforeTwips = 0, bool blockQuote = false)
{
    if (!h || text.empty())
        return;
    SCROLLINFO si{sizeof(si), SIF_ALL};
    const bool follow = !GetScrollInfo(h, SB_VERT, &si) ||
                        ChatScrollAtBottom(si.nMin, si.nMax, si.nPos, si.nPage);
    POINT scroll{};
    SendMessageW(h, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    CHARRANGE cr{};
    SendMessageW(h, EM_EXGETSEL, 0, (LPARAM)&cr);
    SendMessageW(h, EM_SETSEL, -1, -1);
    PARAFORMAT2 pf{};
    pf.cbSize = sizeof(pf);
    pf.dwMask = PFM_STARTINDENT | PFM_OFFSET | PFM_BORDER | (spaceBeforeTwips > 0 ? PFM_SPACEBEFORE : 0);
    pf.dxStartIndent = blockQuote ? MulDiv(210, GetDpiForSystem(), 96) : 0;
    if (spaceBeforeTwips > 0) pf.dySpaceBefore = spaceBeforeTwips;
    if (listDepth >= 0)
    {
        size_t digits = 0;
        while (digits < text.size() && text[digits] >= L'0' && text[digits] <= L'9') ++digits;
        const auto indent = ChatMarkdownListIndents(listDepth, digits, (int)GetDpiForSystem());
        pf.dxStartIndent = indent.first;
        pf.dxOffset = indent.second;
    }
    if (blockQuote)
    {
        // Rich Edit paragraph borders use a fixed 16-color palette, so select the nearest gray for the current surface.
        const WORD borderColorIndex = static_cast<WORD>(GetRValue(C_BG) > 127 ? 15 : 14);
        pf.wBorderWidth = static_cast<WORD>(45);
        pf.wBorders = static_cast<WORD>(1u | (3u << 8) | (borderColorIndex << 12));
    }
    SendMessageW(h, EM_SETPARAFORMAT, 0, (LPARAM)&pf);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD | (link ? CFM_LINK : 0) |
        (strike ? CFM_STRIKEOUT : 0) | (italic ? CFM_ITALIC : 0);
    cf.crTextColor = link ? C_PURPLE_TEXT : color;
    cf.yHeight = MulDiv(fontTwips, GetDpiForSystem(), 96);
    cf.dwEffects = (bold ? CFE_BOLD : 0) | (link ? CFE_LINK : 0) | (strike ? CFE_STRIKEOUT : 0) |
        (italic ? CFE_ITALIC : 0);
    wcscpy_s(cf.szFaceName, code ? L"Cascadia Mono" : L"Segoe UI");
    SendMessageW(h, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    SendMessageW(h, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    if (follow)
        SendMessageW(h, EM_SCROLLCARET, 0, 0);
    else
    {
        SendMessageW(h, EM_EXSETSEL, 0, (LPARAM)&cr);
        SendMessageW(h, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    }
    if (h == g_transcript) UpdateJumpLatest();
}
void RichAppendMarkdown(HWND h, const std::wstring &markdown, COLORREF color, int initialSpaceBeforeTwips = 0)
{
    NativeCodeBlock block;
    bool inBlock = false;
    for (const auto &span : ChatMarkdownSpans(markdown, initialSpaceBeforeTwips))
    {
        if (h == g_transcript && span.fenced && !inBlock)
        {
            inBlock = true;
            block = {};
            block.range.cpMin = GetWindowTextLengthW(h);
            block.language = span.target;
        }
        const auto headingStyle = span.headingStyle != ChatMarkdownStyle::TEXT ? span.headingStyle :
            (span.style == ChatMarkdownStyle::HEADING || span.style == ChatMarkdownStyle::SUBHEADING ? span.style : ChatMarkdownStyle::TEXT);
        const bool heading = headingStyle == ChatMarkdownStyle::HEADING || headingStyle == ChatMarkdownStyle::SUBHEADING;
        const bool bold = heading || span.style == ChatMarkdownStyle::EMPHASIS;
        const int headingFontTwips = headingStyle == ChatMarkdownStyle::HEADING ? 270 : 240;
        const int fontTwips = heading ? (span.style == ChatMarkdownStyle::CODE ? MulDiv(headingFontTwips, 7, 8) : headingFontTwips) : 220;
        const int styleSpaceBeforeTwips = headingStyle == ChatMarkdownStyle::HEADING ? 300 :
            headingStyle == ChatMarkdownStyle::SUBHEADING ? 240 : span.style == ChatMarkdownStyle::HORIZONTAL_RULE ? 300 : 0;
        const int spaceBeforeTwips = std::max(styleSpaceBeforeTwips, span.paragraphSpaceBeforeTwips);
        const bool italic = span.style == ChatMarkdownStyle::ITALIC;
        const bool strike = span.style == ChatMarkdownStyle::STRIKETHROUGH;
        const bool code = span.tableCell || span.style == ChatMarkdownStyle::CODE || span.style == ChatMarkdownStyle::CODE_COMMENT ||
            span.style == ChatMarkdownStyle::CODE_STRING || span.style == ChatMarkdownStyle::CODE_KEYWORD ||
            span.style == ChatMarkdownStyle::CODE_NUMBER || span.style == ChatMarkdownStyle::CODE_FUNCTION ||
            span.style == ChatMarkdownStyle::CODE_ATTRIBUTE || span.style == ChatMarkdownStyle::CODE_VARIABLE;
        const bool link = span.style == ChatMarkdownStyle::LINK || !span.target.empty();
        const bool tableHead = span.style == ChatMarkdownStyle::TABLE_HEADER;
        const auto tint = span.style == ChatMarkdownStyle::CODE_COMMENT ? RGB(124, 132, 173) :
            span.style == ChatMarkdownStyle::CODE_STRING ? RGB(158, 230, 184) :
            span.style == ChatMarkdownStyle::CODE_KEYWORD ? RGB(196, 167, 255) :
            span.style == ChatMarkdownStyle::CODE_ATTRIBUTE ? RGB(255, 212, 121) :
            span.style == ChatMarkdownStyle::CODE_VARIABLE ? RGB(255, 158, 198) :
            span.style == ChatMarkdownStyle::CODE_NUMBER ? RGB(255, 197, 138) :
            (span.style == ChatMarkdownStyle::CODE_FUNCTION || link || tableHead) ? C_CYAN_TEXT :
            (span.blockQuote || span.style == ChatMarkdownStyle::HORIZONTAL_RULE) ? C_MUTED : color;
        CHARRANGE range{};
        if (link && h == g_transcript) range.cpMin = GetWindowTextLengthW(h);
        RichAppend(h, span.text, tint, bold || tableHead, link, code, strike,
                   italic || span.style == ChatMarkdownStyle::CODE_COMMENT, span.listDepth,
                   fontTwips, spaceBeforeTwips, span.blockQuote);
        if (h == g_transcript && span.fenced)
        {
            block.source += span.text;
            block.range.cpMax = GetWindowTextLengthW(h);
        }
        if (h == g_transcript && inBlock && !span.fenced)
        {
            block.complete = true;
            if (block.range.cpMax > block.range.cpMin) g_codeBlocks.push_back(std::move(block));
            inBlock = false;
        }
        if (link && h == g_transcript)
        {
            range.cpMax = GetWindowTextLengthW(h);
            g_markdownLinks.push_back({range, span.target});
        }
    }
    if (h == g_transcript && inBlock && block.range.cpMax > block.range.cpMin)
    {
        block.complete = !ChatMarkdownHasUnclosedFence(markdown);
        g_codeBlocks.push_back(std::move(block));
    }
}
void AppendThoughtSummary(const Chat &chat, size_t index, const Message &message)
{
    const auto key = U(chat.id) + ":" + std::to_string(index);
    const bool live = NativeThoughtIsStreaming(g_busy == J_CHAT, chat.id == g_jobChatId, index,
                                               chat.messages.size(), !g_answer.text.empty());
    const bool expanded = ThoughtIsExpanded(g_expandedThoughts, g_collapsedThoughts, key,
                                            live && g_showThinking);
    if (expanded)
    {
        RichAppend(g_transcript, L"Thinking\r\n", C_MUTED, true);
        RichAppend(g_transcript, W(message.reasoning) + L"\r\n\r\n", C_SOFT);
        const LONG bodyEnd = GetWindowTextLengthW(g_transcript) - 4;
        CHARRANGE range{};
        range.cpMin = GetWindowTextLengthW(g_transcript);
        RichAppend(g_transcript, L"Hide thoughts\r\n\r\n", C_PURPLE_TEXT, false, true);
        range.cpMax = GetWindowTextLengthW(g_transcript);
        g_thoughtLinks.push_back({range, key});
        if (live) g_liveThought = {true, true, key, bodyEnd, g_thoughtLinks.size() - 1};
    }
    else
    {
        std::wstring label = live ? L"Thinking…" : ThoughtSummaryLabel(message.thinkSecs);
        label += L" — click to expand\r\n\r\n";
        CHARRANGE range{};
        range.cpMin = GetWindowTextLengthW(g_transcript);
        RichAppend(g_transcript, label, C_PURPLE_TEXT, false, true);
        range.cpMax = GetWindowTextLengthW(g_transcript);
        g_thoughtLinks.push_back({range, key});
        if (live) g_liveThought = {true, false, key, 0, g_thoughtLinks.size() - 1};
    }
}
void AppendLiveThoughtText(const std::wstring &text)
{
    if (!g_liveThought.active || !g_liveThought.expanded || text.empty() || !g_transcript) return;
    if (g_liveThought.linkIndex >= g_thoughtLinks.size()) return;
    auto &link = g_thoughtLinks[g_liveThought.linkIndex];
    if (link.second != g_liveThought.key) return;

    SCROLLINFO si{sizeof(si), SIF_ALL};
    const bool follow = !GetScrollInfo(g_transcript, SB_VERT, &si) ||
                        ChatScrollAtBottom(si.nMin, si.nMax, si.nPos, si.nPage);
    POINT scroll{};
    SendMessageW(g_transcript, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    CHARRANGE selection{};
    SendMessageW(g_transcript, EM_EXGETSEL, 0, (LPARAM)&selection);

    const LONG position = g_liveThought.bodyEnd;
    SendMessageW(g_transcript, EM_SETSEL, position, position);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_BOLD;
    format.crTextColor = C_SOFT;
    format.yHeight = MulDiv(11 * 20, GetDpiForSystem(), 96);
    wcscpy_s(format.szFaceName, L"Segoe UI");
    SendMessageW(g_transcript, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    SendMessageW(g_transcript, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());

    const LONG added = (LONG)text.size();
    g_liveThought.bodyEnd += added;
    ShiftThoughtRangeAfterInsert(link.first.cpMin, link.first.cpMax, position, added);
    if (follow)
    {
        SendMessageW(g_transcript, EM_SETSEL, -1, -1);
        SendMessageW(g_transcript, EM_SCROLLCARET, 0, 0);
    }
    else
    {
        ShiftThoughtRangeAfterInsert(selection.cpMin, selection.cpMax, position, added);
        SendMessageW(g_transcript, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessageW(g_transcript, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    }
    InvalidateRect(g_transcript, nullptr, FALSE);
}
void AppendToolSummary(const Chat &chat, size_t messageIndex, size_t toolIndex, const Json &tool)
{
    const auto key = U(chat.id) + ":" + std::to_string(messageIndex) + ":" + std::to_string(toolIndex);
    const bool expanded = g_expandedTools.count(key) != 0;
    const auto start = GetWindowTextLengthW(g_transcript);
    if (expanded)
    {
        RichAppend(g_transcript, McpSavedText(tool), tool.value("state", std::string()) == "error" ? C_RED_TEXT : C_CYAN_TEXT);
        RichAppend(g_transcript, L"Hide tool details\r\n\r\n", C_PURPLE_TEXT, false, true);
    }
    else
    {
        Json event = tool;
        event["event"] = tool.contains("result") ? "result" :
                          tool.value("state", std::string()) == "writing" ? "start" : "call";
        event["skipped"] = tool.value("state", std::string()) == "skipped";
        RichAppend(g_transcript, McpCompactText(event) + L"Click to expand tool details\r\n\r\n", C_PURPLE_TEXT, false, true);
    }
    const auto end = GetWindowTextLengthW(g_transcript);
    g_toolLinks.push_back({CHARRANGE{start, end}, key});
}
void AppendLiveAnswerText(const Chat &chat, const std::wstring &text, size_t begin, size_t end, int initialSpaceBeforeTwips = 0)
{
    size_t pos = begin;
    int nextSpaceBeforeTwips = initialSpaceBeforeTwips;
    if (g_answer.tools.is_array())
        for (size_t i = 0; i < g_answer.tools.size(); ++i)
        {
            const auto &tool = g_answer.tools[i];
            if (!tool.is_object()) continue;
            const size_t at = std::clamp(ChatUtf8PrefixCodeUnits(g_answer.text, tool.value("at", size_t(0))),
                                         pos, text.size());
            if (at < begin || at > end) continue;
            if (at > pos)
            {
                RichAppendMarkdown(g_transcript, text.substr(pos, at - pos), C_TEXT, nextSpaceBeforeTwips);
                nextSpaceBeforeTwips = 0;
            }
            AppendToolSummary(chat, chat.messages.size(), i, tool);
            nextSpaceBeforeTwips = 180;
            pos = at;
        }
    if (end > pos) RichAppendMarkdown(g_transcript, text.substr(pos, end - pos), C_TEXT, nextSpaceBeforeTwips);
    if (g_answer.toolLimit > 0)
        RichAppend(g_transcript, L"[Tool calls stopped at the limit of " + std::to_wstring(g_answer.toolLimit) +
                                  L" rounds]\r\n", C_MUTED);
}
void RefreshLiveAnswer(const Chat &chat)
{
    if (!g_liveAnswer.active || !g_transcript) return;
    const auto text = W(g_answer.text);
    const bool hasTools = (g_answer.tools.is_array() && !g_answer.tools.empty()) || g_answer.toolLimit > 0;
    if (hasTools && g_liveAnswer.frozenUntil)
    {
        g_liveAnswer.tailStart = g_liveAnswer.start;
        g_liveAnswer.frozenUntil = 0;
        g_liveAnswer.tailToolLinks = g_liveAnswer.baseToolLinks;
        g_liveAnswer.tailMarkdownLinks = g_liveAnswer.baseMarkdownLinks;
        g_liveAnswer.tailCodeBlocks = g_liveAnswer.baseCodeBlocks;
    }

    SCROLLINFO si{sizeof(si), SIF_ALL};
    const bool follow = !GetScrollInfo(g_transcript, SB_VERT, &si) ||
                        ChatScrollAtBottom(si.nMin, si.nMax, si.nPos, si.nPage);
    POINT scroll{};
    SendMessageW(g_transcript, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    CHARRANGE selection{};
    SendMessageW(g_transcript, EM_EXGETSEL, 0, (LPARAM)&selection);

    SendMessageW(g_transcript, EM_SETSEL, g_liveAnswer.tailStart, -1);
    SendMessageW(g_transcript, EM_REPLACESEL, FALSE, (LPARAM)L"");
    g_toolLinks.resize(g_liveAnswer.tailToolLinks);
    g_markdownLinks.resize(g_liveAnswer.tailMarkdownLinks);
    g_codeBlocks.resize(g_liveAnswer.tailCodeBlocks);

    if (!hasTools)
    {
        const size_t cut = ChatMarkdownStreamCut(text, g_liveAnswer.frozenUntil);
        if (cut > g_liveAnswer.frozenUntil)
        {
            RichAppendMarkdown(g_transcript, text.substr(g_liveAnswer.frozenUntil, cut - g_liveAnswer.frozenUntil), C_TEXT,
                               g_liveAnswer.frozenUntil ? 180 : 0);
            g_liveAnswer.frozenUntil = cut;
        }
        g_liveAnswer.tailStart = GetWindowTextLengthW(g_transcript);
        g_liveAnswer.tailToolLinks = g_toolLinks.size();
        g_liveAnswer.tailMarkdownLinks = g_markdownLinks.size();
        g_liveAnswer.tailCodeBlocks = g_codeBlocks.size();
        AppendLiveAnswerText(chat, text, g_liveAnswer.frozenUntil, text.size(), g_liveAnswer.frozenUntil ? 180 : 0);
    }
    else
    {
        g_liveAnswer.frozenUntil = 0;
        g_liveAnswer.tailStart = g_liveAnswer.start;
        AppendLiveAnswerText(chat, text, 0, text.size());
    }

    const LONG length = GetWindowTextLengthW(g_transcript);
    if (follow)
    {
        SendMessageW(g_transcript, EM_SETSEL, -1, -1);
        SendMessageW(g_transcript, EM_SCROLLCARET, 0, 0);
    }
    else
    {
        selection.cpMin = std::min<LONG>(selection.cpMin, length);
        selection.cpMax = std::min<LONG>(selection.cpMax, length);
        SendMessageW(g_transcript, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessageW(g_transcript, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    }
    UpdateJumpLatest();
    InvalidateRect(g_transcript, nullptr, FALSE);
}
void AppendLiveAnswer(const Chat &chat)
{
    if (g_busy != J_CHAT || chat.id != g_jobChatId) return;
    if (g_answer.text.empty() && g_answer.reasoning.empty() && g_answer.tools.empty() && !g_answer.toolLimit) return;
    if (!g_answer.reasoning.empty()) AppendThoughtSummary(chat, chat.messages.size(), g_answer);
    if (g_answer.text.empty() && g_answer.tools.empty() && !g_answer.toolLimit) return;
    g_liveAnswer.active = true;
    g_liveAnswer.start = g_liveAnswer.tailStart = GetWindowTextLengthW(g_transcript);
    g_liveAnswer.frozenUntil = 0;
    g_liveAnswer.baseToolLinks = g_liveAnswer.tailToolLinks = g_toolLinks.size();
    g_liveAnswer.baseMarkdownLinks = g_liveAnswer.tailMarkdownLinks = g_markdownLinks.size();
    g_liveAnswer.baseCodeBlocks = g_liveAnswer.tailCodeBlocks = g_codeBlocks.size();
    RefreshLiveAnswer(chat);
}
void UpdateChatActionButtons()
{
    bool hasUser = false, hasAnswer = false;
    if (g_chat >= 0 && g_chat < (int)g_chats.size())
    {
        const auto &messages = g_chats[g_chat].messages;
        hasUser = std::any_of(messages.begin(), messages.end(), [](const Message &message) { return message.role == "user"; });
        hasAnswer = !messages.empty() && messages.back().role == "assistant";
    }
    const auto visible = NativeChatToolbarActionsFor(g_page == CHAT, hasUser, hasAnswer, g_editMessage >= 0);
    const auto update = [](HWND control, bool show, bool enabled) {
        if (!control) return;
        ShowWindow(control, show ? SW_SHOW : SW_HIDE);
        EnableWindow(control, enabled);
    };
    const bool idle = NativeChatToolbarActionsEnabled(g_busy != J_NONE);
    if (g_send) EnableWindow(g_send, idle);
    update(g_editLast, visible.editLast, idle);
    update(g_regenerate, visible.regenerate, idle);
    update(g_cancelEdit, visible.cancelEdit, idle);
    update(g_answerOptions, visible.answerOptions, idle);
}
void UpdateContextMeterVisibility()
{
    const bool hasChat = g_chat >= 0 && g_chat < (int)g_chats.size();
    bool hasMessages = false;
    if (hasChat)
    {
        const auto &messages = g_chats[g_chat].messages;
        const size_t visibleMessages = g_editMessage >= 0 && g_editMessage < (int)messages.size()
            ? (size_t)g_editMessage : messages.size();
        hasMessages = visibleMessages != 0;
    }
    if (!hasMessages && g_input)
    {
        const int length = GetWindowTextLengthW(g_input);
        if (length > 0)
        {
            std::wstring draft((size_t)length + 1, L'\0');
            GetWindowTextW(g_input, draft.data(), length + 1);
            draft.resize((size_t)length);
            hasMessages = !TrimChatInput(draft).empty();
        }
    }
    hasMessages = hasMessages || !g_attachments.empty();
    if (hasChat && (!hasMessages || g_contextMeterChatId != g_chats[g_chat].id))
    {
        g_contextMeterChatId.clear();
        g_contextTokensKnown = false;
    }
    const bool hasMaximum = MonitorInteger(g_health, "max_context") > 0;
    const bool hasCount = hasChat && NativeContextMeterCountMatches(g_chats[g_chat].id, g_contextMeterChatId);
    if (g_contextMeter)
        ShowWindow(g_contextMeter, NativeContextMeterVisible(g_page == CHAT, hasMessages, hasMaximum, hasCount) ? SW_SHOW : SW_HIDE);
}
void RenderChat()
{
    if (!g_transcript)
        return;
    SetWindowTextW(g_transcript, L"");
    g_thoughtLinks.clear();
    g_toolLinks.clear();
    g_markdownLinks.clear();
    g_attachmentLinks.clear();
    g_inlineImagePreviews.clear();
    g_codeBlocks.clear();
    g_userMessageRanges.clear();
    g_assistantMessageRanges.clear();
    g_liveAnswer = {};
    g_liveThought = {};
    if (g_chat < 0 || g_chat >= (int)g_chats.size())
    {
        maya_video::CloseInline();
        g_inlineVideoPath.clear();
        UpdateChatActionButtons();
        UpdateContextMeterVisibility();
        return;
    }
    const auto &chat = g_chats[g_chat];
    for (size_t i = 0; i < chat.messages.size(); ++i)
    {
        const auto &m = chat.messages[i];
        const auto userStart = GetWindowTextLengthW(g_transcript);
        RichAppend(g_transcript, m.role == "user" ? L"You\r\n" : L"Maya\r\n", m.role == "user" ? C_CYAN_TEXT : C_PURPLE_TEXT,
                   true);
        if (!m.reasoning.empty()) AppendThoughtSummary(chat, i, m);
        if (!m.error.empty())
            RichAppend(g_transcript, W(m.error) + L"\r\n", C_RED_TEXT);
        else if (!m.text.empty())
            RichAppendMarkdown(g_transcript, W(m.text) + L"\r\n", m.role == "user" ? C_SOFT : C_TEXT);
        if (m.error.empty() && m.tools.is_array())
            for (size_t toolIndex = 0; toolIndex < m.tools.size(); ++toolIndex)
            {
                const auto &tool = m.tools[toolIndex];
                if (tool.is_object())
                    AppendToolSummary(chat, i, toolIndex, tool);
            }
        if (m.error.empty() && m.toolLimit > 0)
            RichAppend(g_transcript, L"[Tool calls stopped at the limit of " + std::to_wstring(m.toolLimit) +
                                          L" rounds]\r\n", C_MUTED);
        if (m.stopped)
            RichAppend(g_transcript, L"Stopped\r\n", C_MUTED);
        for (const auto &file : m.files)
        {
            const auto kind = MayaAttachmentType(file);
            const bool image = kind == MayaAttachmentKind::IMAGE;
            const bool video = kind == MayaAttachmentKind::VIDEO;
            RichAppend(g_transcript, image ? L"Image: " : video ? L"Video: " : L"File: ", C_MUTED);
            CHARRANGE range{};
            range.cpMin = GetWindowTextLengthW(g_transcript);
            const auto label = (image ? L"Preview " : video ? L"Play " : L"") + file.filename().wstring();
            RichAppend(g_transcript, label + L"\r\n", image || video ? C_CYAN_TEXT : C_MUTED, false, image || video);
            if (image)
            {
                range.cpMax = GetWindowTextLengthW(g_transcript);
                g_attachmentLinks.push_back({range, file});
                g_inlineImagePreviews.push_back({range.cpMin, file, {}});
                RichAppend(g_transcript, L"\r\n\r\n\r\n\r\n\r\n", C_TEXT);
            }
            else if (video)
            {
                range.cpMax = GetWindowTextLengthW(g_transcript);
                g_attachmentLinks.push_back({range, file});
                if (g_inlineVideoPath == file)
                    RichAppend(g_transcript, L"\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n", C_TEXT);
            }
        }
        auto &ranges = m.role == "user" ? g_userMessageRanges : g_assistantMessageRanges;
        ranges.push_back({userStart, GetWindowTextLengthW(g_transcript), (int)i});
    }
    if (!g_inlineVideoPath.empty())
    {
        bool found = false;
        for (const auto &[range, path] : g_attachmentLinks)
            if (path == g_inlineVideoPath)
            {
                found = true;
                maya_video::UpdateInlineAttachment(g_transcript, path, range.cpMin);
                break;
            }
        if (!found)
        {
            maya_video::CloseInline();
            g_inlineVideoPath.clear();
        }
    }
    AppendLiveAnswer(chat);
    UpdateChatActionButtons();
    UpdateContextMeterVisibility();
    UpdateJumpLatest();
    UpdateGreeting();
    InvalidateRect(g_transcript, nullptr, FALSE);
}
void RefreshChatList()
{
    if (!g_chatList)
        return;
    int n = GetWindowTextLengthW(g_chatSearch);
    std::wstring filter(n + 1, L'\0');
    if (n)
        GetWindowTextW(g_chatSearch, filter.data(), n + 1);
    filter.resize(n);
    filter = NativeChatSearchQuery(filter);
    SendMessageW(g_chatList, LB_RESETCONTENT, 0, 0);
    g_visibleChats.clear();
    std::vector<int64_t> updated;
    updated.reserve(g_chats.size());
    for (const auto &chat : g_chats)
    {
        std::error_code ec;
        const auto time = fs::last_write_time(fs::path(g_chatDir) / (chat.id + L".jsonl"), ec);
        updated.push_back(ec ? std::numeric_limits<int64_t>::min() : (int64_t)time.time_since_epoch().count());
    }
    const auto recentOrder = NativeChatRecentOrder(updated);
    std::vector<bool> searchVisible(g_chats.size(), filter.empty());
    if (!filter.empty())
    {
        std::vector<bool> searchMatches(g_chats.size(), false);
        for (size_t i = 0; i < g_chats.size(); ++i)
        {
            const auto &chat = g_chats[i];
            searchMatches[i] = NativeChatSearchFind(ChatDisplayTitle(chat), filter) != std::wstring::npos;
            for (const auto &message : chat.messages)
                if (!searchMatches[i] && NativeChatSearchFind(W(message.text), filter) != std::wstring::npos)
                    searchMatches[i] = true;
        }
        for (int index : NativeChatSearchLimitedOrder(recentOrder, searchMatches))
            searchVisible[index] = true;
    }
    auto matches = [&](int index) { return index >= 0 && (size_t)index < searchVisible.size() && searchVisible[index]; };
    auto addRow = [&](const std::wstring &label, int chat, int folder, const std::wstring &snippet = std::wstring())
    {
        const int row = (int)g_visibleChats.size();
        g_visibleChats.push_back({chat, folder, snippet});
        SendMessageW(g_chatList, LB_ADDSTRING, 0, (LPARAM)label.c_str());
        if (chat == g_chat)
            SendMessageW(g_chatList, LB_SETCURSEL, row, 0);
    };
    auto chatSnippet = [&](int index)
    {
        const auto &chat = g_chats[index];
        const auto title = ChatDisplayTitle(chat);
        if (!filter.empty())
            for (const auto &message : chat.messages)
            {
                auto snippet = NativeChatSearchPreview(W(message.text), filter, title);
                if (!snippet.empty()) return snippet;
            }
        SYSTEMTIME now{};
        GetLocalTime(&now);
        const auto relative = NativeChatRelativeTime(fs::path(g_chatDir) / (chat.id + L".jsonl"), now);
        for (const auto &message : chat.messages)
            if (message.role == "assistant" && !message.text.empty())
                return relative.empty() ? NativeChatAssistantPreview(W(message.text))
                                        : relative + L" · " + NativeChatAssistantPreview(W(message.text));
        return relative;
    };
    if (!filter.empty())
    {
        for (int index : recentOrder)
            if (matches(index))
            {
                const auto &chat = g_chats[index];
                addRow((chat.pinned ? L"★ " : L"") + ChatDisplayTitle(chat), index, -1, chatSnippet(index));
            }
        if (g_visibleChats.empty()) addRow(L"No chat matches.", -1, -1);
        return;
    }
    if (g_chats.empty())
    {
        addRow(L"Your chats appear here. They stay on this PC.", -1, -1);
        return;
    }
    auto addChats = [&](const std::vector<int> &indices, const std::wstring &heading, int folder)
    {
        if (indices.empty())
            return;
        std::vector<int> visible;
        for (int index : indices)
            if (matches(index))
                visible.push_back(index);
        if (visible.empty() && !filter.empty())
            return;
        if (!heading.empty())
            addRow(heading + L" (" + std::to_wstring(visible.size()) + L")", -1, folder);
        for (int index : visible)
        {
            const auto title = ChatDisplayTitle(g_chats[index]);
            addRow((g_chats[index].pinned ? L"★ " : L"") + title, index, folder, chatSnippet(index));
        }
    };
    std::vector<int> pinned;
    for (int index : recentOrder)
        if (g_chats[index].pinned)
            pinned.push_back(index);
    addChats(pinned, L"Pinned", -1);

    for (size_t fi = 0; fi < g_folders.size(); ++fi)
    {
        auto &folder = g_folders[fi];
        std::vector<int> members;
        for (int index : recentOrder)
            if (g_chats[index].folder == folder.id)
                members.push_back(index);
        std::vector<int> matched;
        for (int index : members)
            if (matches(index))
                matched.push_back(index);
        if (!filter.empty() && matched.empty())
            continue;
        addRow(std::wstring(folder.collapsed ? L"▸ " : L"▾ ") + folder.name + L" (" +
                   std::to_wstring(matched.size()) + L")",
               -1, (int)fi);
        if (!folder.collapsed)
            for (int index : matched)
            {
                const auto title = ChatDisplayTitle(g_chats[index]);
                addRow((g_chats[index].pinned ? L"★ " : L"  ") + title, index, (int)fi, chatSnippet(index));
            }
    }
    std::vector<int> unfiled;
    for (int index : recentOrder)
    {
        const auto it = std::find_if(g_folders.begin(), g_folders.end(), [&](const Folder &f) {
            return f.id == g_chats[index].folder;
        });
        if (!g_chats[index].pinned && it == g_folders.end())
            unfiled.push_back(index);
    }
    std::wstring day;
    for (int index : unfiled)
    {
        if (!matches(index)) continue;
        const auto group = NativeChatDayGroup(fs::path(g_chatDir) / (g_chats[index].id + L".jsonl"));
        if (group != day)
        {
            day = group;
            addRow(day, -1, -1);
        }
        const auto title = ChatDisplayTitle(g_chats[index]);
        addRow(title, index, -1, chatSnippet(index));
    }
}
void NewChat()
{
    const bool hasCurrentChat = g_chat >= 0 && g_chat < (int)g_chats.size();
    if (NativeCanReuseBlankChat(hasCurrentChat,
                                hasCurrentChat && g_chats[g_chat].messages.empty(),
                                hasCurrentChat && g_chats[g_chat].system == g_defaultSystem))
    {
        ShowPage(CHAT);
        SetFocus(g_input);
        return;
    }
    Chat c;
    c.id = NewId();
    c.title = L"New chat";
    c.system = g_defaultSystem;
    c.thinkingFor = g_health.value("model", std::string());
    const auto defaults = ResolveChatSampling(g_samplingPrefs, c.thinkingFor,
                                              g_health.value("defaults", Json::object()));
    c.thinking = defaults.value("reasoning_effort", std::string("high"));
    auto path = fs::path(g_chatDir) / (c.id + L".jsonl");
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                           nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    CloseHandle(f);
    SaveChatMetadata(c);
    g_chats.insert(g_chats.begin(), std::move(c));
    g_chat = 0;
    SaveCurrentChatSelection();
    SetWindowTextW(g_chatSearch, L"");
    RefreshChatList();
    ApplyChatThinking();
    RenderChat();
    ScheduleContextMeter();
    SetFocus(g_input);
}
struct PromptState
{
    HWND edit{}, list{};
    std::wstring label, value;
    std::vector<std::pair<std::wstring, std::wstring>> choices;
    bool multiline = false, done = false, accepted = false;
};
LRESULT CALLBACK PromptProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    auto *state = (PromptState *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE)
    {
        state = (PromptState *)((CREATESTRUCTW *)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)state);
    }
    if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX || m == WM_CTLCOLORBTN)
    {
        HDC dc = (HDC)w;
        const HWND control = (HWND)l;
        bool mutedLabel = control == g_themeLabel || control == g_schemeLabel || control == g_contextLabel ||
            control == g_portLabel || control == g_budgetLabel || control == g_budgetHint ||
            control == g_defaultInstructionsLabel || control == g_connectKeyLabel ||
            control == g_connectExampleLabel || control == g_connectInfo || control == g_connectAuth ||
            control == g_mayaProjectCredit;
        for (auto label : g_samplingLabels) mutedLabel = mutedLabel || control == label;
        for (auto label : g_connectTitle) mutedLabel = mutedLabel || control == label;
        for (auto label : g_priceLabels) mutedLabel = mutedLabel || control == label;
        SetTextColor(dc, mutedLabel ? C_MUTED : C_TEXT);
        SetBkColor(dc, C_CARD);
        return (LRESULT)g_cardBrush;
    }
    if (m == WM_CREATE)
    {
        const bool selecting = !state->choices.empty();
        CreateWindowW(L"STATIC", state->label.c_str(), WS_CHILD | WS_VISIBLE, 18, 16, 424, selecting ? 38 : 24,
                      h, nullptr, g_inst, nullptr);
        if (selecting)
        {
            state->list = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                18, 58, 424, 190, h, (HMENU)1, g_inst, nullptr);
            for (const auto &choice : state->choices)
                SendMessageW(state->list, LB_ADDSTRING, 0, (LPARAM)choice.first.c_str());
            SendMessageW(state->list, LB_SETCURSEL, 0, 0);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 260, 262, 82, 30,
                          h, (HMENU)2, g_inst, nullptr);
            CreateWindowW(L"BUTTON", L"Switch and delete", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          342, 262, 100, 30, h, (HMENU)3, g_inst, nullptr);
            SendMessageW(state->list, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetFocus(state->list);
        }
        else
        {
            const DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER |
                                (state->multiline ? ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL : ES_AUTOHSCROLL);
            state->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", state->value.c_str(), style, 18, 44, 424,
                                          state->multiline ? 112 : 30, h, (HMENU)1, g_inst, nullptr);
            CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 260, 174, 82, 30,
                          h, (HMENU)2, g_inst, nullptr);
            CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                          352, 174, 90, 30, h, (HMENU)3, g_inst, nullptr);
            SendMessageW(state->edit, WM_SETFONT, (WPARAM)g_font, TRUE);
            SetFocus(state->edit);
            SendMessageW(state->edit, EM_SETSEL, 0, -1);
        }
        return 0;
    }
    if (m == WM_COMMAND && LOWORD(w) == 1 && HIWORD(w) == LBN_SELCHANGE)
        return 0;
    if (m == WM_COMMAND && (LOWORD(w) == 2 || LOWORD(w) == 3))
    {
        if (LOWORD(w) == 3)
        {
            if (state->list)
            {
                const int selected = (int)SendMessageW(state->list, LB_GETCURSEL, 0, 0);
                if (selected >= 0 && selected < (int)state->choices.size())
                {
                    state->value = state->choices[selected].second;
                    state->accepted = true;
                }
            }
            else
            {
                const int n = GetWindowTextLengthW(state->edit);
                state->value.assign(n + 1, L'\0');
                GetWindowTextW(state->edit, state->value.data(), n + 1);
                state->value.resize(n);
                state->accepted = true;
            }
        }
        state->done = true;
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_CLOSE)
    {
        state->done = true;
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
std::optional<std::wstring> PromptText(HWND owner, const std::wstring &title, const std::wstring &label,
                                       const std::wstring &value, bool multiline,
                                       std::vector<std::pair<std::wstring, std::wstring>> choices = {})
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PromptProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = g_bgBrush;
        wc.lpszClassName = L"MayaNativePrompt";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return std::nullopt;
        registered = true;
    }
    PromptState state{};
    state.label = label;
    state.value = value;
    state.multiline = multiline;
    state.choices = std::move(choices);
    const bool selecting = !state.choices.empty();
    const int width = 478, height = selecting ? 330 : 248;
    RECT r{};
    GetWindowRect(owner, &r);
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"MayaNativePrompt", title.c_str(),
                                  WS_POPUP | WS_CAPTION | WS_SYSMENU, (r.left + r.right - width) / 2,
                                  (r.top + r.bottom - height) / 2, width, height, owner, nullptr, g_inst, &state);
    if (!dialog)
        return std::nullopt;
    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(dialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return state.accepted ? std::optional<std::wstring>(state.value) : std::nullopt;
}

struct ModelDeleteReviewUi
{
    HWND details{}, replacement{}, files{}, cancel{};
    std::wstring title, intro, description, filesLabel;
    std::vector<std::pair<std::wstring, std::wstring>> choices;
    std::vector<std::string> targets;
    ModelDeleteReviewSelection result;
    bool running = false, canDeleteFiles = false, done = false;
};

LRESULT CALLBACK ModelDeleteReviewProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    auto *s = (ModelDeleteReviewUi *)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (m == WM_NCCREATE)
    {
        s = (ModelDeleteReviewUi *)((CREATESTRUCTW *)l)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)s);
    }
    if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX)
    {
        SetTextColor((HDC)w, C_TEXT);
        SetBkColor((HDC)w, C_CARD);
        return (LRESULT)g_cardBrush;
    }
    if (m == WM_CREATE)
    {
        CreateWindowW(L"STATIC", s->intro.c_str(), WS_CHILD | WS_VISIBLE, 18, 16, 552, 34,
                      h, nullptr, g_inst, nullptr);
        s->details = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s->description.c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
            18, 54, 552, 270, h, (HMENU)1, g_inst, nullptr);
        SendMessageW(s->details, WM_SETFONT, (WPARAM)g_font, TRUE);
        if (s->running)
        {
            CreateWindowW(L"STATIC", L"Switch the running model to:", WS_CHILD | WS_VISIBLE,
                          18, 334, 552, 22, h, nullptr, g_inst, nullptr);
            s->replacement = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                WS_VSCROLL | CBS_DROPDOWNLIST, 18, 359, 552, 180, h, (HMENU)4, g_inst, nullptr);
            for (const auto &choice : s->choices)
                SendMessageW(s->replacement, CB_ADDSTRING, 0, (LPARAM)choice.first.c_str());
            if (!s->choices.empty()) SendMessageW(s->replacement, CB_SETCURSEL, 0, 0);
            SendMessageW(s->replacement, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        if (s->canDeleteFiles)
        {
            s->files = CreateWindowW(L"BUTTON", s->filesLabel.c_str(), WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                BS_AUTOCHECKBOX, 18, s->running ? 405 : 350, 552, 32, h, (HMENU)5, g_inst, nullptr);
            ApplyCheckboxTheme(s->files);
            SendMessageW(s->files, WM_SETFONT, (WPARAM)g_font, TRUE);
        }
        s->cancel = CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            376, 496, 92, 32, h, (HMENU)2, g_inst, nullptr);
        CreateWindowW(L"BUTTON", s->running ? L"Switch, then delete" : L"Delete model",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP, 474, 496, 96, 32, h, (HMENU)3, g_inst, nullptr);
        SendMessageW(s->cancel, WM_SETFONT, (WPARAM)g_font, TRUE);
        SetFocus(s->cancel);
        return 0;
    }
    if (m == WM_COMMAND && (LOWORD(w) == 2 || LOWORD(w) == 3))
    {
        if (LOWORD(w) == 3)
        {
            const int selected = s->running
                ? (int)SendMessageW(s->replacement, CB_GETCURSEL, 0, 0) : 0;
            const auto index = selected < 0 ? s->targets.size() : (size_t)selected;
            const bool deleteFiles = s->files && SendMessageW(s->files, BM_GETCHECK, 0, 0) == BST_CHECKED;
            s->result = ResolveModelDeleteReview(true, s->running, s->canDeleteFiles,
                                                  deleteFiles, s->targets, index);
            if (!s->result.accepted) return 0;
        }
        s->done = true;
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_CLOSE)
    {
        s->done = true;
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

ModelDeleteReviewSelection ReviewModelDelete(HWND owner, std::wstring title, std::wstring intro,
    std::wstring description, std::wstring filesLabel,
    std::vector<std::pair<std::wstring, std::wstring>> choices, std::vector<std::string> targets,
    bool running, bool canDeleteFiles)
{
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = ModelDeleteReviewProc;
        wc.hInstance = g_inst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = g_bgBrush;
        wc.lpszClassName = L"MayaNativeModelDeleteReview";
        if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return {};
        registered = true;
    }
    ModelDeleteReviewUi state{};
    state.title = std::move(title);
    state.intro = std::move(intro);
    state.description = std::move(description);
    state.filesLabel = std::move(filesLabel);
    state.choices = std::move(choices);
    state.targets = std::move(targets);
    state.running = running;
    state.canDeleteFiles = canDeleteFiles;
    RECT r{};
    GetWindowRect(owner, &r);
    constexpr int width = 610, height = 570;
    HWND dialog = CreateWindowExW(WS_EX_DLGMODALFRAME, L"MayaNativeModelDeleteReview", state.title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, (r.left + r.right - width) / 2,
        (r.top + r.bottom - height) / 2, width, height, owner, nullptr, g_inst, &state);
    if (!dialog) return {};
    EnableWindow(owner, FALSE);
    ShowWindow(dialog, SW_SHOW);
    MSG msg{};
    while (IsWindow(dialog) && GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(dialog, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return state.result;
}

bool RecyclePathToBin(const fs::path &path)
{
    std::error_code ec;
    if (!fs::exists(path, ec)) return !ec;
    if (ec) return false;
    auto from = path.wstring();
    from.push_back(L'\0');
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
    return SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;
}
bool RecycleChatFiles(const std::wstring &id, std::wstring &recoveryPath)
{
    const std::vector<fs::path> sources = {fs::path(g_chatDir) / (id + L".jsonl"),
                                           fs::path(g_chatDir) / (id + L".meta.json"),
                                           fs::path(g_chatDir) / L"attachments" / id};
    const auto stage = fs::path(g_chatDir) / (L".delete-" + NewId());
    std::error_code ec;
    fs::create_directory(stage, ec);
    if (ec) return false;
    std::vector<std::pair<fs::path, fs::path>> moved;
    for (const auto &source : sources)
    {
        const bool exists = fs::exists(source, ec);
        if (ec) break;
        if (!exists) continue;
        const auto destination = stage / source.filename();
        fs::rename(source, destination, ec);
        if (ec) break;
        moved.emplace_back(source, destination);
    }
    if (!ec && RecyclePathToBin(stage)) return true;
    bool restored = true;
    for (auto it = moved.rbegin(); it != moved.rend(); ++it)
    {
        ec.clear();
        fs::rename(it->second, it->first, ec);
        restored = restored && !ec;
    }
    if (restored)
    {
        ec.clear();
        fs::remove(stage, ec);
    }
    else recoveryPath = stage.wstring();
    return false;
}
void RestorePendingChatDelete(PendingChatDelete pending)
{
    const auto currentId = g_chat >= 0 && g_chat < (int)g_chats.size() ? g_chats[g_chat].id : std::wstring();
    g_chats.push_back(std::move(pending.chat));
    const auto current = std::find_if(g_chats.begin(), g_chats.end(), [&](const Chat &chat) { return chat.id == currentId; });
    if (current != g_chats.end()) g_chat = (int)(current - g_chats.begin());
}
bool FinishPendingChatDelete()
{
    KillTimer(g_hwnd, 8);
    bool allFinished = true;
    std::wstring failure;
    while (!g_pendingChatDeletes.empty())
    {
        auto pending = std::move(g_pendingChatDeletes.back());
        g_pendingChatDeletes.pop_back();
        std::wstring recoveryPath;
        if (!RecycleChatFiles(pending.chat.id, recoveryPath))
        {
            allFinished = false;
            failure = recoveryPath.empty()
                ? L"Delete could not finish; the chat was restored in the list."
                : L"Delete could not finish; inspect staged chat files at " + recoveryPath + L". The chat was restored in the list.";
            RestorePendingChatDelete(std::move(pending));
        }
    }
    if (!allFinished)
    {
        RefreshChatList();
        RenderChat();
        SetText(g_status, failure);
    }
    Layout();
    return allFinished;
}
void FinishExpiredPendingChatDeletes()
{
    const auto now = GetTickCount64();
    for (size_t i = 0; i < g_pendingChatDeletes.size();)
    {
        if (g_pendingChatDeletes[i].expiresAt > now) { ++i; continue; }
        auto pending = std::move(g_pendingChatDeletes[i]);
        g_pendingChatDeletes.erase(g_pendingChatDeletes.begin() + i);
        std::wstring recoveryPath;
        if (!RecycleChatFiles(pending.chat.id, recoveryPath))
        {
            RestorePendingChatDelete(std::move(pending));
            RefreshChatList();
            RenderChat();
            SetText(g_status, recoveryPath.empty()
                ? L"Delete could not finish; the chat was restored in the list."
                : L"Delete could not finish; inspect staged chat files at " + recoveryPath + L". The chat was restored in the list.");
        }
    }
    if (!NativeChatUndoAvailable(g_pendingChatDeletes.size())) KillTimer(g_hwnd, 8);
    Layout();
}
void UndoPendingChatDelete()
{
    FinishExpiredPendingChatDeletes();
    if (!NativeChatUndoAvailable(g_pendingChatDeletes.size())) return;
    auto pending = std::move(g_pendingChatDeletes.back());
    g_pendingChatDeletes.pop_back();
    RestorePendingChatDelete(std::move(pending));
    if (!NativeChatUndoAvailable(g_pendingChatDeletes.size())) KillTimer(g_hwnd, 8);
    else SetTimer(g_hwnd, 8, 250, nullptr);
    RefreshChatList();
    RenderChat();
    SetText(g_status, g_pendingChatDeletes.empty() ? L"Chat restored." : L"Chat restored. Undo again for earlier deletes.");
    Layout();
}
void ShowFolderMenu(HWND owner, int contextIndex = -1, POINT contextPoint = {});
void ShowChatMenu(HWND owner, LPARAM screenPoint, int activeChat = -1)
{
    POINT p{GET_X_LPARAM(screenPoint), GET_Y_LPARAM(screenPoint)};
    const bool keyboard = p.x == -1 && p.y == -1;
    int targetIndex = activeChat;
    if (targetIndex < 0)
    {
        int selected = -1, pointed = -1;
        if (keyboard)
        {
            selected = (int)SendMessageW(g_chatList, LB_GETCURSEL, 0, 0);
            RECT item{};
            if (selected < 0 || SendMessageW(g_chatList, LB_GETITEMRECT, selected, (LPARAM)&item) == LB_ERR)
                return;
            p = {item.left + 12, item.top + (item.bottom - item.top) / 2};
            ClientToScreen(g_chatList, &p);
        }
        else
        {
            POINT client = p;
            ScreenToClient(g_chatList, &client);
            const LRESULT item = SendMessageW(g_chatList, LB_ITEMFROMPOINT, 0, MAKELPARAM(client.x, client.y));
            if (HIWORD(item)) return;
            pointed = LOWORD(item);
        }
        const int visible = NativeChatMenuVisibleRow(keyboard, selected, pointed, g_visibleChats.size());
        if (visible < 0) return;
        const auto row = g_visibleChats[visible];
        if (row.chat < 0)
        {
            if (row.folder >= 0 && row.folder < (int)g_folders.size()) ShowFolderMenu(owner, row.folder, p);
            return;
        }
        targetIndex = row.chat;
    }
    if (targetIndex >= (int)g_chats.size()) return;
    const auto targetChatId = g_chats[targetIndex].id;
    auto &chat = g_chats[targetIndex];
    HMENU menu = CreatePopupMenu();
    if (NativeChatMenuFor(chat.messages.size()) == NativeChatMenuMode::FULL)
    {
        AppendMenuW(menu, MF_STRING, 2, chat.pinned ? L"Unpin" : L"Pin");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 8, L"Move to folder");
        for (size_t i = 0; i < g_folders.size() && i < 900; ++i)
            AppendMenuW(menu, MF_STRING | (chat.folder == g_folders[i].id ? MF_CHECKED : 0),
                        (UINT_PTR)(1000 + i), g_folders[i].name.c_str());
        AppendMenuW(menu, MF_STRING, 5, L"New folder");
        if (!chat.folder.empty())
            AppendMenuW(menu, MF_STRING, 6, L"Take out of the folder");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, 1, L"Rename");
        AppendMenuW(menu, MF_STRING, 3, L"Instructions");
        AppendMenuW(menu, MF_STRING, 7, L"Export as Markdown");
        AppendMenuW(menu, MF_STRING, 4, L"Delete");
    }
    else
        AppendMenuW(menu, MF_STRING, 3, L"Instructions for this chat");
    const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, owner, nullptr);
    DestroyMenu(menu);
    if (selected == 1)
    {
        auto value = PromptText(owner, L"Rename chat", L"Chat name", ChatDisplayTitle(chat), false);
        if (!value) return;
        auto name = TrimAndLimitName(*value, 80);
        if (name.empty()) return;
        chat.title = std::move(name);
        chat.titleManual = true;
    }
    else if (selected == 2)
        chat.pinned = !chat.pinned;
    else if (selected == 3)
    {
        auto value = PromptText(owner, L"Chat instructions", L"The model reads these before the conversation",
                                W(chat.system), true);
        if (!value)
            return;
        chat.system = U(*value);
        if (g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].id == targetChatId)
            ScheduleContextMeter();
    }
    else if (selected == 7)
    {
        if (chat.messages.empty())
        {
            SetText(g_status, L"This chat has no messages to export.");
            return;
        }
        wchar_t path[32768]{};
        WIN32_FILE_ATTRIBUTE_DATA history{};
        FILETIME lastWrite{};
        bool haveLastWrite = false;
        SYSTEMTIME updated{};
        const auto historyPath = fs::path(g_chatDir) / (chat.id + L".jsonl");
        if (GetFileAttributesExW(historyPath.c_str(), GetFileExInfoStandard, &history))
        {
            lastWrite = history.ftLastWriteTime;
            haveLastWrite = true;
        }
        if (!haveLastWrite || !FileTimeToSystemTime(&lastWrite, &updated))
            GetSystemTime(&updated);
        const auto filename = ChatExportFilename(updated);
        wcscpy_s(path, filename.c_str());
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = owner;
        ofn.lpstrFile = path;
        ofn.nMaxFile = (DWORD)std::size(path);
        ofn.lpstrFilter = L"Markdown files\0*.md\0All files\0*.*\0\0";
        ofn.lpstrDefExt = L"md";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn))
            return;
        std::vector<ChatExportMessage> messages;
        for (const auto &message : chat.messages)
        {
            ChatExportMessage out{message.role, message.text, message.reasoning, {}};
            out.error = message.error;
            if (message.tools.is_array())
                for (const auto &tool : message.tools)
                    if (tool.is_object())
                        out.tools.push_back(tool);
            messages.push_back(std::move(out));
        }
        const auto markdown = ChatMarkdown(U(ChatDisplayTitle(chat)), chat.system,
                                          g_health.value("model", std::string("Maya")), messages);
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            SetText(g_status, GetLastError() == ERROR_FILE_EXISTS ? L"That file already exists; choose a new name."
                                                                  : L"Could not create the Markdown export.");
            return;
        }
        DWORD written = 0;
        const bool ok = WriteFile(file, markdown.data(), (DWORD)markdown.size(), &written, nullptr) &&
                        written == markdown.size();
        CloseHandle(file);
        SetText(g_status, ok ? L"Chat exported as Markdown." : L"The Markdown export could not be fully written.");
        return;
    }
    else if (selected == 5)
    {
        auto value = PromptText(owner, L"New folder", L"Folder name", L"", false);
        if (!value)
            return;
        auto name = TrimAndLimitName(*value, 60);
        if (name.empty()) return;
        Folder folder{NewId(), std::move(name)};
        chat.folder = NativeChatFolderDestination(false, folder.id);
        g_folders.push_back(folder);
        SaveFolders();
    }
    else if (selected == 6)
        chat.folder.clear();
    else if (selected >= 1000 && selected < 1900 && selected - 1000 < g_folders.size())
    {
        auto &folder = g_folders[selected - 1000];
        const bool selectedCurrent = chat.folder == folder.id;
        chat.folder = NativeChatFolderDestination(selectedCurrent, folder.id);
        const bool wasCollapsed = folder.collapsed;
        folder.collapsed = NativeFolderCollapsedAfterChatMove(selectedCurrent, wasCollapsed);
        if (wasCollapsed && !folder.collapsed) SaveFolders();
    }
    else if (selected == 4)
    {
        if (g_busy == J_CHAT && g_jobChatId == chat.id)
        {
            MessageBoxW(owner, L"Stop the current answer before deleting this chat.", L"Chat is active",
                        MB_OK | MB_ICONINFORMATION);
            return;
        }
        const auto target = std::find_if(g_chats.begin(), g_chats.end(), [&](const Chat &item) { return item.id == targetChatId; });
        if (target == g_chats.end()) return;
        const int removed = (int)(target - g_chats.begin());
        auto &targetChat = *target;
        if (g_busy == J_CHAT && g_jobChatId == targetChat.id)
        {
            MessageBoxW(owner, L"Stop the current answer before deleting this chat.", L"Chat is active",
                        MB_OK | MB_ICONINFORMATION);
            return;
        }
        const bool deletingCurrent = removed == g_chat;
        if (deletingCurrent) g_editMessage = -1;
        g_pendingChatDeletes.push_back({std::move(g_chats[removed]), GetTickCount64() + 6000});
        g_chats.erase(g_chats.begin() + removed);
        SetTimer(g_hwnd, 8, 250, nullptr);
        g_chat = NativeChatSelectionAfterDelete(g_chat, removed, g_chats.size());
        if (g_chat < 0)
        {
            NewChat();
        }
        SaveCurrentChatSelection();
        RefreshChatList();
        RenderChat();
        SetText(g_status, L"Chat deleted. Undo is available for 6 seconds.");
        Layout();
        return;
    }
    else
        return;
    SaveChatMetadata(chat);
    RefreshChatList();
}
void ShowFolderMenu(HWND owner, int contextIndex, POINT contextPoint)
{
    HMENU menu = CreatePopupMenu();
    if (contextIndex >= 0 && contextIndex < (int)g_folders.size())
    {
        AppendMenuW(menu, MF_STRING, (UINT_PTR)(3001 + contextIndex * 3), L"Rename…");
        AppendMenuW(menu, MF_STRING, (UINT_PTR)(3002 + contextIndex * 3), L"Delete folder…");
    }
    else
    {
        AppendMenuW(menu, MF_STRING, 1, L"New folder…");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        for (size_t i = 0; i < g_folders.size() && i < 900; ++i)
        {
            HMENU sub = CreatePopupMenu();
            AppendMenuW(sub, MF_STRING, (UINT_PTR)(3000 + i * 3), g_folders[i].collapsed ? L"Expand" : L"Collapse");
            AppendMenuW(sub, MF_STRING, (UINT_PTR)(3001 + i * 3), L"Rename…");
            AppendMenuW(sub, MF_STRING, (UINT_PTR)(3002 + i * 3), L"Delete folder…");
            AppendMenuW(menu, MF_POPUP, (UINT_PTR)sub, g_folders[i].name.c_str());
        }
    }
    RECT r{};
    GetWindowRect(g_folderMenu, &r);
    const int x = contextIndex >= 0 ? contextPoint.x : r.left;
    const int y = contextIndex >= 0 ? contextPoint.y : r.bottom;
    const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, x, y, 0, owner, nullptr);
    DestroyMenu(menu);
    if (selected == 1)
    {
        auto value = PromptText(owner, L"New folder", L"Folder name", L"", false);
        if (value)
        {
            auto name = TrimAndLimitName(*value, 60);
            if (!name.empty())
            {
                g_folders.push_back({NewId(), std::move(name)});
                SaveFolders();
                RefreshChatList();
            }
        }
        return;
    }
    if (selected < 3000 || selected >= 5700)
        return;
    const size_t index = (selected - 3000) / 3;
    if (index >= g_folders.size())
        return;
    if ((selected - 3000) % 3 == 0)
    {
        g_folders[index].collapsed = !g_folders[index].collapsed;
        SaveFolders();
        RefreshChatList();
        return;
    }
    if ((selected - 3000) % 3 == 1)
    {
        auto value = PromptText(owner, L"Rename folder", L"Folder name", g_folders[index].name, false);
        if (value)
        {
            auto name = TrimAndLimitName(*value, 60);
            if (!name.empty())
            {
                g_folders[index].name = std::move(name);
                SaveFolders();
                RefreshChatList();
            }
        }
        return;
    }
    const auto folder = g_folders[index];
    if (MessageBoxW(owner, (L"Delete folder ‘" + folder.name + L"’? Chats stay and return to the main list.").c_str(),
                    L"Delete folder", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
        return;
    const auto folderChatCount = (size_t)std::count_if(g_chats.begin(), g_chats.end(), [&](const Chat &chat) {
        return chat.folder == folder.id;
    });
    size_t savedChatCount = 0;
    for (auto &chat : g_chats)
        if (chat.folder == folder.id)
        {
            chat.folder.clear();
            if (!SaveChatMetadata(chat))
            {
                chat.folder = folder.id;
                break;
            }
            ++savedChatCount;
        }
    if (!NativeFolderDeleteCanRemove(savedChatCount, folderChatCount))
    {
        SetText(g_status, L"The folder was kept because one or more chats could not be moved out. Check chat storage and retry.");
        RefreshChatList();
        return;
    }
    g_folders.erase(g_folders.begin() + index);
    if (!SaveFolders()) g_folders.insert(g_folders.begin() + index, folder);
    RefreshChatList();
}
void FlushStream()
{
    std::vector<StreamPart> parts;
    {
        std::lock_guard<std::mutex> l(g_streamMutex);
        parts.swap(g_streamParts);
    }
    const bool visible = g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].id == g_jobChatId;
    bool refreshLiveAnswer = false, refreshReasoning = false;
    for (auto &part : parts)
    {
        if (part.kind == StreamPart::JOB)
        {
            g_answer.jobId = U(part.text);
            auto chat = std::find_if(g_chats.begin(), g_chats.end(), [](const Chat &c) { return c.id == g_jobChatId; });
            if (chat != g_chats.end())
            {
                chat->pendingJob = g_answer.jobId;
                SaveChatMetadata(*chat);
            }
        }
        else if (part.kind == StreamPart::REASONING)
        {
            const bool firstReasoning = g_answer.reasoning.empty();
            const bool separator = AppendNativeToolRoundText(g_answer.reasoning, g_answer.tools, "rat", U(part.text));
            if (visible)
            {
                if (g_answer.text.empty() && !firstReasoning && g_liveThought.active)
                    AppendLiveThoughtText((separator ? L"\n\n" : L"") + part.text);
                else
                    refreshLiveAnswer = refreshReasoning = true;
            }
        }
        else if (part.kind == StreamPart::TEXT)
        {
            if (visible && NativeThoughtRefreshOnAnswerStart(g_answer.text.empty(), !g_answer.reasoning.empty()))
            {
                g_answer.thinkSecs = std::chrono::duration<double>(Clock::now() - g_actionAt).count();
                refreshReasoning = true;
            }
            AppendNativeToolRoundText(g_answer.text, g_answer.tools, "at", U(part.text));
            if (visible) refreshLiveAnswer = true;
        }
        else if (part.kind == StreamPart::MCP)
        {
            ApplyMcpEvent(g_answer.tools, g_answer.toolLimit, part.tool, g_answer.text.size(),
                          g_answer.reasoning.size());
            if (visible)
            {
                refreshLiveAnswer = true;
            }
        }
        else
        {
            if (part.tool.contains("usage"))
                g_answer.usage = part.tool["usage"];
            if (part.tool.contains("timings"))
                g_answer.timings = part.tool["timings"];
        }
    }
    if (visible && refreshLiveAnswer)
    {
        if (!g_liveAnswer.active || refreshReasoning)
            RenderChat();
        else
            RefreshLiveAnswer(g_chats[g_chat]);
    }
}

void CreateUi();
void EnableBusy(bool b)
{
    g_busy = b ? J_CHAT : g_modelSettingsRequestInFlight;
    UpdateModelSwitchButton();
    if (b)
        SetTimer(g_hwnd, 6, 1000, nullptr);
    else
        KillTimer(g_hwnd, 6);
    for (auto h : {g_send, g_attach, g_attachmentMenu, g_editLast, g_regenerate, g_cancelEdit, g_start, g_stop, g_restart, g_update,
                   g_profileBox})
        if (h)
            EnableWindow(h, !b);
    UpdateChatActionButtons();
    SetText(g_status, b ? L"Maya is writing…" : StatusText());
}
bool CopyUnicodeText(HWND owner, const std::wstring &text)
{
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
    if (!data)
        return false;
    auto *buffer = (wchar_t *)GlobalLock(data);
    if (!buffer)
    {
        GlobalFree(data);
        return false;
    }
    std::copy(text.begin(), text.end(), buffer);
    buffer[text.size()] = L'\0';
    GlobalUnlock(data);
    if (!OpenClipboard(owner))
    {
        GlobalFree(data);
        return false;
    }
    const bool copied = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
    CloseClipboard();
    if (!copied)
        GlobalFree(data);
    return copied;
}
void AddAttachmentPaths(const std::vector<fs::path> &files)
{
    size_t added = 0, rejected = 0;
    std::wstring firstError;
    for (const auto &file : files)
    {
        const auto kind = MayaAttachmentType(file);
        uint64_t size = 0;
        std::string snapshot;
        std::string error;
        if (kind == MayaAttachmentKind::UNSUPPORTED)
            error = MayaAttachmentSizeError(kind, 0, g_health);
        else if (!MayaAttachmentFileSize(file, size))
            error = "Could not read file.";
        else
            error = MayaAttachmentSizeError(kind, size, g_health);
        if (error.empty() && (!MayaReadFileBytes(file, MayaAttachmentReadLimit(kind, g_health), snapshot) ||
                              snapshot.size() != size))
            error = "Could not read file.";
        if (error.empty() && kind == MayaAttachmentKind::TEXT)
        {
            try
            {
                if (MayaAttachmentHasNul(snapshot)) error = "This file looks binary, not text.";
            }
            catch (...) { error = "Could not read text file."; }
        }
        if (!error.empty())
        {
            ++rejected;
            if (firstError.empty()) firstError = W(error) + L" " + file.filename().wstring();
            continue;
        }
        g_attachments.push_back(file);
        g_attachmentSnapshots.push_back(std::move(snapshot));
        ++added;
    }
    if (added) { UpdateComposerHeight(); ScheduleContextMeter(); }
    if (rejected)
        SetText(g_status, std::to_wstring(added) + L" file(s) attached; " + std::to_wstring(rejected) +
                            L" skipped. " + firstError);
    else if (added)
        SetText(g_status, std::to_wstring(added) + L" file(s) attached.");
    if (!files.empty()) SetFocus(g_input);
}
LRESULT CALLBACK InputSubclassProc(HWND input, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR)
{
    if (message == WM_ERASEBKGND)
    {
        RECT rect{};
        GetClientRect(input, &rect);
        FillRect((HDC)w, &rect, g_cardBrush ? g_cardBrush : GetSysColorBrush(COLOR_WINDOW));
        return 1;
    }
    if (message == WM_PAINT)
    {
        const LRESULT result = DefSubclassProc(input, message, w, l);
        if (g_page == CHAT && GetWindowTextLengthW(input) == 0)
        {
            HDC dc = GetDC(input);
            if (dc)
            {
                HFONT font = (HFONT)SendMessageW(input, WM_GETFONT, 0, 0);
                if (!font) font = g_font;
                HGDIOBJ oldFont = font ? SelectObject(dc, font) : nullptr;
                RECT rect{};
                GetClientRect(input, &rect);
                rect.left += 6;
                rect.right -= 6;
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, C_MUTED);
                DrawTextW(dc, L"Ask anything", -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                if (oldFont) SelectObject(dc, oldFont);
                ReleaseDC(input, dc);
            }
        }
        return result;
    }
    if (message != WM_PASTE || g_page != CHAT) return DefSubclassProc(input, message, w, l);
    if (OpenClipboard(input))
    {
        if (IsClipboardFormatAvailable(CF_HDROP))
        {
            auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
            std::vector<fs::path> files;
            if (drop)
            {
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i)
                {
                    const UINT n = DragQueryFileW(drop, i, nullptr, 0);
                    if (!n || n >= 32768) continue;
                    std::wstring path(n + 1, L'\0');
                    if (DragQueryFileW(drop, i, path.data(), n + 1)) { path.resize(n); files.emplace_back(path); }
                }
            }
            auto images = MayaClipboardImages(files, g_images);
            CloseClipboard();
            if (!images.empty())
            {
                AddAttachmentPaths(images);
                return 0;
            }
            return DefSubclassProc(input, message, w, l);
        }
        const bool hasImage = IsClipboardFormatAvailable(CF_DIBV5) || IsClipboardFormatAvailable(CF_DIB) ||
                              IsClipboardFormatAvailable(CF_BITMAP);
        CloseClipboard();
        if (hasImage)
        {
            if (!g_images) return DefSubclassProc(input, message, w, l);
            if (g_chat < 0 || g_chat >= (int)g_chats.size()) return 0;
            HBITMAP bitmap = MayaClipboardBitmap(input);
            if (!bitmap) { SetText(g_status, L"The clipboard image format could not be read."); return 0; }
            const auto path = MayaStoredAttachmentPath(g_chatDir, g_chats[g_chat].id, NewId(), L"pasted-image.png");
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            const bool saved = !ec && MayaEnsureGdiplus(g_gdiplusToken) && MayaSaveClipboardPng(bitmap, path);
            DeleteObject(bitmap);
            if (!saved) { SetText(g_status, L"The clipboard image could not be saved as PNG under Maya's 20 MB limit."); return 0; }
            g_attachments.push_back(path);
            std::string snapshot;
            if (!MayaReadFileBytes(path, 20000000ULL, snapshot))
            {
                g_attachments.pop_back();
                SetText(g_status, L"Clipboard image could not be read after saving.");
                return 0;
            }
            g_attachmentSnapshots.push_back(std::move(snapshot));
            UpdateComposerHeight();
            ScheduleContextMeter();
            SetText(g_status, L"Clipboard image attached.");
            return 0;
        }
    }
    return DefSubclassProc(input, message, w, l);
}
LRESULT CALLBACK ModelListSubclassProc(HWND list, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR)
{
    if (message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(list, ModelListSubclassProc, id);
        return DefSubclassProc(list, message, w, l);
    }
    if (NativeModelListDetailsKey(message, w))
    {
        SendMessageW(GetParent(list), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(list), LBN_DBLCLK), (LPARAM)list);
        return 0;
    }
    if (message == WM_MOUSEWHEEL || (message == WM_KEYDOWN && (w == VK_NEXT || w == VK_PRIOR)))
    {
        PostMessageW(GetParent(list), message, w, l);
        return 0;
    }
    return DefSubclassProc(list, message, w, l);
}
LRESULT CALLBACK PageScrollSubclassProc(HWND control, UINT message, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR)
{
    if (message == WM_NCDESTROY)
    {
        RemoveWindowSubclass(control, PageScrollSubclassProc, id);
        return DefSubclassProc(control, message, w, l);
    }
    if (message == WM_MOUSEWHEEL || (message == WM_KEYDOWN && (w == VK_NEXT || w == VK_PRIOR)))
    {
        PostMessageW(GetParent(control), message, w, l);
        return 0;
    }
    return DefSubclassProc(control, message, w, l);
}
std::string PreferredLanIPv4Address()
{
    ULONG bytes = 15000;
    std::vector<unsigned char> storage(bytes);
    auto *adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data());
    ULONG result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapters, &bytes);
    if (result == ERROR_BUFFER_OVERFLOW)
    {
        storage.resize(bytes);
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data());
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_GATEWAYS, nullptr, adapters, &bytes);
    }
    if (result != NO_ERROR) return {};

    ULONG bestMetric = std::numeric_limits<ULONG>::max();
    std::string best;
    for (auto *adapter = adapters; adapter; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        bool hasGateway = false;
        for (auto *gateway = adapter->FirstGatewayAddress; gateway; gateway = gateway->Next)
        {
            const auto *address = gateway->Address.lpSockaddr;
            if (!address || address->sa_family != AF_INET) continue;
            const auto *octets = reinterpret_cast<const unsigned char *>(
                &reinterpret_cast<const sockaddr_in *>(address)->sin_addr);
            hasGateway = octets[0] || octets[1] || octets[2] || octets[3];
            if (hasGateway) break;
        }
        if (!hasGateway) continue;
        for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next)
        {
            const auto *address = unicast->Address.lpSockaddr;
            if (!address || address->sa_family != AF_INET) continue;
            const auto *octets = reinterpret_cast<const unsigned char *>(
                &reinterpret_cast<const sockaddr_in *>(address)->sin_addr);
            if (octets[0] == 0 || octets[0] == 127 || (octets[0] == 169 && octets[1] == 254) || octets[0] >= 224 ||
                adapter->Ipv4Metric >= bestMetric)
                continue;
            bestMetric = adapter->Ipv4Metric;
            best = std::to_string(octets[0]) + "." + std::to_string(octets[1]) + "." +
                   std::to_string(octets[2]) + "." + std::to_string(octets[3]);
        }
    }
    return best;
}

void UpdateConnectPage()
{
    try
    {
        const Configuration config(g_root, g_profile);
        const auto model = g_health.value("model", Utf8(config.model));
        const auto bindHost = Utf8(config.host);
        const auto connectHost = ApiConnectDisplayHost(bindHost,
            bindHost == "0.0.0.0" ? PreferredLanIPv4Address() : std::string());
        const auto info = BuildApiConnectInfo(connectHost, config.port, model,
                                              g_health.value("api_key", false), !config.apiKey.empty());
        SetText(g_connectValue[0], W(info.openaiBase));
        SetText(g_connectValue[1], W(info.anthropicBase));
        SetText(g_connectValue[2], W(info.model));
        SetText(g_connectAuth, W(config.apiKeyOverride ? "A Windows-user API key override is saved and used by this manager."
            : g_health.contains("api_key") ? info.authentication
            : config.apiKey.empty() ? "Authentication status is not available until Maya connects."
                                    : "The selected profile API key will be sent automatically."));
        SetText(g_connectInfo, L"The password field is stored in Windows Credential Manager for your account. When set, it overrides the selected profile key for this manager only.");
        int example = (int)SendMessageW(g_connectExamples, CB_GETCURSEL, 0, 0);
        if (example < 0 || example > 2) example = 0;
        const auto examples = BuildApiConnectExamples(info, g_health.value("api_key", false));
        SetText(g_connectExample, W(examples[example]));
    }
    catch (const std::exception &e)
    {
        SetText(g_connectAuth, W(e.what()));
    }
}
void PostDone(Job job, Json val = {}, std::string error = {}, bool backgroundPoll = false)
{
    if (!IsWindow(g_hwnd))
        return;
    auto p = new Done{job, std::move(val), std::move(error), backgroundPoll};
    PostMessageW(g_hwnd, WM_DONE, 0, (LPARAM)p);
}
void RequestChatCancellation(std::string id, std::wstring profile)
{
    std::thread([id = std::move(id), profile = std::move(profile)] {
        try
        {
            MayaHttp a(Configuration(g_root, profile));
            a.Post(L"/v1/strata/cancel", {{"id", id}});
        }
        catch (...) {}
    }).detach();
}

void WorkerJob(Job job, std::wstring action, bool backgroundPoll)
{
    auto profile = g_profile;
    std::thread([job, action = std::move(action), profile, backgroundPoll] {
        try
        {
            const DWORD timeout = job == J_RESTART ? 660000 : job == J_UPDATE_START ? 3600000 : 180000;
            auto j = RunWorker(g_worker, g_root, action.c_str(), timeout, g_root / profile);
            PostDone(job, std::move(j), {}, backgroundPoll);
        }
        catch (const std::exception &e)
        {
            PostDone(job, {}, e.what(), backgroundPoll);
        }
    }).detach();
}
void BeginJob(Job job, const wchar_t *action, bool backgroundPoll)
{
    const bool backgroundRoutine = backgroundPoll && (job == J_UPDATE_CHECK || job == J_TURBO_INFO);
    const bool isModelSettingsRequest = job == J_MODEL_SETTINGS_PREVIEW || job == J_MODEL_SETTINGS_SAVE;
    const bool settingsDuringChat = isModelSettingsRequest && g_busy == J_CHAT &&
        NativeModelSettingsRequestMayOverlapChat(true, g_modelSettingsRequestInFlight != J_NONE);
    const bool mcpDuringChat = job == J_MCP && g_busy == J_CHAT;
    if (g_modelSettingsRequestInFlight != J_NONE && !isModelSettingsRequest)
        return;
    if (job == J_UPDATE_CHECK && g_updateCheckInFlight) return;
    if (job == J_TURBO_INFO && g_turboInfoInFlight) return;
    if (g_busy != J_NONE && job != J_STATUS && !settingsDuringChat && !mcpDuringChat && !backgroundRoutine)
        return;
    if (job == J_MCP && g_mcpPollInFlight.exchange(true))
        return;
    if (job == J_REFRESH && g_metricsInFlight.exchange(true))
        return;
    if (isModelSettingsRequest)
        g_modelSettingsRequestInFlight = job;
    if (job == J_UPDATE_CHECK) g_updateCheckInFlight = true;
    if (job == J_TURBO_INFO) g_turboInfoInFlight = true;
    if (!settingsDuringChat && !mcpDuringChat && !backgroundRoutine)
        g_busy = job;
    UpdateChatActionButtons();
    UpdateModelSwitchButton();
    UpdateBudgetSave();
    if (job == J_LOAD || job == J_UNLOAD)
        UpdateModelLoadButton();
    if (NativeJobShowsProgress(backgroundPoll || job == J_STATUS || job == J_REFRESH,
                               job == J_MODELS || job == J_CATALOG) && !settingsDuringChat)
        SetText(g_status, L"Working…");
    if (job == J_START || job == J_STOP || job == J_RESTART)
        SetLifecycleButtons(g_online, true);
    if (job == J_UPDATE_START)
    {
        SetLifecycleButtons(g_online, true);
        SetText(g_status, L"Checking Maya release, dependencies and update backups…");
    }
    if (job == J_RESTART)
    {
        g_restartStartedAt = GetTickCount64();
        SetTimer(g_hwnd, NativeRestartTimerId, 1000, nullptr);
        SetText(g_status, L"Restarting Maya… 00:00 elapsed");
    }
    if (job == J_START || job == J_STOP || job == J_RESTART || job == J_UPDATE_CHECK || job == J_UPDATE_START)
    {
        if (job == J_UPDATE_CHECK || job == J_UPDATE_START) EnableWindow(g_updateInstall, FALSE);
        WorkerJob(job, action ? action : L"status", backgroundPoll);
        return;
    }
    auto profile = g_profile;
    const auto deleteFile = g_deleteFile;
    const auto deleteThen = g_deleteThen;
    const auto deleteFiles = g_deleteFiles;
    const auto priceRequest = g_priceRequest;
    const auto modelSettingsRequest = g_modelSettingsRequest;
    const auto sharedRequest = g_sharedRequest;
    const auto turboRequest = g_turboRequest;
    const auto leftoverPath = g_leftoverPath;
    const auto allRequests = g_allRequests;
    const int budgetRequest = g_budgetRequest;
    const auto budgetInfo = g_contextData.value("budget", Json::object());
    std::thread([job, profile, deleteFile, deleteThen, deleteFiles, priceRequest, modelSettingsRequest, sharedRequest, turboRequest,
                 leftoverPath, allRequests, budgetRequest, budgetInfo, backgroundPoll] {
        try
        {
            MayaHttp a(Configuration(g_root, profile));
            Json j = Json::object();
            switch (job)
            {
            case J_STATUS:
                j = a.Get(L"/health");
                break;
            case J_REFRESH:
                j = a.Get(allRequests ? L"/metrics?requests=all" : L"/metrics");
                break;
            case J_REPORT: {
                const auto reply = a.Request(L"GET", L"/api/report");
                if (reply.status < 200 || reply.status >= 300)
                    throw std::runtime_error(reply.status == 401 ? "This Maya server needs its API key." :
                                                                   "Maya report request failed (HTTP " +
                                                                       std::to_string(reply.status) + ").");
                j = reply.body;
                break;
            }
            case J_MODELS:
                j = a.Get(L"/api/models");
                if (ModelContextTelemetryRequested(j))
                    try { j["live_context"] = a.Get(L"/api/context"); } catch (...) { j["live_context"] = Json::object(); }
                break;
            case J_CATALOG:
                j = a.Get(L"/api/catalog");
                break;
            case J_PULL_STATUS:
                j = a.Get(L"/api/pull/status");
                break;
            case J_PULL_CANCEL:
                j = a.Post(L"/api/pull/cancel", Json::object());
                break;
            case J_MODEL_DELETE_PLAN:
            case J_MODEL_DELETE:
                j = a.Post(L"/api/models/delete", ModelDeleteRequest(deleteFile, job == J_MODEL_DELETE_PLAN,
                                                                       deleteFiles, deleteThen));
                break;
            case J_LOAD:
                j = a.Post(L"/models/load", Json::object());
                break;
            case J_UNLOAD:
                j = a.Post(L"/models/unload", Json::object());
                break;
            case J_MODEL_SETTINGS_PREVIEW:
                j = a.Post(L"/api/models/settings", modelSettingsRequest);
                break;
            case J_MODEL_SETTINGS_SAVE: {
                auto request = modelSettingsRequest;
                request.erase("dry_run");
                j = a.Post(L"/api/models/settings", request);
                break;
            }
            case J_MODEL_LEFTOVER_DELETE:
                j = a.Post(L"/api/models/leftovers", ModelLeftoverRequest(leftoverPath));
                break;
            case J_CONTEXT:
                j["context"] = a.Get(L"/api/context");
                j["port"] = a.Get(L"/api/port");
                j["budget"] = a.Get(L"/api/thinking-budget");
                j["mcp"] = a.Get(L"/mcp");
                j["shared"] = a.Get(L"/settings");
                break;
            case J_MCP:
                j = a.Get(L"/mcp");
                break;
            case J_SETTINGS:
                j = a.Get(L"/settings");
                break;
            case J_SHARED_SAVE:
                j = SaveSharedDefaults(a, sharedRequest);
                break;
            case J_TURBO_INFO:
                j = a.Get(L"/api/turbo");
                break;
            case J_TURBO_SET:
                j = a.Post(L"/api/turbo", turboRequest);
                break;
            case J_BUDGET_SAVE:
                j = SaveThinkingBudget(a, budgetRequest, budgetInfo);
                break;
            case J_SAVINGS:
                j = a.Get(L"/api/savings");
                break;
            case J_SAVINGS_RESET:
                j = a.Post(L"/api/savings/reset", Json::object());
                break;
            case J_SAVINGS_SAVE_PRICE:
            case J_SAVINGS_DEFAULT_PRICE:
                j = a.Post(L"/api/savings/prices", priceRequest);
                break;
            default:
                break;
            }
            PostDone(job, std::move(j), {}, backgroundPoll);
        }
        catch (const MayaHttpError &e)
        {
            PostDone(job, {{"http_status", e.status}}, e.what(), backgroundPoll);
        }
        catch (const std::exception &e)
        {
            PostDone(job, {}, e.what(), backgroundPoll);
        }
    }).detach();
}

void RefreshStatus()
{
    if (g_busy != J_NONE)
        return;
    BeginJob(J_STATUS);
}
bool MayaHistoryHasServerAttachment(const std::vector<Message> &history)
{
    for (const auto &message : history)
        if (message.role == "user")
            for (const auto &file : message.files)
                if (MayaAttachmentType(file) == MayaAttachmentKind::SERVER)
                    return true;
    return false;
}
Json BuildChatMessages(const Chat &chat, const std::vector<Message> &history, bool placeholderImages,
                       const Json &health, MayaHttp *api = nullptr)
{
    Json messages = Json::array();
    const bool keepReasoning = health.is_object() && health.contains("history_thinking") &&
                               health["history_thinking"].is_boolean() && health["history_thinking"].get<bool>();
    const auto system = TrimAndLimitName(W(chat.system), chat.system.size());
    if (!system.empty())
        messages.push_back({{"role", "system"}, {"content", U(system)}});
    for (const auto &m : history)
    {
        Json content = m.text;
        if (m.role == "user" && !m.files.empty())
        {
            std::string text = m.text;
            Json images = Json::array();
            for (size_t fileIndex = 0; fileIndex < m.files.size(); ++fileIndex)
            {
                const auto &f = m.files[fileIndex];
                const auto kind = MayaAttachmentType(f);
                uint64_t size = fileIndex < m.attachmentData.size() ? m.attachmentData[fileIndex].size() : 0;
                if (fileIndex >= m.attachmentData.size() && !MayaAttachmentFileSize(f, size))
                    throw std::runtime_error("Could not read attached file: " + U(f.filename().wstring()));
                const auto invalid = MayaAttachmentSizeError(kind, size, health);
                if (!invalid.empty()) throw std::runtime_error(invalid + " " + U(f.filename().wstring()));
                if (kind == MayaAttachmentKind::SERVER && !api)
                {
                    if (!placeholderImages)
                        throw std::runtime_error("Maya must read this attachment before sending: " + U(f.filename().wstring()));
                    const auto fileBlock = MayaTextFileBlock(U(f.filename().wstring()), "[content is read by Maya when sent]");
                    text = text.empty() ? fileBlock : text + "\n\n" + fileBlock;
                    continue;
                }
                if (kind == MayaAttachmentKind::VIDEO && placeholderImages)
                {
                    images.push_back(MayaMediaContentPart(kind, "data:,"));
                    continue;
                }
                std::string bytes;
                if (!MayaAttachmentData(f, m.attachmentData, fileIndex, MayaAttachmentReadLimit(kind, health), bytes) ||
                    bytes.size() != size)
                    throw std::runtime_error("Could not read attached file: " + U(f.filename().wstring()));
                if (kind == MayaAttachmentKind::IMAGE || kind == MayaAttachmentKind::VIDEO)
                {
                    std::string url = "data:,";
                    if (!placeholderImages)
                    {
                        const auto mime = MayaAttachmentMimeType(f);
                        const auto expected = kind == MayaAttachmentKind::VIDEO ? L"video/" : L"image/";
                        if (mime.rfind(expected, 0) != 0)
                            throw std::runtime_error("Could not determine the media type for attached file: " + U(f.filename().wstring()));
                        url = MayaAttachmentDataUrl(bytes, mime);
                    }
                    images.push_back(MayaMediaContentPart(kind, url));
                }
                else if (kind == MayaAttachmentKind::SERVER)
                    MayaAppendServerAttachment(U(f.filename().wstring()),
                        api->ExtractAttachment(MayaAttachmentHeaderFilename(f.filename().wstring()), bytes,
                                               g_streamHandle, g_cancelled), text, images);
                else
                {
                    const auto data = MayaDecodeTextBytes(bytes);
                    if (data.find('\0') != std::string::npos)
                        throw std::runtime_error("This file looks binary, not text: " + U(f.filename().wstring()));
                    const auto fileBlock = MayaTextFileBlock(U(f.filename().wstring()), data);
                    text = text.empty() ? fileBlock : text + "\n\n" + fileBlock;
                }
            }
            content = MayaUserMessageContent(text, images);
        }
        if (m.role == "assistant")
        {
            for (auto &apiMessage : AssistantApiMessages(m.text, m.tools, m.stopped, !m.error.empty(), m.reasoning, keepReasoning))
                messages.push_back(std::move(apiMessage));
        }
        else
            messages.push_back({{"role", m.role}, {"content", content}});
    }
    return messages;
}
void ShowContextCount(uint64_t tokens, uint64_t maximum, bool approximate)
{
    g_contextTokens = tokens;
    g_contextTokensKnown = true;
    g_contextTokensApproximate = approximate;
    g_contextMeterChatId = g_chat >= 0 && g_chat < (int)g_chats.size() ? g_chats[g_chat].id : L"";
    if (g_contextMeter)
        SetText(g_contextMeter, W(FormatContextMeter(tokens, maximum, approximate)));
    UpdateContextMeterVisibility();
}
void ScheduleContextMeter()
{
    ++g_contextSeq;
    g_contextTokensKnown = false;
    if (g_hwnd)
        SetTimer(g_hwnd, 5, 400, nullptr);
}
void RequestContextCount()
{
    if (g_page != CHAT || g_chat < 0 || g_chat >= (int)g_chats.size())
        return;
    if (g_busy == J_CHAT && g_chats[g_chat].id == g_jobChatId)
        return;
    Chat chat = g_chats[g_chat];
    auto history = chat.messages;
    if (g_editMessage >= 0 && g_editMessage < (int)history.size())
        history.resize((size_t)g_editMessage);
    const int n = GetWindowTextLengthW(g_input);
    std::wstring draft(n + 1, L'\0');
    if (n)
        GetWindowTextW(g_input, draft.data(), n + 1);
    draft.resize(n);
    draft = TrimChatInput(draft);
    if (!draft.empty() || !g_attachments.empty())
    {
        Message user;
        user.role = "user";
        user.text = U(draft);
        user.files = g_attachments;
        if (std::none_of(g_attachments.begin(), g_attachments.end(), [](const auto &file) {
                const auto kind = MayaAttachmentType(file);
                return kind == MayaAttachmentKind::SERVER || kind == MayaAttachmentKind::VIDEO;
            }))
            user.attachmentData = g_attachmentSnapshots;
        history.push_back(std::move(user));
    }
    if (history.empty())
    {
        g_contextTokensKnown = false;
        g_contextMeterChatId.clear();
        SetText(g_contextMeter, L"Context: —");
        UpdateContextMeterVisibility();
        return;
    }
    const auto seq = g_contextSeq.load();
    const auto profile = g_profile;
    const auto maxContext = (uint64_t)g_health.value("max_context", 0LL);
    const auto health = g_health;
    const bool unresolvedAttachments = MayaHistoryHasServerAttachment(history);
    const bool tools = NativeMcpEnabled(g_useTools, g_mcpData);
    const auto effort = U(ChatThinking(chat));
    SetText(g_contextMeter, L"Context: counting…");
    std::thread([chat = std::move(chat), history = std::move(history), profile, maxContext, health, tools, effort, seq,
                 unresolvedAttachments] {
        Json result = Json::object(), body = Json::object();
        try
        {
            body = {{"messages", BuildChatMessages(chat, history, true, health)}, {"reasoning_effort", effort}};
            if (tools)
                body["strata_mcp"] = true;
            MayaHttp api(Configuration(g_root, profile));
            result = api.Post(L"/api/tokens", body);
            if (unresolvedAttachments)
                result["approximate"] = true;
        }
        catch (const std::exception &e)
        {
            const auto chars = body.dump().size();
            result = {{"tokens", (uint64_t)(chars / 3.5)}, {"approximate", true}, {"error", e.what()}};
        }
        if (!result.is_object() || !result.contains("tokens") || !result["tokens"].is_number())
            result = {{"tokens", (uint64_t)(body.dump().size() / 3.5)}, {"approximate", true}};
        result["sequence"] = seq;
        if (!result.contains("max_context"))
            result["max_context"] = maxContext;
        PostDone(J_TOKENS, std::move(result));
    }).detach();
}
void SendChat(bool regenerate = false, std::string resumeJob = {})
{
    if (g_busy != J_NONE || g_chat < 0)
        return;
    auto &chat = g_chats[g_chat];
    const bool generatedTitle = ChatTitleIsAuto(chat);
    Json msgs;
    std::vector<Message> requestHistory;
    bool deferredAttachments = false;
    if (!resumeJob.empty())
    {
        if (!chat.messages.empty())
        {
            const auto &last = chat.messages.back();
            if (NativeChatReplaceFailedAnswerOnResume(last.role == "assistant", last.jobId == resumeJob,
                                                      !last.error.empty()))
            {
                if (!AppendHistoryRecord(chat, TruncateChatHistory(chat.messages.size() - 1)))
                {
                    g_pendingChatRetryAt = GetTickCount64() + 30000;
                    SetText(g_status, L"Could not replace the interrupted answer; the existing chat was kept.");
                    return;
                }
                chat.messages.pop_back();
            }
        }
        g_pendingChatRetryAt = 0;
        g_resumingPendingJob = true;
    }
    else if (!regenerate)
    {
        int n = GetWindowTextLengthW(g_input);
        if (n <= 0 && g_attachments.empty())
            return;
        std::wstring wt(n + 1, L'\0');
        GetWindowTextW(g_input, wt.data(), n + 1);
        wt.resize(n);
        std::string text = U(TrimChatInput(wt));
        if (text.empty() && g_attachments.empty())
            return;
        Message user;
        user.role = "user";
        user.text = text;
        for (size_t fileIndex = 0; fileIndex < g_attachments.size(); ++fileIndex)
        {
            const auto &file = g_attachments[fileIndex];
            const auto &snapshot = g_attachmentSnapshots[fileIndex];
            const auto kind = MayaAttachmentType(file);
            std::string error = MayaAttachmentSizeError(kind, snapshot.size(), g_health);
            if (error.empty() && kind == MayaAttachmentKind::TEXT)
            {
                try
                {
                    if (MayaAttachmentHasNul(snapshot)) error = "This file looks binary, not text.";
                }
                catch (...) { error = "Could not read text file."; }
            }
            fs::path stored;
            std::string storeError;
            if (!error.empty() || !StoreChatAttachment(chat, file, snapshot, stored, storeError))
            {
                SetText(g_status, W(error.empty() ? "Could not save attachment: " + storeError : error));
                return;
            }
            user.files.push_back(std::move(stored));
        }
        requestHistory = chat.messages;
        if (g_editMessage >= 0 && g_editMessage < (int)requestHistory.size())
            requestHistory.resize((size_t)g_editMessage);
        requestHistory.push_back(user);
        deferredAttachments = MayaHistoryHasServerAttachment(requestHistory);
        if (deferredAttachments)
            msgs = Json::array();
        else
        {
            try
            {
                msgs = BuildChatMessages(chat, requestHistory, false, g_health);
            }
            catch (const std::exception &e)
            {
                SetText(g_status, W(e.what()));
                return;
            }
        }
        if (g_editMessage >= 0 && g_editMessage < (int)chat.messages.size())
        {
            if (!AppendHistoryRecord(chat, ReplaceChatHistoryFrom((size_t)g_editMessage, MessageRecord(user))))
            {
                SetText(g_status, L"Could not save the edit; the existing conversation was kept.");
                return;
            }
            chat.messages.resize((size_t)g_editMessage);
            chat.messages.push_back(user);
            if (generatedTitle) chat.title = L"New chat";
            g_editMessage = -1;
            SetText(g_status, L"Edited turn saved; generating a replacement answer.");
        }
        else
        {
            if (!AppendLog(chat, user))
            {
                SetText(g_status, L"Could not save the message; it was not sent.");
                return;
            }
            chat.messages.push_back(user);
        }
        SaveChatMetadata(chat);
        g_attachments.clear();
        g_attachmentSnapshots.clear();
        UpdateComposerHeight();
        SetWindowTextW(g_input, L"");
    }
    else
    {
        requestHistory = chat.messages;
        deferredAttachments = MayaHistoryHasServerAttachment(requestHistory);
        if (deferredAttachments)
            msgs = Json::array();
        else
        {
            try
            {
                msgs = BuildChatMessages(chat, requestHistory, false, g_health);
            }
            catch (const std::exception &e)
            {
                SetText(g_status, W(e.what()));
                return;
            }
        }
    }
    RenderChat();
    RefreshChatList();
    g_answer = Message{};
    {
        std::lock_guard<std::mutex> lock(g_streamMutex);
        g_streamParts.clear();
    }
    g_answer.role = "assistant";
    g_answer.jobId = resumeJob;
    g_jobChatId = chat.id;
    g_actionAt = Clock::now();
    EnableBusy(true);
    ++g_contextSeq;
    KillTimer(g_hwnd, 5);
    {
        std::lock_guard<std::mutex> lock(g_jobIdMutex);
        g_jobId = resumeJob;
    }
    g_cancelled = false;
    g_cancelSent = false;
    SetTimer(g_hwnd, 3, 60, nullptr);
    Json body = Json::object();
    if (resumeJob.empty())
    {
        body = {{"model", g_health.value("model", std::string("maya"))}, {"messages", msgs}, {"stream", true}};
        body["reasoning_effort"] = U(ChatThinking(chat));
        body["strata_resume"] = true;
        if (NativeMcpEnabled(g_useTools, g_mcpData))
            body["strata_mcp"] = true;
        body = ApplyChatSampling(std::move(body),
            ResolveChatSampling(g_samplingPrefs, g_health.value("model", std::string()),
                                g_health.value("defaults", Json::object())));
        body["reasoning_effort"] = U(ChatThinking(chat));
        AddNativeChatAblation(body, g_health, ChatModelIdentity(g_health),
                              chat.hasAblate, chat.ablate, chat.ablateFor);
        const auto activeModel = g_health.value("model", std::string());
        AddNativeChatDefaultThinkBudget(body, g_health, g_samplingPrefs, activeModel,
                                        U(ChatThinking(chat)), chat.thinkingPicked, chat.thinkingFor);
    }
    auto profile = g_profile;
    std::vector<Message> serverHistory;
    std::string serverSystem;
    Json serverHealth;
    if (deferredAttachments)
    {
        serverHistory = std::move(requestHistory);
        for (auto &message : serverHistory) message.attachmentData.clear();
        serverSystem = chat.system;
        serverHealth = g_health;
    }
    std::thread([body = std::move(body), profile, resumeJob, deferredAttachments,
                 history = std::move(serverHistory), system = std::move(serverSystem), health = std::move(serverHealth)]() mutable {
        try
        {
            MayaHttp a(Configuration(g_root, profile));
            if (deferredAttachments)
            {
                Chat requestChat;
                requestChat.system = std::move(system);
                // shortcut: saved server-extracted files are re-read per request; cache only if this proves slow.
                body["messages"] = BuildChatMessages(requestChat, history, false, health, &a);
                if (g_cancelled.load()) throw std::runtime_error("Attachment processing was cancelled.");
            }
            bool announced = !resumeJob.empty();
            a.Stream(body, g_streamHandle, [&announced, profile, &a](const Json &j) {
                std::wstring r, t;
                Json stats = Json::object();
                if (j.contains("usage"))
                    stats["usage"] = j["usage"];
                if (j.contains("timings"))
                    stats["timings"] = j["timings"];
                if (j.contains("id") && j["id"].is_string())
                {
                    const auto id = j["id"].get<std::string>();
                    std::string cancelId;
                    {
                        std::lock_guard<std::mutex> lock(g_jobIdMutex);
                        if (g_jobId.empty()) g_jobId = id;
                        if (NativeChatCancelReady(g_cancelled.load(), g_cancelSent.load(), !g_jobId.empty()) &&
                            !g_cancelSent.exchange(true))
                            cancelId = g_jobId;
                    }
                    if (!cancelId.empty())
                    {
                        RequestChatCancellation(cancelId, profile);
                        a.Cancel(g_streamHandle);
                    }
                    if (!announced)
                    {
                        announced = true;
                        std::lock_guard<std::mutex> lock(g_streamMutex);
                        g_streamParts.push_back({StreamPart::JOB, W(id), {}});
                    }
                }
                if (j.contains("choices") && j["choices"].is_array() && !j["choices"].empty())
                {
                    auto d = j["choices"][0].value("delta", Json::object());
                    if (d.contains("reasoning_content") && d["reasoning_content"].is_string())
                        r = W(d["reasoning_content"].get<std::string>());
                    if (d.contains("content") && d["content"].is_string())
                        t = W(d["content"].get<std::string>());
                }
                if (!r.empty() || !t.empty() || j.contains("strata_mcp") || !stats.empty())
                {
                    std::lock_guard<std::mutex> l(g_streamMutex);
                    if (j.contains("strata_mcp"))
                        g_streamParts.push_back({StreamPart::MCP, {}, j["strata_mcp"]});
                    if (!r.empty())
                        g_streamParts.push_back({StreamPart::REASONING, std::move(r), {}});
                    if (!t.empty())
                        g_streamParts.push_back({StreamPart::TEXT, std::move(t), {}});
                    if (!stats.empty())
                        g_streamParts.push_back({StreamPart::STATS, {}, std::move(stats)});
                }
            }, resumeJob);
            PostDone(J_CHAT, {{"ok", true}});
        }
        catch (const std::exception &e)
        {
            PostDone(J_CHAT, {}, NativeChatFailureText(g_cancelled.load(), e.what()));
        }
    }).detach();
}
void TrySendStartupQuestion()
{
    const bool mayaReady = g_online && g_health.value("status", std::string()) == "ok";
    if (!NativeStartupQuestionCanSend(g_startupQuestion, mayaReady, g_busy == J_NONE,
                                      GetWindowTextLengthW(g_input) == 0, g_attachments.empty()))
        return;
    if (g_chat >= 0 && g_chat < (int)g_chats.size() && !g_chats[g_chat].messages.empty())
    {
        const auto previous = g_chats[g_chat].id;
        NewChat();
        if (g_chat < 0 || g_chats[g_chat].id == previous)
            return;
    }
    ShowPage(CHAT);
    SetWindowTextW(g_input, g_startupQuestion.c_str());
    g_startupQuestion.clear();
    SendChat();
}
void ResumePendingChat()
{
    if (g_chat < 0 || g_chat >= (int)g_chats.size()) return;
    auto &chat = g_chats[g_chat];
    if (!NativePendingChatResumeEligible(true, !chat.pendingJob.empty(), g_busy != J_NONE)) return;
    if (NativeChatResumeCooldownActive(GetTickCount64(), g_pendingChatRetryAt)) return;
    const auto job = chat.pendingJob;
    RefreshChatList();
    RenderChat();
    SetText(g_status, L"Resuming Maya’s saved answer…");
    SendChat(false, job);
}
void EditUserMessage(int index)
{
    if (g_busy != J_NONE)
    {
        SetText(g_status, L"Wait for Maya's answer to finish before editing a chat turn.");
        return;
    }
    if (g_chat < 0 || g_chat >= (int)g_chats.size())
        return;
    if (GetWindowTextLengthW(g_input) || !g_attachments.empty())
        if (MessageBoxW(g_hwnd, L"Replace the current draft with this user turn?", L"Edit chat turn",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            return;
    auto &messages = g_chats[g_chat].messages;
    if (index < 0 || index >= (int)messages.size() || messages[index].role != "user")
        return;
    std::vector<std::string> snapshots;
    for (const auto &file : messages[index].files)
    {
        const auto kind = MayaAttachmentType(file);
        uint64_t size = 0;
        if (!MayaAttachmentFileSize(file, size))
        {
            SetText(g_status, L"Could not read a saved attachment for editing.");
            return;
        }
        const auto invalid = MayaAttachmentSizeError(kind, size, g_health);
        if (!invalid.empty())
        {
            SetText(g_status, W(invalid));
            return;
        }
        std::string bytes;
        if (!MayaReadFileBytes(file, MayaAttachmentReadLimit(kind, g_health), bytes) || bytes.size() != size)
        {
            SetText(g_status, L"Could not read a saved attachment for editing.");
            return;
        }
        snapshots.push_back(std::move(bytes));
    }
    g_editMessage = index;
    g_attachments = messages[index].files;
    g_attachmentSnapshots = std::move(snapshots);
    SetWindowTextW(g_input, W(messages[index].text).c_str());
    SetText(g_status, index == (int)messages.size() - 1
                          ? L"Editing the last user turn. Send replaces it and later turns; Cancel edit keeps history."
                          : L"Editing an earlier user turn. Send replaces it and later turns; Cancel edit keeps history.");
    EnableWindow(g_editLast, FALSE);
    EnableWindow(g_regenerate, FALSE);
    EnableWindow(g_cancelEdit, TRUE);
    SetFocus(g_input);
    ScheduleContextMeter();
}
void EditLastUserMessage()
{
    if (g_busy != J_NONE || g_chat < 0 || g_chat >= (int)g_chats.size()) return;
    const auto &messages = g_chats[g_chat].messages;
    for (int i = (int)messages.size() - 1; i >= 0; --i)
        if (messages[i].role == "user") { EditUserMessage(i); return; }
}
void CancelChatEdit()
{
    if (g_editMessage < 0)
        return;
    g_editMessage = -1;
    g_attachments.clear();
    g_attachmentSnapshots.clear();
    UpdateComposerHeight();
    SetWindowTextW(g_input, L"");
    RenderChat();
    ScheduleContextMeter();
    SetText(g_status, L"Edit cancelled; saved chat history is unchanged.");
}
void RegenerateLastAnswer()
{
    if (g_busy != J_NONE || g_editMessage >= 0 || g_chat < 0 || g_chat >= (int)g_chats.size())
        return;
    auto &messages = g_chats[g_chat].messages;
    const size_t before = messages.size();
    size_t keep = before;
    while (keep && messages[keep - 1].role == "assistant")
        --keep;
    if (!keep || messages[keep - 1].role != "user")
        return;
    if (keep != before && !AppendHistoryRecord(g_chats[g_chat], TruncateChatHistory(keep)))
    {
        SetText(g_status, L"Could not save the regenerate point; chat history was kept.");
        return;
    }
    messages.resize(keep);
    SendChat(true);
}
void StopChat()
{
    g_cancelled = true;
    std::string id;
    {
        std::lock_guard<std::mutex> lock(g_jobIdMutex);
        id = g_jobId;
    }
    if (id.empty())
        SetText(g_status, L"Stop requested; waiting for Maya's stream ID…");
    else if (!g_cancelSent.exchange(true))
        RequestChatCancellation(id, g_profile);
    Api().Cancel(g_streamHandle);
}

void LoadStatusData()
{
    if (g_busy != J_NONE)
        return;
    auto profile = g_profile;
    std::thread([profile] {
        try
        {
            MayaHttp a(Configuration(g_root, profile));
            auto h = a.Get(L"/health");
            PostDone(J_STATUS, h);
        }
        catch (const std::exception &e)
        {
            PostDone(J_STATUS, {}, e.what());
        }
    }).detach();
    g_busy = J_STATUS;
    UpdateChatActionButtons();
}

void RequestLiveMetrics(bool allRequests)
{
    if (g_metricsInFlight.exchange(true)) return;
    auto profile = g_profile;
    std::thread([profile, allRequests] {
        Json result;
        try
        {
            MayaHttp api(Configuration(g_root, profile));
            result = api.Get(allRequests ? L"/metrics?requests=all" : L"/metrics");
        }
        catch (const MayaHttpError &e)
        {
            result = {{"poll_error", e.what()}, {"http_status", e.status}};
        }
        catch (const std::exception &e)
        {
            result = {{"poll_error", e.what()}, {"http_status", 0}};
        }
        catch (...)
        {
            result = {{"poll_error", "Unknown Maya metrics polling error."}, {"http_status", 0}};
        }
        auto *metrics = new Json(std::move(result));
        if (!PostMessageW(g_hwnd, WM_LIVE_METRICS, 0, (LPARAM)metrics))
        {
            delete metrics;
            g_metricsInFlight = false;
        }
    }).detach();
}

std::wstring ActiveLanIpv4()
{
    ULONG bytesNeeded = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, nullptr, &bytesNeeded) != ERROR_BUFFER_OVERFLOW || !bytesNeeded)
        return {};
    std::vector<BYTE> storage(bytesNeeded);
    auto *adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(storage.data());
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &bytesNeeded) != NO_ERROR)
        return {};

    DWORD defaultIfIndex = 0;
    const bool hasDefaultRoute = GetBestInterface(0x01010101u, &defaultIfIndex) == NO_ERROR;
    const auto findAddress = [&](bool defaultOnly) -> std::wstring {
        for (auto *adapter = adapters; adapter; adapter = adapter->Next)
        {
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
                (defaultOnly && adapter->IfIndex != defaultIfIndex)) continue;
            for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next)
            {
                if (!unicast->Address.lpSockaddr || unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
                const auto *ipv4 = reinterpret_cast<const SOCKADDR_IN *>(unicast->Address.lpSockaddr);
                const auto &octets = ipv4->sin_addr.S_un.S_un_b;
                if (octets.s_b1 == 0 || octets.s_b1 == 127 || octets.s_b1 >= 224 ||
                    (octets.s_b1 == 169 && octets.s_b2 == 254)) continue;
                wchar_t address[16]{};
                swprintf_s(address, std::size(address), L"%u.%u.%u.%u",
                    (unsigned)octets.s_b1, (unsigned)octets.s_b2,
                    (unsigned)octets.s_b3, (unsigned)octets.s_b4);
                return address;
            }
        }
        return {};
    };
    auto address = hasDefaultRoute ? findAddress(true) : std::wstring();
    return address.empty() ? findAddress(false) : address;
}

std::wstring HeaderApiAddress()
{
    const auto configured = Configuration(g_root, g_profile).Address();
    const auto colon = configured.rfind(L':');
    if (colon == std::wstring::npos || configured.substr(0, colon) != L"0.0.0.0") return configured;
    auto host = ActiveLanIpv4();
    if (host.empty()) host = L"127.0.0.1";
    return host + configured.substr(colon);
}

void OpenHeaderApiAddress(HWND owner)
{
    const auto address = HeaderApiAddress();
    const auto colon = address.rfind(L':');
    if (colon == std::wstring::npos)
    {
        SetText(g_status, L"Maya's API address could not be read.");
        return;
    }
    const auto ip = address.substr(0, colon);
    const auto url = L"http://" + address + L"/";
    const bool copied = CopyUnicodeText(owner, ip);
    const auto opened = (INT_PTR)ShellExecuteW(owner, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if (opened <= 32)
    {
        if (copied) SetText(g_status, L"IPv4 copied, but Windows could not open Maya.");
        else SetText(g_status, L"Windows could not open Maya.");
    }
    else if (copied)
        SetText(g_status, L"Copied " + ip + L" and opened Maya in your browser.");
    else
        SetText(g_status, L"Opened Maya in your browser.");
}

void UpdateAboutFacts()
{
    if (g_aboutFacts)
        SetText(g_aboutFacts, W(MayaAboutFacts(g_health, g_metrics, U(Configuration(g_root, g_profile).Address()))));
    const auto engine = g_metrics.value("engine", Json::object());
    const auto version = engine.value("maya_version", g_health.value("version", std::string()));
    const bool validVersion = !version.empty() && version.size() <= 32 &&
        version.find('.') != std::string::npos &&
        std::all_of(version.begin(), version.end(), [](unsigned char c) { return std::isdigit(c) || c == '.'; });
    if (validVersion)
    {
        g_mayaReleaseUrl = L"https://github.com/mw00/project-maya/releases/tag/v" + W(version);
        SetText(g_mayaVersionLink, W("Project Maya v" + version + " · View release ↗"));
        ShowWindow(g_mayaVersionLink, g_page == ABOUT ? SW_SHOW : SW_HIDE);
    }
    else
    {
        g_mayaReleaseUrl.clear();
        ShowWindow(g_mayaVersionLink, SW_HIDE);
    }
}

void UpdateSupportLinkVisibility()
{
    const auto placement = NativeSupportLinkPlacementFor(g_updateInstallVisible, g_updateWatching);
    if (g_supportAbout) ShowWindow(g_supportAbout, g_page == ABOUT && placement.about ? SW_SHOW : SW_HIDE);
}

void ApplyCompactVisibility()
{
    for (HWND child = GetWindow(g_hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        const bool controller = child == g_headerLayer || child == g_profileBox || child == g_start || child == g_stop || child == g_restart ||
                                child == g_update || child == g_compactToggle || child == g_status || child == g_detail;
        ShowWindow(child, controller ? SW_SHOW : SW_HIDE);
    }
}

void SetPageControls()
{
    if (g_compactMode)
    {
        ApplyCompactVisibility();
        return;
    }
    if (g_page != MODELS) g_modelSheetOpen = false;
    if (g_modelSheetOpen) UpdateModelSheetView();
    for (auto h : {g_chatList,    g_chatSearch,    g_newChat,     g_folderMenu,   g_editLast,       g_regenerate,    g_cancelEdit, g_answerOptions, g_chatOptions,
                   g_contextMeter, g_transcript, g_greeting, g_greetingModel, g_chatModelMenu, g_ideas[0], g_ideas[1], g_ideas[2], g_ideas[3], g_jumpLatest, g_input, g_send,
                   g_attach, g_attachmentMenu, g_reason, g_reasonValue, g_ablate, g_stopAnswer, g_monitorRequests, g_monitorRequestsHeading, g_models, g_catalog, g_modelsHeading, g_catalogHeading, g_catalogDescription, g_modelAddButton, g_pullProgress,
                   g_modelAction, g_modelDelete, g_modelDownload, g_modelCancel, g_modelRefresh, g_modelLoad,
                   g_modelSettingsButton, g_modelSetupCommand, g_modelSettingsEdit, g_modelSettingsPreview, g_modelSettingsPreviewButton,
                   g_modelSettingsSave, g_modelSettingsCancel, g_modelSettingsModeButton, g_modelSettingsEnvButton,
                   g_modelSettingsEnvInfo, g_modelSettingsEnvList, g_modelSettingsEnvNameLabel, g_modelSettingsEnvName,
                   g_modelSettingsEnvValueLabel, g_modelSettingsEnvValue, g_modelSettingsEnvAdd, g_modelSettingsEnvRemove,
                   g_modelSettingsHint, g_modelSettingsResetButton,
                   g_modelSettingsTabs[0], g_modelSettingsTabs[1], g_modelSettingsTabs[2], g_modelSettingsTabs[3],
                   g_modelSettingsWarning,
                   g_modelSheetHeading, g_modelSheetTabs[0], g_modelSheetTabs[1], g_modelSheetTabs[2],
                   g_modelSheetContent, g_modelSheetEditEngine, g_modelSheetClose,
                   g_leftoverList, g_leftoverDetails,
                   g_leftoverDelete, g_leftoverClose, g_leftoverOpenButton, g_settings, g_contextLabel, g_contextEdit, g_portLabel, g_turboButton,
                   g_portEdit, g_budgetLabel, g_budgetEdit, g_budgetSave, g_budgetHint, g_toolsCheck, g_mcpDetails, g_settingsSave, g_settingsStatus,
                   g_connectAuth, g_connectInfo, g_connectKeyLabel, g_connectKey, g_connectKeySave, g_connectKeyClear,
                   g_connectExampleLabel, g_connectExamples, g_connectExample, g_connectCopyExample,
                   g_defaultInstructionsLabel, g_defaultInstructions, g_saveDefaultInstructions,
                   g_windowsStartupCheck, g_autoStartMayaCheck, g_minimizeToTrayCheck, g_saveWindowsStartup, g_themeLabel,
                   g_schemeLabel, g_themeBox, g_schemeBox, g_updateSummary, g_updateNotes, g_updateInstall,
                    g_supportAbout, g_authorCredit, g_authorGithub, g_authorHuggingFace,
                    g_mayaProjectCredit, g_mayaCreatorGithub, g_mayaCreatorHuggingFace,
                    g_mayaComaintainerGithub, g_mayaVersionLink, g_releaseNotes,
                     g_showThinkingCheck, g_reportButton, g_requestToggle, g_aboutFacts, g_aboutReport, g_aboutReportCopy, g_dataInfo,
                   GetDlgItem(g_hwnd, 260), g_samplingPreset, g_samplingEffort, g_sharedDefaultsCheck, g_sharedDefaultsSummary,
                   g_samplingSave, g_samplingReset, g_samplingStatus, g_clearDataButton, g_shortcutsButton,
                   g_savings, g_savingsRefresh, g_savingsReset, g_priceModels, g_priceInput, g_priceCached,
                   g_priceOutput, g_pricePeak, g_priceSave, g_priceCancel, g_priceDefault,
                   g_priceLabels[0], g_priceLabels[1], g_priceLabels[2], g_priceLabels[3], g_priceLabels[4]})
        if (h)
            ShowWindow(h, SW_HIDE);
    if (g_transcript) EnableWindow(g_transcript, g_page == CHAT);
    for (auto h : g_monitorGpuButtons) if (h) ShowWindow(h, SW_HIDE);
    for (auto h : g_samplingLabels) if (h) ShowWindow(h, SW_HIDE);
    for (auto h : g_samplingFields) if (h) ShowWindow(h, SW_HIDE);
    for (auto h : g_connectTitle) if (h) ShowWindow(h, SW_HIDE);
    for (auto h : g_connectValue) if (h) ShowWindow(h, SW_HIDE);
    for (auto h : g_connectCopy) if (h) ShowWindow(h, SW_HIDE);
    ShowModelSettingsFields(false);
    if (g_page == CHAT)
    {
        for (auto h : {g_chatList, g_chatSearch, g_newChat, g_folderMenu, g_chatOptions,
                       g_transcript, g_greeting, g_greetingModel, g_chatModelMenu, g_ideas[0], g_ideas[1], g_ideas[2], g_ideas[3], g_input, g_send, g_attach, g_attachmentMenu, g_reason, g_reasonValue, g_ablate})
            if (h)
                ShowWindow(h, SW_SHOW);
        ApplyChatAblation();
        UpdateChatActionButtons();
        UpdateContextMeterVisibility();
        UpdateGreeting();
        if (g_busy == J_CHAT)
            ShowWindow(g_stopAnswer, SW_SHOW);
        UpdateJumpLatest();
    }
    else if (g_page == MONITOR)
    {
        if (g_monitorRequestsHeading) ShowWindow(g_monitorRequestsHeading, SW_SHOW);
        if (g_monitorRequests) ShowWindow(g_monitorRequests, SW_SHOW);
        ShowWindow(g_reportButton, SW_SHOW);
        const auto gpuRows = MonitorGpuRows(g_metrics);
        if (gpuRows.size() > 1 && !gpuRows.front().value("plain", false))
            for (size_t i = 0; i < gpuRows.size() && i < 16; ++i)
                if (g_monitorGpuButtons[i]) ShowWindow(g_monitorGpuButtons[i], SW_SHOW);
        const auto kept = g_metrics.value("requests_kept", 0);
        if (g_allRequests || kept > 12)
        {
            SetText(g_requestToggle, g_allRequests ? L"Show last 12" : L"Show all " + std::to_wstring(kept));
            ShowWindow(g_requestToggle, SW_SHOW);
        }
    }
    else if (g_page == MODELS)
    {
        if (g_modelSheetOpen) {
            for (auto h : {g_modelSheetHeading, g_modelSheetTabs[0], g_modelSheetTabs[1], g_modelSheetTabs[2],
                           g_modelSheetContent, g_modelSheetClose})
                if (h) ShowWindow(h, SW_SHOW);
            if (g_modelSheetTab == 1) ShowWindow(g_modelSheetEditEngine, SW_SHOW);
            if (g_modelSheetTab == 2)
            {
                SetText(g_modelDelete, L"Review deletion…");
                ShowWindow(g_modelDelete, SW_SHOW);
            }
        } else if (g_modelSettingsOpen) {
            for (auto h : {g_modelSettingsModeButton, g_modelSettingsHint, g_modelSettingsResetButton, g_modelSettingsPreview,
                           g_modelSettingsPreviewButton, g_modelSettingsSave, g_modelSettingsCancel})
                if (h) ShowWindow(h, SW_SHOW);
            if (g_modelSettingsAdvanced && g_modelSettingsEnvironmentOpen)
            {
                for (auto control : {g_modelSettingsEnvInfo, g_modelSettingsEnvList, g_modelSettingsEnvNameLabel,
                                     g_modelSettingsEnvName, g_modelSettingsEnvValueLabel, g_modelSettingsEnvValue,
                                     g_modelSettingsEnvAdd, g_modelSettingsEnvRemove})
                    if (control) ShowWindow(control, SW_SHOW);
            }
            else if (g_modelSettingsAdvanced)
            {
                ShowWindow(g_modelSettingsEdit, SW_SHOW);
                ShowWindow(g_modelSettingsEnvButton, SW_SHOW);
            }
            else
            {
                for (int i = 0; i < 4; ++i)
                {
                    const bool hasFields = std::any_of(g_modelSettingFields.begin(), g_modelSettingFields.end(),
                        [i](const auto &field) { return field.group == i; });
                    if (hasFields) ShowWindow(g_modelSettingsTabs[i], SW_SHOW);
                }
                ShowModelSettingsFields(true);
                if (GetWindowTextLengthW(g_modelSettingsWarning) > 0) ShowWindow(g_modelSettingsWarning, SW_SHOW);
            }
        } else if (g_leftoverOpen) {
            for (auto h : {g_leftoverList, g_leftoverDetails, g_leftoverDelete, g_leftoverClose})
                if (h) ShowWindow(h, SW_SHOW);
        } else {
            for (auto h : {g_modelsHeading, g_modelAddButton, g_models, g_modelAction, g_modelDelete, g_modelRefresh,
                           g_modelLoad, g_modelSettingsButton, g_leftoverOpenButton})
                if (h) ShowWindow(h, SW_SHOW);
            SetText(g_modelDelete, L"Delete selected");
            const bool showCatalog = ModelCatalogVisible(g_modelsAddOpen, g_pullActive);
            if (showCatalog)
                for (auto h : {g_catalogHeading, g_catalogDescription, g_catalog, g_pullProgress})
                    if (h) ShowWindow(h, SW_SHOW);
            if (g_modelsAddOpen) ShowWindow(g_modelSetupCommand, SW_SHOW);
            if (g_modelsAddOpen) ShowWindow(g_modelDownload, SW_SHOW);
            if (g_pullActive) ShowWindow(g_modelCancel, SW_SHOW);
            if (g_pullActive) ShowWindow(g_pullProgress, SW_SHOW);
            SetText(g_modelAddButton, g_modelsAddOpen ? L"Hide downloads" : L"Add a model");
            UpdateCatalogDownloadButton();
        }
    }
    else if (g_page == SETTINGS)
    {
        for (auto h :
             {g_settings, g_contextLabel, g_contextEdit, g_portLabel, g_portEdit, g_budgetSave, g_budgetHint, g_toolsCheck, g_mcpDetails, g_settingsSave, g_settingsStatus,
               g_defaultInstructionsLabel, g_defaultInstructions, g_saveDefaultInstructions, g_showThinkingCheck,
               g_windowsStartupCheck, g_autoStartMayaCheck, g_minimizeToTrayCheck, g_saveWindowsStartup, g_shortcutsButton,
               g_themeLabel, g_schemeLabel, g_themeBox, g_schemeBox})
            if (h)
                ShowWindow(h, SW_SHOW);
        ShowWindow(g_budgetLabel, g_budgetSupported ? SW_SHOW : SW_HIDE);
        if (g_budgetSupported) ShowWindow(g_budgetEdit, SW_SHOW);
        SetText(g_settingsSave, L"Apply settings");
        UpdateBudgetSave();
        LoadWindowsStartupControls();
    }
    else if (g_page == SAVINGS)
    {
        for (auto h : {g_savings, g_savingsRefresh, g_savingsReset, g_priceModels, g_priceInput, g_priceCached,
                       g_priceOutput, g_pricePeak, g_priceSave, g_priceCancel, g_priceDefault})
            if (h) ShowWindow(h, SW_SHOW);
        for (auto h : g_priceLabels) if (h) ShowWindow(h, SW_SHOW);
        UpdatePriceDraftControls();
    }
    else if (g_page == CHAT_DEFAULTS)
    {
        for (auto h : {g_samplingPreset, g_samplingEffort, g_sharedDefaultsCheck, g_sharedDefaultsSummary,
                       g_samplingSave, g_samplingReset, g_samplingStatus, g_turboButton,
                       g_contextLabel, g_contextEdit, g_portLabel, g_portEdit, g_settingsSave, g_settingsStatus})
            if (h) ShowWindow(h, SW_SHOW);
        SetText(g_settingsSave, L"Apply context / port");
        for (auto h : g_samplingLabels) if (h) ShowWindow(h, SW_SHOW);
        for (auto h : g_samplingFields) if (h) ShowWindow(h, SW_SHOW);
    }
    else if (g_page == CONNECT)
    {
        for (auto h : g_connectTitle) if (h) ShowWindow(h, SW_SHOW);
        for (auto h : g_connectValue) if (h) ShowWindow(h, SW_SHOW);
        for (auto h : g_connectCopy) if (h) ShowWindow(h, SW_SHOW);
        for (auto h : {g_connectAuth, g_connectInfo, g_connectKeyLabel, g_connectKey, g_connectKeySave,
                       g_connectKeyClear, g_connectExampleLabel, g_connectExamples,
                       g_connectExample, g_connectCopyExample}) if (h) ShowWindow(h, SW_SHOW);
        UpdateConnectPage();
    }
    else if (g_page == ABOUT)
    {
        UpdateAboutFacts();
        SetText(g_updateSummary, W(g_updateSummaryText));
        SetText(g_releaseNotes, W(g_latestReleaseNotesText));
        for (auto h : {g_aboutFacts, g_aboutReportCopy, GetDlgItem(g_hwnd, 260),
                       g_authorCredit, g_authorGithub, g_authorHuggingFace, g_mayaProjectCredit,
                       g_mayaCreatorGithub, g_mayaCreatorHuggingFace, g_mayaComaintainerGithub,
                       g_releaseNotes})
            if (h) ShowWindow(h, SW_SHOW);
        ShowWindow(g_updateSummary, SW_SHOW);
        ShowWindow(g_updateNotes, g_updateNotesUrl.empty() ? SW_HIDE : SW_SHOW);
        ShowWindow(g_updateInstall, g_updateInstallVisible ? SW_SHOW : SW_HIDE);
        UpdateSupportLinkVisibility();
        if (g_aboutLogVisible && GetWindowTextLengthW(g_aboutReport) > 0) ShowWindow(g_aboutReport, SW_SHOW);
        SetWindowTextW(GetDlgItem(g_hwnd, 260), g_aboutLogVisible ? L"Hide report" : L"Show report");
    }
    else if (g_page == DATA)
    {
        SetText(g_dataInfo, NativeDataPageText(g_chats.size(), g_folders.size(), g_chatDir));
        ShowWindow(g_dataInfo, SW_SHOW);
        ShowWindow(g_clearDataButton, SW_SHOW);
    }
}

int MonitorPageContentTopFor(HDC dc, int width, const Json &metrics)
{
    const auto banners = MonitorBanners(metrics);
    if (banners.empty())
        return 150;
    if (!dc)
        return 150 + (int)banners.size() * 40;
    const auto previousFont = dc && g_small ? SelectObject(dc, g_small) : nullptr;
    int bottom = 138;
    for (const auto &banner : banners)
    {
        RECT text{0, 0, std::max(1, width - 56), 0};
        DrawTextW(dc, banner.text.c_str(), -1, &text, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        bottom += std::max(32, (int)(text.bottom - text.top) + 16) + 8;
    }
    if (previousFont) SelectObject(dc, previousFont);
    return std::max(150, bottom);
}

int MonitorPageContentTop(int width, const Json &metrics)
{
    HDC dc = g_hwnd ? GetDC(g_hwnd) : nullptr;
    if (!dc)
        return 150 + (int)MonitorBanners(metrics).size() * 40;
    const int top = MonitorPageContentTopFor(dc, width, metrics);
    ReleaseDC(g_hwnd, dc);
    return top;
}

void Layout()
{
    if (!g_hwnd)
        return;
    NativeLayoutTransition transition;
    RECT c{};
    GetClientRect(g_hwnd, &c);
    if (g_compactMode)
    {
        Place(g_headerLayer, 0, 0, (int)c.right, std::min(128, (int)c.bottom));
        Place(g_profileBox, 138, 18, 208, 34);
        Place(g_start, 356, 18, 88, 34);
        Place(g_stop, 452, 18, 82, 34);
        Place(g_restart, 542, 18, 94, 34);
        Place(g_update, 644, 18, 100, 34);
        Place(g_compactToggle, 752, 18, 120, 34);
        Place(g_status, 24, 72, std::max(120, (int)c.right - 48), 26);
        Place(g_detail, 24, 104, std::max(120, (int)c.right - 48), 22);
        ApplyCompactVisibility();
        RaiseHeaderControls();
        return;
    }
    const auto gpuRows = MonitorGpuRows(g_metrics).size();
    const bool gpuExpanded = g_monitorGpuOpen >= 0;
    const size_t requestRows = g_monitorRequests
        ? (size_t)std::max(0, ListView_GetItemCount(g_monitorRequests)) : 0;
    const int pageStartX = SidebarContentLeft();
    const auto initialBounds = MonitorPageBoundsFor(pageStartX, std::max(0, (int)c.right - pageStartX - 28));
    const int initialWidth = initialBounds.width;
    LONG_PTR style = GetWindowLongPtrW(g_hwnd, GWL_STYLE);
    if (style & WS_VSCROLL)
    {
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, style & ~((LONG_PTR)WS_VSCROLL));
        SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    if (g_page != MONITOR) g_monitorScrollY = 0;
    GetClientRect(g_hwnd, &c);
    int w = c.right, h = c.bottom, side = SidebarWidth(), top = 128;
    const auto monitorBounds = MonitorPageBoundsFor(pageStartX, std::max(0, w - pageStartX - 28));
    const int monitorPageX = monitorBounds.left, monitorPageWidth = monitorBounds.width;
    const int monitorContentTop = g_page == MONITOR ? MonitorPageContentTop(monitorPageWidth, g_metrics) : 150;
    const int monitorScrollLimit = g_page == MONITOR
        ? MonitorPageScrollLimit(monitorContentTop, monitorPageWidth, gpuRows, gpuExpanded, h - 22, requestRows) : 0;
    g_monitorScrollY = std::clamp(g_monitorScrollY, 0, monitorScrollLimit);
    const bool modelCatalogPage = g_page == MODELS && !g_modelSheetOpen && !g_modelSettingsOpen && !g_leftoverOpen &&
                                  ModelCatalogVisible(g_modelsAddOpen, g_pullActive);
    const int catalogRows = modelCatalogPage && g_catalog
        ? std::max(0, (int)SendMessageW(g_catalog, LB_GETCOUNT, 0, 0)) : 0;
    const int installedRows = g_models ? std::max(0, (int)SendMessageW(g_models, LB_GETCOUNT, 0, 0)) : 0;
    const int catalogListHeight = std::max(88, catalogRows * 88);
    const int installedListHeight = std::max(118, installedRows * 118);
    const int modelButtonsRawY = top + std::max(56 + installedListHeight, 100 + catalogListHeight) + 26;
    if (g_page == MONITOR)
        g_pageScrollLimit = monitorScrollLimit;
    else if (modelCatalogPage)
        g_pageScrollLimit = std::max(0, modelButtonsRawY + 34 + 16 - (h - 22));
    else
        g_pageScrollLimit = 0;
    if (!modelCatalogPage) g_catalogScrollY = 0;
    if (g_page == MONITOR)
    {
        g_monitorScrollY = std::clamp(g_monitorScrollY, 0, g_pageScrollLimit);
    }
    else if (modelCatalogPage)
        g_catalogScrollY = std::clamp(g_catalogScrollY, 0, g_pageScrollLimit);
    const int navTop = 132, navStep = 48;
    for (int i = 0; i < 9; ++i)
    {
        ShowWindow(g_nav[i], SW_SHOW);
        Place(g_nav[i], g_sidebarOpen ? 20 : 12, navTop + i * navStep,
              g_sidebarOpen ? 200 : 48, g_sidebarOpen ? 40 : 42);
    }
    Place(g_sidebarToggle, g_sidebarOpen ? 206 : 12, g_sidebarOpen ? 28 : 68,
          g_sidebarOpen ? 28 : 48, 28);
    Place(g_modelSidebar, g_sidebarOpen ? 18 : 12, std::max(116, h - (g_sidebarOpen ? 124 : 128)),
          g_sidebarOpen ? 204 : 48, g_sidebarOpen ? 60 : 48);
    ShowWindow(g_modelSidebar, SW_SHOW);
    Place(g_savedLine, 20, std::max(552, h - 52), 200, 32);
    UpdateSavingsSidebar();
    const int controllerLeft = side + 24;
    const int controllerWidth = std::max(0, w - controllerLeft - 24);
    const bool fullControllerRow = controllerWidth >= 848;
    const int controlGap = controllerWidth < 400 ? 0 : controllerWidth < 600 ? 4 : 8;
    const int maxProfileWidth = std::max(0, controllerWidth - 5 * controlGap);
    const int profileWidth = fullControllerRow ? 260
        : std::clamp(controllerWidth * 3 / 10, std::min(96, maxProfileWidth), maxProfileWidth);
    const int actionWidth = fullControllerRow ? 0
        : std::max(0, (controllerWidth - profileWidth - 5 * controlGap) / 5);
    int actionX = controllerLeft + profileWidth + controlGap;
    Place(g_profileBox, controllerLeft, 18, profileWidth, 34);
    Place(g_start, actionX, 18, fullControllerRow ? 108 : actionWidth, 34);
    actionX += (fullControllerRow ? 108 : actionWidth) + controlGap;
    Place(g_stop, actionX, 18, fullControllerRow ? 108 : actionWidth, 34);
    actionX += (fullControllerRow ? 108 : actionWidth) + controlGap;
    Place(g_restart, actionX, 18, fullControllerRow ? 108 : actionWidth, 34);
    actionX += (fullControllerRow ? 108 : actionWidth) + controlGap;
    Place(g_update, actionX, 18, fullControllerRow ? 124 : actionWidth, 34);
    actionX += (fullControllerRow ? 124 : actionWidth) + controlGap;
    Place(g_compactToggle, actionX, 18, fullControllerRow ? 100 : actionWidth, 34);
    const int statusX = side + 24;
    const int undoWidth = NativeChatUndoAvailable(g_pendingChatDeletes.size()) ? 84 : 0;
    Place(g_status, statusX, 65, std::max(0, w - statusX - undoWidth - 32), 24);
    Place(g_undoDelete, w - 104, 61, 80, 30);
    ShowWindow(g_undoDelete, NativeChatUndoAvailable(g_pendingChatDeletes.size()) ? SW_SHOW : SW_HIDE);
    Place(g_detail, statusX, 91, std::max(0, w - statusX - 24), 22);
    Place(g_headerLayer, 0, 0, w, 128);
    int x = side + 24, bodyY = top, bodyW = w - side - 48, bodyH = h - top - 22;
    const auto chatColumn = NativeChatColumnFor(x, bodyW, 220, 768);
    const int chatX = chatColumn.left, chatW = chatColumn.width;
    Place(g_newChat, x, bodyY, 126, 32);
    Place(g_folderMenu, x + 134, bodyY, 72, 32);
    Place(g_chatSearch, x, bodyY + 38, 206, 32);
    Place(g_chatList, x, bodyY + 78, 206, bodyH - 252);
    const int chatOptionsWidth = std::min(104, chatW);
    Place(g_chatOptions, chatX + std::max(0, chatW - chatOptionsWidth), bodyY, chatOptionsWidth, 30);
    Place(g_chatModelMenu, chatX, bodyY, std::min(330, std::max(0, chatW - chatOptionsWidth - 16)), 30);
    const int composerTop = bodyY + bodyH - g_inputHeight - 52;
    Place(g_transcript, chatX + 8, bodyY + 42, std::max(0, chatW - 16), std::max(0, composerTop - bodyY - 100));
    Place(g_greeting, chatX + 24, bodyY + 82, std::max(0, chatW - 48), 38);
    Place(g_greetingModel, chatX + 24, bodyY + 126, std::max(0, chatW - 48), 42);
    const int ideaGap = 12, ideaWidth = std::max(0, (chatW - 48 - ideaGap) / 2);
    for (int i = 0; i < 4; ++i)
        Place(g_ideas[i], chatX + 24 + (i % 2) * (ideaWidth + ideaGap), bodyY + 182 + (i / 2) * 52, ideaWidth, 42);
    Place(g_jumpLatest, chatX + std::max(0, chatW - 122), bodyY + bodyH - 158, std::min(104, chatW), 28);
    Place(g_editLast, chatX, bodyY + bodyH - 144, 110, 28);
    Place(g_regenerate, chatX + 118, bodyY + bodyH - 144, 110, 28);
    Place(g_cancelEdit, chatX + 236, bodyY + bodyH - 144, 100, 28);
    Place(g_answerOptions, chatX + 344, bodyY + bodyH - 144, 112, 28);
    Place(g_contextMeter, chatX + 14, composerTop - 34, chatW - 28, 22);
    Place(g_input, chatX + 12, composerTop, std::max(0, chatW - 24), g_inputHeight);
    const int composerY = bodyY + bodyH - 42;
    Place(g_attach, chatX + 12, composerY, 96, 34);
    Place(g_attachmentMenu, chatX + 116, composerY, 120, 34);
    const int sendWidth = std::min(104, std::max(72, chatW - 24));
    const int sendX = chatX + std::max(12, chatW - sendWidth - 12);
    Place(g_send, sendX, composerY, sendWidth, 34);
    Place(g_stopAnswer, chatX + std::max(12, chatW - 238), composerY, std::min(108, chatW - 24), 34);
    const int leftEnd = chatX + 236;
    const bool showAblation = chatW >= 1040 && sendX - leftEnd >= 382;
    const int ablateWidth = showAblation ? 142 : 0;
    const int groupWidth = 196 + (showAblation ? 150 : 0);
    const int minGroupX = leftEnd + 14;
    const int maxGroupX = std::max(minGroupX, sendX - groupWidth - 14);
    const int groupStart = std::clamp(chatX + (chatW - groupWidth) / 2, minGroupX, maxGroupX);
    Place(g_reasonValue, groupStart, composerY, 82, 34);
    Place(g_reason, groupStart + 90, composerY, 106, 34);
    Place(g_ablate, groupStart + 204, composerY, ablateWidth, 34);
    if (!showAblation) ShowWindow(g_ablate, SW_HIDE);
    else ApplyChatAblation();
    const int monitorPageTop = monitorContentTop - g_monitorScrollY;
    const int monitorActionsY = MonitorGpuActionsTop(monitorPageTop, monitorPageWidth, gpuRows, gpuExpanded);
    const int monitorTextY = monitorActionsY + 40;
    const bool showAllRequests = g_allRequests || MonitorInteger(g_metrics, "requests_kept") > 12;
    Place(g_monitorRequestsHeading, monitorPageX + 2, monitorTextY, std::max(80, monitorPageWidth - (showAllRequests ? 160 : 4)), 24);
    Place(g_requestToggle, monitorPageX + std::max(0, monitorPageWidth - 152), monitorTextY - 4, 148, 30);
    const bool requestHeaderVisible = g_page == MONITOR && monitorTextY >= top && monitorTextY + 30 <= h;
    ShowWindow(g_monitorRequestsHeading, requestHeaderVisible ? SW_SHOW : SW_HIDE);
    ShowWindow(g_requestToggle, requestHeaderVisible ? SW_SHOW : SW_HIDE);
    Place(g_monitorRequests, monitorPageX, monitorTextY + 30,
          monitorPageWidth, std::max(MonitorRequestCardHeight, (int)requestRows * MonitorRequestCardHeight));
    if (g_monitorRequests)
    {
        RECT requestClient{};
        GetClientRect(g_monitorRequests, &requestClient);
        const int requestWidth = std::max(1, (int)requestClient.right - 18);
        ListView_SetColumnWidth(g_monitorRequests, 0, requestWidth);
        ShowScrollBar(g_monitorRequests, SB_BOTH, FALSE);
    }
    Place(g_reportButton, monitorPageX, monitorActionsY, 170, 32);
    const bool gpuFold = gpuRows > 1 && !MonitorGpuRows(g_metrics).front().value("plain", false);
    const int gpuLabelWidth = std::max(156, monitorPageWidth - 136);
    const auto gpuRowsData = MonitorGpuRows(g_metrics);
    const int monitorGpuMapY = MonitorGpuMapTop(monitorPageTop, monitorPageWidth);
    for (int i = 0; i < 16; ++i)
    {
        if (!g_monitorGpuButtons[i]) continue;
        if (g_page != MONITOR || !gpuFold || (size_t)i >= gpuRows)
        {
            ShowWindow(g_monitorGpuButtons[i], SW_HIDE);
            continue;
        }
        const auto &gpu = gpuRowsData[i];
        const int index = gpu.value("index", gpu.value("dev", i));
        std::wstring label = L"GPU " + std::to_wstring(index);
        const auto name = MonitorText(gpu, "name", L"");
        if (!name.empty()) label += L" · " + name;
        label += (i == g_monitorGpuOpen ? L"  ·  Collapse details" : L"  ·  Enter for details");
        SetWindowTextW(g_monitorGpuButtons[i], label.c_str());
        const int rowY = monitorGpuMapY + 39 + i * MonitorGpuRowHeight +
                         (g_monitorGpuOpen >= 0 && i > g_monitorGpuOpen ? 46 : 0);
        MoveWindow(g_monitorGpuButtons[i], monitorPageX + 14, rowY,
                   gpuLabelWidth - 20, 15, !g_deferLayoutRedraw);
        ShowWindow(g_monitorGpuButtons[i], SW_SHOW);
    }
    const int modelButtonY = bodyY + bodyH - 42;
    if (g_page == MODELS && g_modelSheetOpen)
    {
        Place(g_modelSheetHeading, x, bodyY, bodyW, 30);
        const int sheetGap = 8, sheetTabWidth = (bodyW - sheetGap * 2) / 3;
        for (int i = 0; i < 3; ++i)
            Place(g_modelSheetTabs[i], x + i * (sheetTabWidth + sheetGap), bodyY + 38, sheetTabWidth, 32);
        Place(g_modelSheetContent, x, bodyY + 80, bodyW, std::max(100, bodyH - 144));
        const int actionY = bodyY + bodyH - 46;
        Place(g_modelSheetClose, x + bodyW - 128, actionY, 128, 36);
        if (g_modelSheetTab == 1)
            Place(g_modelSheetEditEngine, x + bodyW - 340, actionY, 204, 36);
        if (g_modelSheetTab == 2)
            Place(g_modelDelete, x + bodyW - 332, actionY, 196, 36);
    }
    else
    {
        const bool showCatalog = ModelCatalogVisible(g_modelsAddOpen, g_pullActive);
        const int modelScroll = showCatalog ? g_catalogScrollY : 0;
        const int contentButtonY = showCatalog ? modelButtonsRawY - modelScroll : modelButtonY;
        Place(g_modelAddButton, x + bodyW - 158, bodyY - modelScroll, 148, 28);
        Place(g_modelsHeading, x, bodyY + 30 - modelScroll, showCatalog ? bodyW / 2 - 10 : bodyW, 24);
        if (showCatalog)
        {
            Place(g_catalogHeading, x + bodyW / 2 + 10, bodyY + 30 - modelScroll, bodyW / 2 - 210, 24);
            Place(g_modelSetupCommand, x + bodyW - 190, bodyY + 30 - modelScroll, 180, 24);
            Place(g_models, x, bodyY + 56 - modelScroll, bodyW / 2 - 10, installedListHeight);
            Place(g_catalogDescription, x + bodyW / 2 + 10, bodyY + 56 - modelScroll, bodyW / 2 - 10, 40);
            Place(g_catalog, x + bodyW / 2 + 10, bodyY + 100 - modelScroll,
                  bodyW / 2 - 10, catalogListHeight);
        }
        else
            Place(g_models, x, bodyY + 56, bodyW, bodyH - 138);
        Place(g_pullProgress, x + bodyW / 2 + 10,
              showCatalog ? contentButtonY - 24 : bodyY + bodyH - 74, bodyW / 2 - 10, 12);
        std::vector<HWND> modelButtons{g_modelAction, g_modelDelete};
        if (g_modelsAddOpen) modelButtons.push_back(g_modelDownload);
        if (g_pullActive) modelButtons.push_back(g_modelCancel);
        modelButtons.insert(modelButtons.end(), {g_modelRefresh, g_modelLoad, g_modelSettingsButton, g_leftoverOpenButton});
        const int modelGap = 8, modelButtonW = std::max(64, (bodyW - modelGap * (int)(modelButtons.size() - 1)) / (int)modelButtons.size());
        for (size_t i = 0; i < modelButtons.size(); ++i)
            Place(modelButtons[i], x + (int)i * (modelButtonW + modelGap), contentButtonY, modelButtonW, 34);
    }
    const bool showSettingsEnvButton = g_modelSettingsAdvanced && !g_modelSettingsEnvironmentOpen;
    const int settingsModeWidth = std::min(176, std::max(1, bodyW / 2));
    const int settingsEnvWidth = std::min(146, std::max(1, bodyW / 4));
    const int settingsResetWidth = std::min(184, std::max(1, bodyW - settingsModeWidth - 8 -
                                                           (showSettingsEnvButton ? settingsEnvWidth + 8 : 0)));
    const int settingsControlsStart = x + bodyW - settingsModeWidth - settingsResetWidth - 8 -
                                      (showSettingsEnvButton ? settingsEnvWidth + 8 : 0);
    Place(g_modelSettingsModeButton, x + bodyW - settingsModeWidth, bodyY + 2, settingsModeWidth, 30);
    if (showSettingsEnvButton)
        Place(g_modelSettingsEnvButton, settingsControlsStart + settingsResetWidth + 8, bodyY + 2, settingsEnvWidth, 30);
    Place(g_modelSettingsResetButton, settingsControlsStart, bodyY + 2, settingsResetWidth, 30);
    Place(g_modelSettingsHint, x, bodyY + 5, std::max(0, settingsControlsStart - x - 8), 24);
    const int tabGap = 6, tabWidth = (bodyW - tabGap * 3) / 4;
    for (int i = 0; i < 4; ++i) Place(g_modelSettingsTabs[i], x + i * (tabWidth + tabGap), bodyY + 36, tabWidth, 28);
    Place(g_modelSettingsWarning, x, bodyY + 66, bodyW, 52);
    const int settingsEditorY = bodyY + 122;
    const int settingsEditorHeight = std::max(90, bodyH - 324);
    Place(g_modelSettingsEdit, x, settingsEditorY, bodyW, settingsEditorHeight);
    Place(g_modelSettingsEnvInfo, x, settingsEditorY, bodyW, 22);
    const int envContentY = settingsEditorY + 28;
    const int envContentHeight = std::max(62, settingsEditorHeight - 28);
    const int envListWidth = bodyW * 56 / 100;
    Place(g_modelSettingsEnvList, x, envContentY, envListWidth, envContentHeight);
    const int envFormX = x + envListWidth + 14;
    const int envFormWidth = std::max(1, bodyW - envListWidth - 14);
    Place(g_modelSettingsEnvNameLabel, envFormX, envContentY, envFormWidth, 18);
    Place(g_modelSettingsEnvName, envFormX, envContentY + 20, envFormWidth, 30);
    Place(g_modelSettingsEnvValueLabel, envFormX, envContentY + 56, envFormWidth, 18);
    const int envActionY = envContentY + envContentHeight - 34;
    const int envValueHeight = std::max(24, envActionY - (envContentY + 78) - 8);
    Place(g_modelSettingsEnvValue, envFormX, envContentY + 78, envFormWidth, envValueHeight);
    const int envAddWidth = std::min(140, std::max(1, (envFormWidth - 8) / 2));
    Place(g_modelSettingsEnvAdd, envFormX, envActionY, envAddWidth, 34);
    Place(g_modelSettingsEnvRemove, envFormX + envAddWidth + 8, envActionY,
          std::max(1, envFormWidth - envAddWidth - 8), 34);
    Place(g_modelSettingsPreview, x, bodyY + bodyH - 194, bodyW, 142);
    Place(g_modelSettingsPreviewButton, x, modelButtonY, 144, 34);
    Place(g_modelSettingsSave, x + 156, modelButtonY, 144, 34);
    Place(g_modelSettingsCancel, x + 312, modelButtonY, 120, 34);
    const int fieldCell = bodyW / 2;
    int fieldRow = 0, fieldColumn = 0;
    for (size_t i = 0; i < g_modelSettingFields.size(); ++i)
    {
        const auto &field = g_modelSettingFields[i];
        if (field.kind == ModelSettingField::GPU_LIST)
        {
            if (fieldColumn) { ++fieldRow; fieldColumn = 0; }
            const int topField = bodyY + 128 + fieldRow * 42;
            Place(field.label, x, topField + 5, bodyW, 20);
            Place(field.control, x, topField + 24, bodyW, 64);
            fieldRow += 2;
            continue;
        }
        const int left = x + fieldColumn * fieldCell, topField = bodyY + 128 + fieldRow * 42;
        Place(field.label, left, topField + 5, fieldCell * 44 / 100 - 10, 28);
        Place(field.control, left + fieldCell * 44 / 100, topField, fieldCell * 56 / 100 - 6,
              field.kind == ModelSettingField::BOOLEAN ? 28 : 32);
        if (++fieldColumn == 2) { fieldColumn = 0; ++fieldRow; }
    }
    const int leftoversSplit = bodyW * 42 / 100;
    Place(g_leftoverList, x, bodyY, leftoversSplit, bodyH - 52);
    Place(g_leftoverDetails, x + leftoversSplit + 16, bodyY,
          bodyW - leftoversSplit - 16, bodyH - 52);
    Place(g_leftoverDelete, x + leftoversSplit + 16, modelButtonY, 144, 34);
    Place(g_leftoverClose, x + leftoversSplit + 172, modelButtonY, 144, 34);
    const int settingsLeftWidth = std::max(1, bodyW * 54 / 100);
    Place(g_settings, x + 18, bodyY + 4, std::max(0, settingsLeftWidth - 36), 22);
    const int settingFieldX = x + 280;
    const int settingFieldWidth = std::min(360, std::max(180, settingsLeftWidth - 324));
    const int settingRowsY = bodyY + 28;
    const HWND settingLabels[] = {g_themeLabel, g_schemeLabel, g_contextLabel, g_portLabel, g_budgetLabel};
    const int settingRowGap = 42;
    for (int i = 0; i < 5; ++i)
        Place(settingLabels[i], x + 24, settingRowsY + i * settingRowGap + 3, 232, 28);
    Place(g_themeBox, settingFieldX, settingRowsY, settingFieldWidth, 34);
    Place(g_schemeBox, settingFieldX, settingRowsY + settingRowGap, settingFieldWidth, 34);
    Place(g_contextEdit, settingFieldX, settingRowsY + settingRowGap * 2, settingFieldWidth, 34);
    Place(g_portEdit, settingFieldX, settingRowsY + settingRowGap * 3, settingFieldWidth, 34);
    const int budgetTop = settingRowsY + settingRowGap * 4;
    const int budgetSaveWidth = 92;
    Place(g_budgetEdit, settingFieldX, budgetTop, std::max(80, settingFieldWidth - budgetSaveWidth - 8), 34);
    Place(g_budgetSave, settingFieldX + settingFieldWidth - budgetSaveWidth, budgetTop, budgetSaveWidth, 34);
    Place(g_budgetHint, settingFieldX, bodyY + 238, settingFieldWidth + 100, 22);
    Place(g_settingsSave, x + 24, bodyY + 244, 160, 34);
    Place(g_settingsStatus, x + 194, bodyY + 247, std::max(0, settingsLeftWidth - 218), 30);
    const int mcpX = x + settingsLeftWidth + 14;
    const int mcpWidth = std::max(1, bodyW - settingsLeftWidth - 14);
    Place(g_toolsCheck, mcpX + 14, bodyY + 324, mcpWidth - 28, 32);
    Place(g_mcpDetails, mcpX + 14, bodyY + 360, mcpWidth - 28, 120);

    Place(g_showThinkingCheck, x + 24, bodyY + 340, settingsLeftWidth - 42, 26);
    Place(g_windowsStartupCheck, x + 24, bodyY + 370, settingsLeftWidth - 42, 26);
    Place(g_autoStartMayaCheck, x + 24, bodyY + 400, settingsLeftWidth - 42, 26);
    Place(g_minimizeToTrayCheck, x + 24, bodyY + 430, settingsLeftWidth - 42, 26);
    Place(g_saveWindowsStartup, x + 24, bodyY + 462, 170, 34);
    Place(g_shortcutsButton, x + 210, bodyY + 462, 190, 34);
    Place(g_defaultInstructionsLabel, x + 24, bodyY + 502, std::max(0, bodyW - 282), 22);
    Place(g_defaultInstructions, x + 24, bodyY + 530, bodyW - 48, std::max(70, bodyH - 540));
    const bool wideAbout = bodyW >= 760;
    const int aboutTeamY = bodyY;
    const int aboutTeamHeight = 86;
    Place(g_mayaProjectCredit, x + 18, aboutTeamY + 10, bodyW - 36, 24);
    const int teamLinkGap = 10;
    const int teamLinkWidth = std::max(120, (bodyW - 36 - 2 * teamLinkGap) / 3);
    Place(g_mayaCreatorGithub, x + 18, aboutTeamY + 44, teamLinkWidth, 30);
    Place(g_mayaCreatorHuggingFace, x + 18 + teamLinkWidth + teamLinkGap,
          aboutTeamY + 44, teamLinkWidth, 30);
    Place(g_mayaComaintainerGithub, x + 18 + 2 * (teamLinkWidth + teamLinkGap),
          aboutTeamY + 44, bodyW - 36 - 2 * (teamLinkWidth + teamLinkGap), 30);
    const int aboutRowY = bodyY + aboutTeamHeight + 12;
    const int aboutRowHeight = std::min(320, std::max(272, bodyH - 548));
    const int aboutFactsWidth = wideAbout ? bodyW * 57 / 100 : bodyW;
    const int aboutAuthorX = wideAbout ? x + aboutFactsWidth + 14 : x;
    const int aboutAuthorY = wideAbout ? aboutRowY : aboutRowY + aboutRowHeight + 10;
    const int aboutAuthorWidth = wideAbout ? bodyW - aboutFactsWidth - 14 : bodyW;
    const int aboutAuthorHeight = wideAbout ? aboutRowHeight : 132;
    Place(g_aboutFacts, x + 16, aboutRowY + 54, aboutFactsWidth - 32, aboutRowHeight - 68);
    Place(g_mayaVersionLink, x + 16, aboutRowY + 12, aboutFactsWidth - 32, 30);
    Place(g_authorCredit, aboutAuthorX + 18, aboutAuthorY + 18, aboutAuthorWidth - 36, 44);
    Place(g_authorGithub, aboutAuthorX + 18, aboutAuthorY + 80, aboutAuthorWidth - 36, 34);
    Place(g_authorHuggingFace, aboutAuthorX + 18, aboutAuthorY + 124, aboutAuthorWidth - 36, 34);
    const int aboutRowsBottom = wideAbout ? aboutRowY + aboutRowHeight : aboutAuthorY + aboutAuthorHeight;
    const int updateRow = aboutRowsBottom + 12;
    const int updateSummaryWidth = std::max(120, bodyW - 36);
    int updateSummaryHeight = 38;
    if (!g_updateSummaryText.empty())
    {
        HDC summaryDc = GetDC(g_hwnd);
        if (summaryDc)
        {
            const auto previousFont = SelectObject(summaryDc, g_small ? g_small : g_font);
            RECT measured{0, 0, updateSummaryWidth, 0};
            const auto summary = W(g_updateSummaryText);
            DrawTextW(summaryDc, summary.c_str(), -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
            updateSummaryHeight = std::max(updateSummaryHeight, (int)(measured.bottom - measured.top) + 8);
            if (previousFont) SelectObject(summaryDc, previousFont);
            ReleaseDC(g_hwnd, summaryDc);
        }
    }
    const int changelogHeight = std::min(360, std::max({220, bodyH - (updateRow - bodyY) - 126,
        updateSummaryHeight + 144}));
    g_aboutUpdateCardHeight = changelogHeight;
    const int updateSummaryTop = updateRow + 12;
    const int releaseNotesTop = updateSummaryTop + updateSummaryHeight + 12;
    const int updateButtonsTop = updateRow + changelogHeight - 44;
    const int releaseNotesHeight = std::max(64, updateButtonsTop - releaseNotesTop - 8);
    Place(g_updateSummary, x + 18, updateSummaryTop, updateSummaryWidth, updateSummaryHeight);
    Place(g_releaseNotes, x + 18, releaseNotesTop, bodyW - 36, releaseNotesHeight);
    Place(g_updateNotes, x + 18, updateButtonsTop, 210, 32);
    if (g_updateInstallVisible) Place(g_updateInstall, x + 238, updateButtonsTop, 180, 32);
    const int supportTop = updateRow + changelogHeight + 12;
    Place(g_supportAbout, x + 12, supportTop + 8, std::max(0, bodyW - 24), 32);
    const int reportButtons = supportTop + 68;
    Place(g_aboutReportCopy, x, reportButtons, 220, 34);
    Place(GetDlgItem(g_hwnd, 260), x + 232, reportButtons, 170, 34);
    Place(g_aboutReport, x, reportButtons + 40,
          bodyW, std::max(80, bodyH - (reportButtons - bodyY) - 46));
    Place(g_dataInfo, x, bodyY, bodyW, 180);
    Place(g_clearDataButton, x, bodyY + 196, 260, 38);
    Place(g_saveDefaultInstructions, x + bodyW - 234, bodyY + 498, 210, 30);
    const int pricingTop = bodyY + bodyH - 166;
    Place(g_savings, x, bodyY, bodyW, std::max(180, pricingTop - bodyY - 14));
    const int priceGap = 14;
    const int modelWidth = std::clamp(bodyW * 27 / 100, 280, 440);
    const int priceCell = std::max(1, (bodyW - modelWidth - priceGap * 4) / 4);
    const HWND priceFields[] = {g_priceModels, g_priceInput, g_priceCached, g_priceOutput, g_pricePeak};
    int priceX = x;
    for (int i = 0; i < 5; ++i)
    {
        const int cellWidth = i == 0 ? modelWidth : priceCell;
        Place(g_priceLabels[i], priceX, pricingTop + 28, cellWidth, 20);
        Place(priceFields[i], priceX, pricingTop + 52, cellWidth, 34);
        priceX += cellWidth + priceGap;
    }
    const int pricingButtonY = pricingTop + 92;
    int pricingButtonX = x;
    for (auto h : {g_priceSave, g_priceCancel, g_priceDefault, g_savingsRefresh, g_savingsReset})
    {
        if (!IsWindowVisible(h) && h == g_priceCancel) continue;
        const int buttonWidth = h == g_priceDefault ? 150 : h == g_savingsRefresh ? 142 :
                               h == g_savingsReset ? 156 : 126;
        Place(h, pricingButtonX, pricingButtonY, buttonWidth, 34);
        pricingButtonX += buttonWidth + 10;
    }
    Place(g_samplingPreset, x, bodyY, 190, 34);
    Place(g_samplingEffort, x + 210, bodyY, 210, 34);
    Place(g_samplingStatus, x + 440, bodyY, std::max(120, bodyW - 568), 34);
    Place(g_turboButton, x + bodyW - 120, bodyY, 120, 34);
    if (g_page == CHAT_DEFAULTS)
    {
        const int samplingContextY = bodyY + 44;
        const int samplingHalf = bodyW / 2;
        Place(g_contextLabel, x + 12, samplingContextY + 6, 116, 22);
        Place(g_contextEdit, x + 132, samplingContextY, std::max(0, samplingHalf - 152), 34);
        Place(g_portLabel, x + samplingHalf + 8, samplingContextY + 6, 48, 22);
        Place(g_portEdit, x + samplingHalf + 60, samplingContextY,
              std::max(0, bodyW - samplingHalf - 72), 34);
        Place(g_sharedDefaultsCheck, x + 12, bodyY + 86, std::max(0, bodyW - 24), 28);
        Place(g_sharedDefaultsSummary, x + 12, bodyY + 115, std::max(0, bodyW - 24), 13);
        const int sampleGap = 24, sampleW = std::max(0, (bodyW - 2 * sampleGap) / 3);
        for (int i = 0; i < 9; ++i)
        {
            const int col = i % 3, row = i / 3;
            const int sx = x + col * (sampleW + sampleGap), sy = bodyY + 148 + row * 120;
            Place(g_samplingLabels[i], sx + 12, sy, std::max(0, sampleW - 24), 22);
            Place(g_samplingFields[i], sx + 12, sy + 28, std::max(0, sampleW - 24), 34);
        }
        const int footerLeft = x + 12, footerWidth = std::max(0, bodyW - 24), footerGap = 12;
        const int saveWidth = std::min(150, std::max(0, (footerWidth - 2 * footerGap) / 4));
        const int resetWidth = std::min(210, std::max(0, (footerWidth - saveWidth - 2 * footerGap) / 2));
        const int applyWidth = std::max(0, footerWidth - saveWidth - resetWidth - 2 * footerGap);
        Place(g_settingsStatus, footerLeft, bodyY + bodyH - 78, footerWidth, 22);
        Place(g_samplingSave, footerLeft, bodyY + bodyH - 48, saveWidth, 36);
        Place(g_samplingReset, footerLeft + saveWidth + footerGap, bodyY + bodyH - 48, resetWidth, 36);
        Place(g_settingsSave, footerLeft + saveWidth + resetWidth + 2 * footerGap,
              bodyY + bodyH - 48, applyWidth, 36);
    }
    const int connectCopyWidth = std::min(112, std::max(0, bodyW - 36));
    const int connectCopyX = x + bodyW - 12 - connectCopyWidth;
    for (int i = 0; i < 3; ++i)
    {
        const int cy = bodyY + 42 + i * 80;
        Place(g_connectTitle[i], x + 12, cy, std::max(0, bodyW - 24), 22);
        Place(g_connectValue[i], x + 12, cy + 26, std::max(0, connectCopyX - x - 24), 34);
        Place(g_connectCopy[i], connectCopyX, cy + 26, connectCopyWidth, 34);
    }
    Place(g_connectAuth, x + 12, bodyY + 286, std::max(0, bodyW - 24), 26);
    Place(g_connectInfo, x + 12, bodyY + 316, std::max(0, bodyW - 24), 40);
    Place(g_connectKeyLabel, x + 12, bodyY + 374, std::max(0, bodyW - 24), 20);
    const int keyClearWidth = std::min(112, std::max(0, (bodyW - 36) / 2));
    const int keySaveWidth = std::min(108, std::max(0, bodyW - 36 - keyClearWidth - 12));
    const int keyClearX = x + bodyW - 12 - keyClearWidth;
    const int keySaveX = keyClearX - 12 - keySaveWidth;
    Place(g_connectKey, x + 12, bodyY + 398, std::max(0, keySaveX - x - 24), 34);
    Place(g_connectKeySave, keySaveX, bodyY + 398, keySaveWidth, 34);
    Place(g_connectKeyClear, keyClearX, bodyY + 398, keyClearWidth, 34);
    Place(g_connectExampleLabel, x + 12, bodyY + 456, std::max(0, bodyW - 24), 22);
    const int exampleCopyWidth = std::min(132, std::max(0, bodyW - 36));
    const int exampleCopyX = x + bodyW - 12 - exampleCopyWidth;
    const int exampleSelectWidth = std::min(250, std::max(0, exampleCopyX - x - 24));
    Place(g_connectExamples, x + 12, bodyY + 484, exampleSelectWidth, 34);
    Place(g_connectCopyExample, exampleCopyX, bodyY + 484, exampleCopyWidth, 34);
    Place(g_connectExample, x + 12, bodyY + 528, std::max(0, bodyW - 24), std::max(60, bodyH - 550));
    SetPageControls();
    Place(g_headerLayer, 0, 0, w, 128);
    RaiseHeaderControls();
}

void OpenModelSheet(int index)
{
    if (index < 0 || !g_modelData.contains("models") || !g_modelData["models"].is_array() ||
        index >= (int)g_modelData["models"].size()) return;
    const auto &model = g_modelData["models"][index];
    if (!model.is_object() || !model.contains("file") || !model["file"].is_string()) return;
    const auto file = model["file"].get<std::string>();
    if (file.empty()) return;
    if (g_modelSheetFile != file) g_modelSheetPlanError.clear();
    g_modelSheetFile = file;
    g_modelSel = index;
    g_modelSheetTab = 0;
    g_modelSheetOpen = true;
    g_leftoverOpen = false;
    UpdateModelSheetView();
    SetPageControls();
    Layout();
    SetFocus(g_modelSheetTabs[0]);
}

void RequestModelSheetStoragePlan()
{
    if (!g_modelSheetOpen || g_modelSheetTab != 2 || g_modelSheetFile.empty() || g_modelSheetPlanPreview ||
        (g_modelSheetPlanFile == g_modelSheetFile && g_modelSheetPlan.is_object())) return;
    if (g_busy != J_NONE)
    {
        g_modelSheetPlanError = "Maya is processing another request. Re-open Storage when it is idle.";
        UpdateModelSheetView();
        return;
    }
    g_deleteFile = g_modelSheetFile;
    g_deleteThen.clear();
    g_deleteFiles = false;
    g_modelSheetPlanFile.clear();
    g_modelSheetPlan = Json::object();
    g_modelSheetPlanError.clear();
    g_modelSheetPlanPreview = true;
    UpdateModelSheetView();
    BeginJob(J_MODEL_DELETE_PLAN);
}

void UpdateGreeting()
{
    const bool chatPage = g_page == CHAT;
    const bool hasSelectedChat = g_chat >= 0 && g_chat < (int)g_chats.size();
    const bool hasMessages = hasSelectedChat && !g_chats[g_chat].messages.empty();
    const bool empty = NativeChatGreetingVisible(chatPage, hasSelectedChat, hasMessages, g_editMessage >= 0);
    const bool showConversation = chatPage && !empty;
    if (g_transcript) ShowWindow(g_transcript, showConversation ? SW_SHOW : SW_HIDE);
    if (g_greeting) ShowWindow(g_greeting, empty ? SW_SHOW : SW_HIDE);
    if (g_chatModelMenu) ShowWindow(g_chatModelMenu, showConversation ? SW_SHOW : SW_HIDE);
    if (g_chatModelMenu)
        SetText(g_chatModelMenu, W(NativeChatModelButtonName(g_health.value("model", std::string()))));
    if (g_modelSidebar)
        SetText(g_modelSidebar, W(g_health.value("model_label", g_health.value("model", std::string("Maya")))));
    if (g_greetingModel)
    {
        const auto model = g_health.value("model_label", g_health.value("model", std::string()));
        const auto connectionStatus = NativeApiConnectionText(g_apiConnection);
        SetText(g_greetingModel, NativeGreetingModelCard(W(model),
            (unsigned long long)g_health.value("max_context", 0LL), g_health.value("images", false),
            g_online && g_health.value("status", std::string()) == "ok",
            *connectionStatus ? connectionStatus : nullptr));
        ShowWindow(g_greetingModel, empty ? SW_SHOW : SW_HIDE);
    }
    const auto &ideas = NativeChatIdeas(g_turboData.value("on", false));
    for (size_t i = 0; i < ideas.size(); ++i)
        if (g_ideas[i])
        {
            SetText(g_ideas[i], ideas[i].label);
            ShowWindow(g_ideas[i], empty ? SW_SHOW : SW_HIDE);
        }
}

void ToggleCompactController()
{
    if (!g_hwnd || !g_compactToggle) return;
    NativeLayoutTransition transition;
    if (!g_compactMode)
    {
        g_fullWindowPlacement = {sizeof(WINDOWPLACEMENT)};
        g_fullWindowPlacementValid = GetWindowPlacement(g_hwnd, &g_fullWindowPlacement) != FALSE;
        const HMONITOR monitor = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
        ShowWindow(g_hwnd, SW_RESTORE);
        g_compactMode = true;
        LONG_PTR style = GetWindowLongPtrW(g_hwnd, GWL_STYLE);
        if (style & WS_VSCROLL)
        {
            SetWindowLongPtrW(g_hwnd, GWL_STYLE, style & ~((LONG_PTR)WS_VSCROLL));
            SetWindowPos(g_hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        }
        RECT bounds{0, 0, NativeCompactClientWidth, NativeCompactClientHeight};
        const DWORD exStyle = (DWORD)GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        AdjustWindowRectEx(&bounds, (DWORD)GetWindowLongPtrW(g_hwnd, GWL_STYLE), GetMenu(g_hwnd) != nullptr, exStyle);
        const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
        RECT current{};
        GetWindowRect(g_hwnd, &current);
        int x = current.left, y = current.top;
        MONITORINFO info{sizeof(info)};
        if (monitor && GetMonitorInfoW(monitor, &info))
        {
            const int left = (int)info.rcWork.left, top = (int)info.rcWork.top;
            const int right = (int)info.rcWork.right, bottom = (int)info.rcWork.bottom;
            x = std::clamp(x, left, std::max(left, right - width));
            y = std::clamp(y, top, std::max(top, bottom - height));
        }
        SetWindowPos(g_hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        SetText(g_compactToggle, L"Expand");
    }
    else
    {
        g_compactMode = false;
        SetText(g_compactToggle, L"Compact");
        if (g_fullWindowPlacementValid)
            SetWindowPlacement(g_hwnd, &g_fullWindowPlacement);
        g_fullWindowPlacementValid = false;
    }
    SetPageControls();
    Layout();
}

LRESULT CALLBACK NativeButtonHoverProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR subclassId, DWORD_PTR referenceData)
{
    if (message == WM_MOUSEMOVE)
    {
        if (g_hoverButton != hwnd)
        {
            const HWND previous = g_hoverButton;
            g_hoverButton = hwnd;
            if (previous && IsWindow(previous)) InvalidateRect(previous, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tracking);
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    else if (message == WM_MOUSELEAVE)
    {
        if (g_hoverButton == hwnd) g_hoverButton = nullptr;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
    else if (message == WM_NCDESTROY)
    {
        if (g_hoverButton == hwnd) g_hoverButton = nullptr;
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        RemoveWindowSubclass(hwnd, NativeButtonHoverProc, subclassId);
        return result;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

HWND Button(const wchar_t *label, int id)
{
    HWND button = CreateWindowW(L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 1, 1,
                                g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
    if (button)
    {
        AttachMayaAccessibleButton(button);
        SetWindowSubclass(button, NativeButtonHoverProc, 2, 0);
    }
    return button;
}
void InitSidebarTooltips()
{
    g_navTooltip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, TTS_ALWAYSTIP | TTS_NOPREFIX,
                                   CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                   g_hwnd, nullptr, g_inst, nullptr);
    if (!g_navTooltip) return;
    SendMessageW(g_navTooltip, TTM_SETMAXTIPWIDTH, 0, 320);
    auto add = [](HWND tip, HWND target, const wchar_t *text) {
        if (!target) return;
        TOOLINFOW info{};
        info.cbSize = sizeof(info);
        info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = g_hwnd;
        info.uId = (UINT_PTR)target;
        info.lpszText = const_cast<LPWSTR>(text);
        SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&info);
    };
    for (int i = 0; i < 9; ++i) add(g_navTooltip, g_nav[i], SidebarNavLabels[i]);
    add(g_navTooltip, g_sidebarToggle, L"Expand or collapse the navigation");
    add(g_navTooltip, g_modelSidebar, L"DeepSeek V4.1 Flash");
}
void StyleDropdown(HWND combo)
{
    if (!combo) return;
    SendMessageW(combo, CB_SETITEMHEIGHT, 0, 28);
    SetWindowSubclass(combo, ThemedComboProc, 1, 0);
}
HWND Edit(const wchar_t *label, DWORD style, int id = 0)
{
    HWND edit = CreateWindowExW(0, L"EDIT", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 1, 1,
                                g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
    StyleNativeEdit(edit);
    return edit;
}bool ModelSettingIsNumeric(const std::string &key)
{
    return ModelSettingsNumericKey(key);
}
constexpr const wchar_t *ModelSettingGroupNames[] = {L"Model", L"GPUs and memory", L"Prompts and cache", L"Speed"};

std::wstring ModelSettingLabel(const std::string &key)
{
    static const std::map<std::string, std::wstring> labels = {
        {"ablate", L"Refusal ablation"}, {"context", L"Context size (tokens)"}, {"parallel", L"Answers at once"},
        {"reserve_mib", L"VRAM kept free (MiB)"}, {"reserve_gb", L"VRAM kept free (GB)"},
        {"expert_cache", L"Expert cache"}, {"prefill", L"Prompt chunk"},
        {"prefill_chunk", L"Prompt chunk limit"}, {"kv_resident", L"KV positions in VRAM"},
        {"kv_streaming", L"KV streaming"}, {"kv_cache", L"KV cache format"}, {"kv", L"KV cache format"},
        {"tp", L"Tensor parallel"}, {"cpu_threads", L"CPU threads"}, {"images", L"Images"},
        {"gpus", L"GPUs (select all that apply)"}, {"draft", L"Drafting"},
        {"draft_conf", L"Draft confidence"}, {"dspark_conf", L"Draft confidence"},
        {"prompt_lookup", L"Prompt lookup"}, {"fast_prefill", L"Fast prompt reading"},
        {"mmap", L"Memory map"}, {"nmap_experts", L"N-map experts"},
        {"ram_gb", L"Experts in RAM (GB)"}};
    const auto found = labels.find(key);
    if (found != labels.end()) return found->second;
    auto label = W(key);
    std::replace(label.begin(), label.end(), L'_', L' ');
    if (!label.empty()) label[0] = (wchar_t)std::towupper(label[0]);
    return label;
}
void DestroyModelSettingsFields()
{
    for (const auto &field : g_modelSettingFields)
    {
        if (field.label) DestroyWindow(field.label);
        if (field.control) DestroyWindow(field.control);
    }
    g_modelSettingFields.clear();
}
void PopulateModelContextChoices(Json &choices, const Json &settings, const Json &range)
{
    if (!choices.is_object()) return;
    choices.erase("context");
    if (!settings.is_object() || !settings.contains("context") ||
        !settings["context"].is_number_integer()) return;
    int min = 4096, max = 262144;
    if (range.is_object() && range.value("min", Json()).is_number_integer() &&
        range.value("max", Json()).is_number_integer())
    {
        min = range["min"].get<int>();
        max = range["max"].get<int>();
    }
    const auto stops = NativeContextStops(min, max, {settings["context"].get<int>()});
    choices["context"] = Json::array();
    for (const int stop : stops) choices["context"].push_back(stop);
}
void BuildModelSettingsFields(const Json &settings, const Json &choices, const Json &availableGpus)
{
    DestroyModelSettingsFields();
    if (!settings.is_object()) return;
    Json fields = settings;
    if (NativeModelAblationAvailable(choices) && !fields.contains("ablate")) fields["ablate"] = false;
    if (!NativeModelAblationVisible(settings, choices)) fields.erase("ablate");
    std::vector<std::pair<int, std::wstring>> gpuOptions;
    if (availableGpus.is_array())
        for (const auto &gpu : availableGpus)
            if (gpu.is_object() && gpu.contains("index") && gpu["index"].is_number_integer())
            {
                const int index = gpu["index"].get<int>();
                if (index >= 0)
                {
                    const auto name = gpu.contains("name") && gpu["name"].is_string()
                                          ? W(gpu["name"].get<std::string>()) : L"on this PC";
                    gpuOptions.emplace_back(index, L"GPU " + std::to_wstring(index) + L" · " + name);
                }
            }
    std::sort(gpuOptions.begin(), gpuOptions.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
    int id = 210;
    for (auto it = fields.begin(); it != fields.end() && id <= 259; ++it)
    {
        const auto &key = it.key();
        const auto &value = it.value();
        ModelSettingField::Kind kind;
        if (key == "gpus" && value.is_array() && !gpuOptions.empty()) kind = ModelSettingField::GPU_LIST;
        else if (value.is_boolean()) kind = ModelSettingField::BOOLEAN;
        else if (key == "context" && choices.is_object() && choices.contains(key) &&
                 choices[key].is_array() && !choices[key].empty()) kind = ModelSettingField::CHOICE;
        else if (value.is_number() || (value.is_null() && ModelSettingIsNumeric(key))) kind = ModelSettingField::NUMBER;
        else if (value.is_string())
        {
            const auto options = choices.is_object() ? choices.value(key, Json()) : Json();
            kind = options.is_array() && !options.empty() ? ModelSettingField::CHOICE : ModelSettingField::STRING;
        }
        else continue;

        const auto label = ModelSettingLabel(key);
        HWND labelControl = CreateWindowW(L"STATIC", label.c_str(), WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                          g_hwnd, nullptr, g_inst, nullptr);
        HWND control = nullptr;
        if (kind == ModelSettingField::BOOLEAN)
        {
            control = CreateWindowW(L"BUTTON", L"Enabled", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                    0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
            ApplyCheckboxTheme(control);
            SendMessageW(control, BM_SETCHECK, value.get<bool>() ? BST_CHECKED : BST_UNCHECKED, 0);
            if (key == "images")
            {
                const bool imageAvailable = choices.is_object() && choices.contains("images") &&
                                            choices["images"].is_boolean() && choices["images"].get<bool>();
                if (!NativeModelImageToggleEnabled(imageAvailable, value.get<bool>())) EnableWindow(control, FALSE);
            }
        }
        else if (kind == ModelSettingField::GPU_LIST)
        {
            control = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                LBS_MULTIPLESEL | LBS_HASSTRINGS | LBS_NOINTEGRALHEIGHT, 0, 0, 1, 1,
                g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
            auto options = gpuOptions;
            for (const auto &selected : value)
                if (selected.is_number_integer())
                {
                    const int index = selected.get<int>();
                    if (index >= 0 && std::none_of(options.begin(), options.end(),
                                                   [&](const auto &option) { return option.first == index; }))
                        options.emplace_back(index, L"GPU " + std::to_wstring(index) + L" · not detected");
                }
            std::sort(options.begin(), options.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
            for (const auto &[index, gpuLabel] : options)
            {
                const LRESULT row = SendMessageW(control, LB_ADDSTRING, 0, (LPARAM)gpuLabel.c_str());
                if (row == LB_ERR || row == LB_ERRSPACE) continue;
                SendMessageW(control, LB_SETITEMDATA, row, index);
                if (std::find(value.begin(), value.end(), index) != value.end())
                    SendMessageW(control, LB_SETSEL, TRUE, row);
            }
        }
        else if (kind == ModelSettingField::CHOICE)
        {
            control = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
                                    0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
            StyleDropdown(control);
            int selected = CB_ERR;
            for (const auto &option : choices[key])
            {
                const auto text = W(ModelSettingsChoiceLabel(key, option));
                const LRESULT index = SendMessageW(control, CB_ADDSTRING, 0, (LPARAM)text.c_str());
                if (option == value) selected = (int)index;
            }
            if (selected != CB_ERR) SendMessageW(control, CB_SETCURSEL, selected, 0);
        }
        else
        {
            const auto text = value.is_null() ? std::wstring() : W(value.is_string() ? value.get<std::string>() : value.dump());
            control = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
                WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 1, 1,
                g_hwnd, (HMENU)(INT_PTR)id, g_inst, nullptr);
        }
        SendMessageW(labelControl, WM_SETFONT, (WPARAM)g_small, TRUE);
        SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
        g_modelSettingFields.push_back({kind, key, (int)ModelSettingsGroupForKey(key), id, labelControl, control});
        ++id;
    }
}
void ShowModelSettingsFields(bool show)
{
    for (const auto &field : g_modelSettingFields)
    {
        const bool visible = show && field.group == g_modelSettingsTab;
        ShowWindow(field.label, visible ? SW_SHOW : SW_HIDE);
        ShowWindow(field.control, visible ? SW_SHOW : SW_HIDE);
    }
}
void SelectModelSettingsTab(int tab)
{
    g_modelSettingsTab = std::clamp(tab, 0, 3);
    UpdateModelSettingsTabs();
    ShowModelSettingsFields(true);
}
std::wstring TrimModelEnvironmentKey(std::wstring key)
{
    const auto first = key.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    const auto last = key.find_last_not_of(L" \t\r\n");
    return key.substr(first, last - first + 1);
}
void RefreshModelSettingsEnvironmentList(int selected = -1)
{
    SendMessageW(g_modelSettingsEnvList, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g_modelSettingsEnvironmentRows.size(); ++i)
    {
        const auto &row = g_modelSettingsEnvironmentRows[i];
        const auto display = row.first + L" = " + row.second;
        SendMessageW(g_modelSettingsEnvList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
    }
    if (selected < 0 || selected >= (int)g_modelSettingsEnvironmentRows.size()) selected = LB_ERR;
    SendMessageW(g_modelSettingsEnvList, LB_SETCURSEL, selected, 0);
    if (selected != LB_ERR)
    {
        SetText(g_modelSettingsEnvName, g_modelSettingsEnvironmentRows[selected].first);
        SetText(g_modelSettingsEnvValue, g_modelSettingsEnvironmentRows[selected].second);
        SetText(g_modelSettingsEnvAdd, L"Update variable");
        EnableWindow(g_modelSettingsEnvRemove, TRUE);
    }
    else
    {
        SetText(g_modelSettingsEnvName, L"");
        SetText(g_modelSettingsEnvValue, L"");
        SetText(g_modelSettingsEnvAdd, L"Add variable");
        EnableWindow(g_modelSettingsEnvRemove, FALSE);
    }
}
void LoadModelSettingsEnvironmentRows(const Json &settings)
{
    g_modelSettingsEnvironmentRows.clear();
    for (const auto &[key, value] : ModelSettingsEnvironmentRows(settings))
        g_modelSettingsEnvironmentRows.emplace_back(W(key), W(value));
    RefreshModelSettingsEnvironmentList();
}
bool CommitModelSettingsEnvironmentRows(const std::vector<std::pair<std::wstring, std::wstring>> &rows)
{
    auto settings = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
    if (settings.is_discarded() || !settings.is_object())
    {
        SetText(g_status, L"Settings must remain a JSON object before editing environment variables.");
        return false;
    }
    std::vector<std::pair<std::string, std::string>> values;
    values.reserve(rows.size());
    for (const auto &[rawKey, value] : rows)
    {
        const auto key = TrimModelEnvironmentKey(rawKey);
        if (!key.empty()) values.emplace_back(U(key), U(value));
    }
    settings["env"] = ModelSettingsEnvironmentObject(values);
    SetText(g_modelSettingsEdit, W(settings.dump(2)));
    return true;
}
void SelectModelSettingsEnvironmentRow()
{
    const int selected = (int)SendMessageW(g_modelSettingsEnvList, LB_GETCURSEL, 0, 0);
    if (selected >= 0 && selected < (int)g_modelSettingsEnvironmentRows.size())
        RefreshModelSettingsEnvironmentList(selected);
}
void EnableModelSettingsInputs(bool enabled)
{
    EnableWindow(g_modelSettingsEdit, enabled);
    EnableWindow(g_modelSettingsEnvButton, enabled);
    EnableWindow(g_modelSettingsEnvList, enabled);
    EnableWindow(g_modelSettingsEnvName, enabled);
    EnableWindow(g_modelSettingsEnvValue, enabled);
    EnableWindow(g_modelSettingsEnvAdd, enabled);
    const int selected = (int)SendMessageW(g_modelSettingsEnvList, LB_GETCURSEL, 0, 0);
    EnableWindow(g_modelSettingsEnvRemove, enabled && selected != LB_ERR);
    EnableWindow(g_modelSettingsResetButton, NativeModelSettingsResetEnabled(g_modelSettingsSetup.is_object(), !enabled));
    for (const auto &field : g_modelSettingFields)
    {
        const bool disabledAblation = field.key == "ablate" && !NativeModelAblationAvailable(g_modelSettingsChoices) &&
            SendMessageW(field.control, BM_GETCHECK, 0, 0) != BST_CHECKED;
        EnableWindow(field.control, enabled && !disabledAblation);
    }
}
void UpdateModelSettingsContextHint(const Json &draft)
{
    if (g_modelSel < 0 || !g_modelData.contains("models") || g_modelSel >= (int)g_modelData["models"].size()) return;
    const auto &model = g_modelData["models"][g_modelSel];
    SetText(g_modelSettingsHint, ModelSettingsContextHint(model.value("settings", Json::object()), draft,
                                                           g_modelLiveContext,
                                                           model.value("running", false),
                                                           model.value("family", std::string())));
}
bool ReadModelSettingsFields(Json &settings, std::array<std::vector<std::wstring>, 4> *warnings = nullptr)
{
    settings = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
    if (settings.is_discarded() || !settings.is_object())
    {
        if (warnings) for (auto &group : *warnings) group.push_back(L"Settings must be a JSON object.");
        return false;
    }
    bool valid = true;
    for (const auto &field : g_modelSettingFields)
    {
        if (field.kind == ModelSettingField::GPU_LIST)
        {
            std::vector<int> indexes;
            const LRESULT count = SendMessageW(field.control, LB_GETCOUNT, 0, 0);
            for (LRESULT row = 0; row < count; ++row)
                if (SendMessageW(field.control, LB_GETSEL, row, 0) > 0)
                {
                    const LRESULT index = SendMessageW(field.control, LB_GETITEMDATA, row, 0);
                    if (index < 0 || index > INT_MAX)
                    {
                        if (warnings) AppendModelSettingsWarning(*warnings, field.group, ModelSettingLabel(field.key),
                                                                 L"At least one GPU must be selected.");
                        valid = false;
                        continue;
                    }
                    indexes.push_back((int)index);
                }
            if (!ModelSettingsSetGpuIndexes(settings, field.key, std::move(indexes)))
            {
                if (warnings) AppendModelSettingsWarning(*warnings, field.group, ModelSettingLabel(field.key),
                                                         L"At least one GPU must be selected.");
                valid = false;
            }
            continue;
        }
        std::string text;
        if (field.kind == ModelSettingField::BOOLEAN)
            text = SendMessageW(field.control, BM_GETCHECK, 0, 0) == BST_CHECKED ? "true" : "false";
        else if (field.kind == ModelSettingField::CHOICE)
        {
            const int selected = (int)SendMessageW(field.control, CB_GETCURSEL, 0, 0);
            const auto options = g_modelSettingsChoices.value(field.key, Json());
            if (selected == CB_ERR || !options.is_array() || selected >= (int)options.size())
            {
                if (warnings) AppendModelSettingsWarning(*warnings, field.group, ModelSettingLabel(field.key),
                                                         L"Choose a supported value.");
                valid = false;
                continue;
            }
            const auto &option = options[selected];
            text = option.is_string() ? option.get<std::string>() : option.dump();
        }
        else
            text = ControlText(field.control);
        if (field.key == "ablate" && text == "false")
        {
            bool savedAblation = false;
            for (const auto &model : g_modelData.value("models", Json::array()))
                if (model.value("file", std::string()) == g_modelSettingsFile)
                {
                    const auto saved = model.value("settings", Json::object());
                    savedAblation = saved.is_object() && saved.contains("ablate");
                    break;
                }
            if (!savedAblation)
            {
                settings.erase(field.key);
                continue;
            }
        }
        if (field.kind == ModelSettingField::NUMBER &&
            (field.key == "reserve_gb" || field.key == "cache_gb" || field.key == "dspark_conf"))
            std::replace(text.begin(), text.end(), ',', '.');
        if (!ModelSettingsSetScalar(settings, field.key, text))
        {
            if (warnings) AppendModelSettingsWarning(*warnings, field.group, ModelSettingLabel(field.key),
                                                     ModelSettingsInvalidWarning(field.key));
            valid = false;
        }
    }
    return valid;
}
void UpdateModelSettingsTabs()
{
    Json saved = Json::object(), draft = Json::object();
    std::array<std::vector<std::wstring>, 4> warnings{};
    std::array<std::vector<std::wstring>, 4> conditionalWarnings{};
    if (g_modelSel >= 0 && g_modelData.contains("models") && g_modelSel < (int)g_modelData["models"].size())
    {
        const auto &model = g_modelData["models"][g_modelSel];
        saved = model.value("settings", Json::object());
        if (g_modelSettingsOpen && !g_modelSettingsAdvanced)
        {
            ReadModelSettingsFields(draft, &warnings);
            conditionalWarnings = ModelSettingsConditionalWarningMessages(saved, draft,
                model.value("context_range", Json::object()), model.value("family", std::string()));
        }
        else if (g_modelSettingsOpen)
            draft = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
    }
    g_modelSettingsEnvironmentChanged = ModelSettingsEnvironmentChanged(saved, draft);
    std::wstring modeLabel = g_modelSettingsEnvironmentOpen ? L"Back to JSON"
        : g_modelSettingsAdvanced ? L"Show native fields" : L"Show Advanced JSON";
    if (g_modelSettingsEnvironmentChanged) modeLabel += L" — changed";
    SetText(g_modelSettingsModeButton, modeLabel);
    SetText(g_modelSettingsEnvButton, g_modelSettingsEnvironmentChanged
        ? L"Environment variables — changed" : L"Environment variables");
    for (auto &field : g_modelSettingFields)
    {
        const bool changed = ModelSettingsFieldChanged(saved, draft, field.key);
        if (changed != field.changed)
        {
            field.changed = changed;
            InvalidateRect(field.label, nullptr, TRUE);
        }
    }
    for (int i = 0; i < 4; ++i)
    {
        const bool hasWarning = !warnings[i].empty() || !conditionalWarnings[i].empty();
        const bool changed = ModelSettingsGroupChanged(saved, draft, (NativeModelSettingsGroup)i);
        g_modelSettingsTabMarkers[i] = ModelSettingsTabMarker(changed, hasWarning);
        std::wstring label = ModelSettingGroupNames[i];
        if (hasWarning) label += L" — needs a look";
        else if (changed) label += L" — changed";
        SetText(g_modelSettingsTabs[i], label);
    }
    std::wstring activeWarning;
    for (const auto &warning : warnings[g_modelSettingsTab])
    {
        if (!activeWarning.empty()) activeWarning += L"\n";
        activeWarning += warning;
    }
    for (const auto &warning : conditionalWarnings[g_modelSettingsTab])
    {
        if (!activeWarning.empty()) activeWarning += L"\n";
        activeWarning += warning;
    }
    SetText(g_modelSettingsWarning, activeWarning);
    if (g_page == MODELS && g_modelSettingsOpen && !g_modelSettingsAdvanced && !activeWarning.empty())
        ShowWindow(g_modelSettingsWarning, SW_SHOW);
    else ShowWindow(g_modelSettingsWarning, SW_HIDE);
}
bool SyncModelSettingsFields()
{
    Json settings;
    if (!ReadModelSettingsFields(settings)) return false;
    SetText(g_modelSettingsEdit, W(settings.dump(2)));
    g_modelSettingsPreviewValid = false;
    EnableWindow(g_modelSettingsSave, FALSE);
    EnableWindow(g_modelSettingsPreviewButton, TRUE);
    UpdateModelSettingsContextHint(settings);
    UpdateModelSettingsTabs();
    return true;
}
bool CurrentModelSettingsChanged(const Json &draft)
{
    if (!g_modelData.contains("models") || !g_modelData["models"].is_array()) return false;
    for (const auto &model : g_modelData["models"])
        if (model.is_object() && model.value("file", std::string()) == g_modelSettingsFile)
            return ModelSettingsChanged(model.value("settings", Json::object()), draft);
    return false;
}
Json CurrentModelSavedCommand()
{
    if (!g_modelData.contains("models") || !g_modelData["models"].is_array()) return Json();
    for (const auto &model : g_modelData["models"])
        if (model.is_object() && model.value("file", std::string()) == g_modelSettingsFile)
            return model.value("command", Json());
    return Json();
}
void RequestModelSettingsPreview()
{
    const bool idleOrChat = g_busy == J_NONE || g_busy == J_CHAT;
    if (!NativeModelSettingsAutoPreviewReady(g_modelSettingsOpen, true, idleOrChat,
                                             g_modelSettingsRequestInFlight != J_NONE))
        return;
    if (!g_modelSettingsAdvanced && !SyncModelSettingsFields())
    {
        SetText(g_status, L"Correct invalid settings before previewing.");
        return;
    }
    const auto raw = ControlText(g_modelSettingsEdit);
    auto settings = Json::parse(raw, nullptr, false);
    if (settings.is_discarded() || !settings.is_object())
    {
        SetText(g_status, L"Settings must be valid JSON with an object at the top level.");
        return;
    }
    try
    {
        KillTimer(g_hwnd, 11);
        g_modelSettingsRequest = ModelSettingsRequest(g_modelSettingsFile, settings, true);
        g_modelSettingsPreviewText = raw;
        g_modelSettingsPreviewValid = false;
        EnableModelSettingsInputs(false);
        EnableWindow(g_modelSettingsPreviewButton, FALSE);
        EnableWindow(g_modelSettingsModeButton, FALSE);
        EnableWindow(g_modelSettingsCancel, FALSE);
        EnableWindow(g_modelSettingsSave, FALSE);
        SetText(g_modelSettingsPreview, L"Checking settings with Maya… nothing has been saved.");
        BeginJob(J_MODEL_SETTINGS_PREVIEW);
    }
    catch (const std::exception &e) { SetText(g_status, W(e.what())); }
}
void CreateUi()
{
    g_profileBox = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                                 CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS, 0, 0, 1, 1,
                                 g_hwnd, (HMENU)110, g_inst, nullptr);
    SendMessageW(g_profileBox, CB_ADDSTRING, 0, (LPARAM)L"DeepSeek V4.1 Flash");
    SendMessageW(g_profileBox, CB_ADDSTRING, 0, (LPARAM)L"DeepSeek V4.1 Flash Abliterated");
    SendMessageW(g_profileBox, CB_SETITEMHEIGHT, (WPARAM)-1, 34);
    SendMessageW(g_profileBox, CB_SETITEMHEIGHT, 0, 30);
    SetWindowSubclass(g_profileBox, ThemedComboProc, 1, 0);
    SendMessageW(g_profileBox, CB_SETCURSEL, g_profile == L"maya-ds4.json" ? 0 : 1, 0);
    g_nav[0] = Button(L"Chat", 101);
    g_nav[1] = Button(L"Monitor", 102);
    g_nav[2] = Button(L"Models", 103);
    g_nav[3] = Button(L"Settings", 104);
    g_nav[4] = Button(L"Savings", 105);
    g_nav[5] = Button(L"Chat defaults", 106);
    g_nav[6] = Button(L"Connect your tools", 107);
    g_nav[7] = Button(L"About and updates", 108);
    g_nav[8] = Button(L"Data", 109);
    g_sidebarToggle = Button(L"Menu", 263);
    g_modelSidebar = Button(L"Model", NativeModelSidebarId);
    g_savedLine = Button(L"Saved – vs the API", NativeSavingsSidebarId);
    InitSidebarTooltips();
    g_dataInfo = CreateWindowExW(0, L"EDIT", L"",
                                 WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                 0, 0, 1, 1, g_hwnd, (HMENU)262, g_inst, nullptr);
    g_aboutFacts = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                 0, 0, 1, 1, g_hwnd, (HMENU)208, g_inst, nullptr);
    g_aboutReport = CreateWindowExW(0, L"EDIT", L"",
                                    WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                    0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)NativeAboutReportControlId, g_inst, nullptr);
    g_aboutReportCopy = Button(L"Copy troubleshooting report", 210);
    g_updateSummary = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                    0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_updateNotes = Button(L"Full release notes ↗", 211);
    g_updateInstall = Button(L"Install update", 220);
    g_supportAbout = Button(NativeSupportMessage, NativeSupportAboutId);
    g_authorCredit = CreateWindowW(L"STATIC", L"Win-Manager for Maya\r\nCreated by Gligor Daniel Marius",
                                   WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_authorGithub = Button(L"GitHub · @DanielMarius", NativeAuthorGithubId);
    g_authorHuggingFace = Button(L"Hugging Face · @DanielMarius", NativeAuthorHuggingFaceId);
    g_mayaProjectCredit = CreateWindowW(L"STATIC",
        L"Project Maya · Created and led by Peasant Smith · Co-maintained with @needmorevram",
        WS_CHILD | SS_LEFT | SS_NOPREFIX, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_mayaCreatorGithub = Button(L"GitHub · @mw00", NativeMayaCreatorGithubId);
    g_mayaCreatorHuggingFace = Button(L"Hugging Face · Peasant Smith", NativeMayaCreatorHuggingFaceId);
    g_mayaComaintainerGithub = Button(L"GitHub · @needmorevram", NativeMayaComaintainerGithubId);
    g_mayaVersionLink = Button(L"Project Maya release ↗", NativeMayaVersionReleaseId);
    g_releaseNotes = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                   0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    Button(L"Show engine log", 260);
    g_start = Button(L"Start", 111);
    g_stop = Button(L"Stop", 112);
    g_restart = Button(L"Restart", 113);
    g_update = Button(L"Check updates", 114);
    g_compactToggle = Button(L"Compact", NativeCompactToggleId);
    g_status = Button(L"Checking Maya…", NativeStatusCardId);
    g_undoDelete = Button(L"Undo", 266);
    ShowWindow(g_undoDelete, SW_HIDE);
    g_detail = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_NOTIFY, 0, 0, 1, 1,
                             g_hwnd, (HMENU)(INT_PTR)NativeApiAddressId, g_inst, nullptr);
    SetText(g_detail, L"Open Maya · " + HeaderApiAddress() + L" · Checking connection…");
    g_newChat = Button(L"+  New chat", 126);
    g_folderMenu = Button(L"Folders", 128);
    g_editLast = Button(L"Edit last turn", 129);
    g_regenerate = Button(L"Regenerate", 131);
    g_cancelEdit = Button(L"Cancel edit", 132);
    g_answerOptions = Button(L"Answer options", 133);
    g_chatOptions = Button(L"Chat options", NativeChatOptionsId);
    g_chatModelMenu = Button(L"Model menu", NativeChatModelButtonId);
    g_contextMeter = CreateWindowW(L"STATIC", L"Context: —", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst,
                                  nullptr);
    g_jumpLatest = Button(L"Latest ↓", 264);
    ShowWindow(g_jumpLatest, SW_HIDE);
    // Win32 uses the nearest preceding static text as an edit control's accessible name.
    CreateWindowW(L"STATIC", L"Search chats", WS_CHILD, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_chatSearch = Edit(L"", ES_AUTOHSCROLL, 127);
    SendMessageW(g_chatSearch, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search chats");
    CreateWindowW(L"STATIC", L"Chats", WS_CHILD, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_chatList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                               LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 0, 0, 1, 1, g_hwnd, (HMENU)120, g_inst, nullptr);
    SendMessageW(g_chatList, LB_SETITEMHEIGHT, 0, 44);
    g_transcript = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
                                   WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_CLIPCHILDREN | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0,
                                   0, 1, 1, g_hwnd, (HMENU)121, g_inst, nullptr);
    SendMessageW(g_transcript, EM_SETEVENTMASK, 0, ENM_LINK | ENM_SCROLL);
    SendMessageW(g_transcript, EM_SETBKGNDCOLOR, 0, C_CARD);
    SetWindowSubclass(g_transcript, TranscriptSubclassProc, 1, 0);
    g_greeting = CreateWindowW(L"STATIC", L"What are we building today?", WS_CHILD | SS_CENTER, 0, 0, 1, 1,
                               g_hwnd, nullptr, g_inst, nullptr);
    SendMessageW(g_greeting, WM_SETFONT, (WPARAM)g_bold, TRUE);
    g_greetingModel = Button(L"", NativeGreetingModelButtonId);
    for (int i = 0; i < 4; ++i)
        g_ideas[i] = Button(L"", 270 + i);
    CreateWindowW(L"STATIC", L"Ask anything", WS_CHILD, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_input = CreateWindowExW(0, L"EDIT", L"",
                              WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                              0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)0, g_inst, nullptr);
    StyleNativeEdit(g_input);
    SetWindowSubclass(g_input, InputSubclassProc, 1, 0);
    g_send = Button(L"Send", 122);
    g_attach = Button(L"Add +", 123);
    g_attachmentMenu = Button(L"Attachments…", 265);
    g_reasonValue = CreateWindowW(L"STATIC", L"Reasoning:", WS_CHILD | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
                                  0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    SendMessageW(g_reasonValue, WM_SETFONT, (WPARAM)g_small, TRUE);
    g_reason = Button(L"High", NativeChatReasoningControlId);
    g_ablate = CreateWindowW(L"BUTTON", L"Refusal ablation", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                             0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)NativeChatAblationControlId, g_inst, nullptr);
    ApplyChatThinking();
    g_stopAnswer = Button(L"Stop answer", 125);
    g_monitorRequestsHeading = CreateWindowW(L"STATIC", L"Recent requests", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_monitorRequests = CreateWindowExW(0, WC_LISTVIEWW, L"",
        WS_CHILD | WS_CLIPSIBLINGS | WS_TABSTOP | LVS_REPORT | LVS_OWNERDATA | LVS_OWNERDRAWFIXED | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | LVS_NOSCROLL,
        0, 0, 1, 1, g_hwnd, (HMENU)202, g_inst, nullptr);
    if (g_monitorRequests)
    {
        SetWindowSubclass(g_monitorRequests, PageScrollSubclassProc, 1, 0);
        SetWindowTheme(g_monitorRequests, L"DarkMode_Explorer", nullptr);
        ListView_SetExtendedListViewStyle(g_monitorRequests,
            LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        ListView_SetBkColor(g_monitorRequests, C_BG);
        ListView_SetTextBkColor(g_monitorRequests, C_BG);
        ListView_SetTextColor(g_monitorRequests, C_TEXT);
        LVCOLUMNW column{};
        column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
        column.pszText = const_cast<LPWSTR>(L"Request");
        column.cx = 600;
        column.fmt = LVCFMT_LEFT;
        column.iSubItem = 0;
        ListView_InsertColumn(g_monitorRequests, 0, &column);
    }
    for (int i = 0; i < 16; ++i)
    {
        g_monitorGpuButtons[i] = CreateWindowW(L"BUTTON", L"GPU details", WS_CHILD | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)(5000 + i), g_inst, nullptr);
        if (g_monitorGpuButtons[i])
        {
            AttachMayaAccessibleButton(g_monitorGpuButtons[i]);
            SetWindowSubclass(g_monitorGpuButtons[i], NativeButtonHoverProc, 2, 0);
            SendMessageW(g_monitorGpuButtons[i], WM_SETFONT, (WPARAM)g_small, TRUE);
        }
    }
    g_reportButton = Button(L"Copy diagnostics report", 146);
    g_requestToggle = Button(L"Show all requests", 201);
    g_modelsHeading = CreateWindowW(L"STATIC", L"INSTALLED MODELS · Enter or double-click for details", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                    g_hwnd, nullptr, g_inst, nullptr);
    g_models = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                             LBS_OWNERDRAWFIXED | LBS_HASSTRINGS, 0,
                             0, 1, 1, g_hwnd, (HMENU)140, g_inst, nullptr);
    SendMessageW(g_models, LB_SETITEMHEIGHT, 0, 118);
    SetWindowSubclass(g_models, ModelListSubclassProc, 1, 0);
    g_catalogHeading = CreateWindowW(L"STATIC", L"MAYA-SUPPORTED DOWNLOADS",
                                    WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_catalog = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT |
                              LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
                              0, 0, 1, 1, g_hwnd, (HMENU)141, g_inst, nullptr);
    SendMessageW(g_catalog, LB_SETITEMHEIGHT, 0, 88);
    SetWindowSubclass(g_catalog, PageScrollSubclassProc, 2, 0);
    g_catalogDescription = CreateWindowW(L"STATIC",
        L"The setup resumes downloads where they stopped, checks each file's SHA-256, builds the pack, then writes the model config shown in the list.",
        WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelAddButton = Button(L"Add a model", 212);
    g_pullProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | PBS_SMOOTH, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    SendMessageW(g_pullProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    ShowWindow(g_pullProgress, SW_HIDE);
    g_modelAction = Button(L"Switch model", 142);
    g_modelDelete = Button(L"Delete selected", 147);
    g_modelDownload = Button(L"Download selected", 143);
    EnableWindow(g_modelDownload, FALSE);
    g_modelRefresh = Button(L"Refresh models", 144);
    g_modelCancel = Button(L"Cancel download", 145);
    g_modelLoad = Button(L"Load model", 162);
    g_modelSettingsButton = Button(L"Engine settings", 163);
    g_modelSetupCommand = Button(L"Manual setup commands…", NativeModelSetupCommandControlId);
    g_modelSettingsEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL, 0, 0, 1, 1, g_hwnd, (HMENU)164, g_inst, nullptr);
    g_modelSettingsModeButton = Button(L"Show Advanced JSON", 203);
    g_modelSettingsEnvButton = Button(L"Environment variables", 278);
    g_modelSettingsEnvInfo = CreateWindowW(L"STATIC", L"Variables Maya starts the model with.", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSettingsEnvList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 1, 1, g_hwnd, (HMENU)279, g_inst, nullptr);
    g_modelSettingsEnvNameLabel = CreateWindowW(L"STATIC", L"Variable name", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSettingsEnvName = Edit(L"", ES_AUTOHSCROLL, 280);
    SendMessageW(g_modelSettingsEnvName, EM_SETCUEBANNER, TRUE, (LPARAM)L"NAME");
    g_modelSettingsEnvValueLabel = CreateWindowW(L"STATIC", L"Value", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSettingsEnvValue = Edit(L"", ES_AUTOHSCROLL, 281);
    SendMessageW(g_modelSettingsEnvValue, EM_SETCUEBANNER, TRUE, (LPARAM)L"value");
    g_modelSettingsEnvAdd = Button(L"Add variable", 286);
    g_modelSettingsEnvRemove = Button(L"Remove selected", 287);
    for (auto control : {g_modelSettingsEnvInfo, g_modelSettingsEnvList, g_modelSettingsEnvNameLabel,
                         g_modelSettingsEnvValueLabel})
        SendMessageW(control, WM_SETFONT, (WPARAM)g_font, TRUE);
    EnableWindow(g_modelSettingsEnvRemove, FALSE);
    for (int i = 0; i < 4; ++i)
        g_modelSettingsTabs[i] = Button(ModelSettingGroupNames[i], 282 + i);
    g_modelSettingsResetButton = Button(L"Reset to setup values", 277);
    EnableWindow(g_modelSettingsResetButton, FALSE);
    g_modelSettingsHint = CreateWindowW(L"STATIC", L"Common settings include GPU selection. Use Advanced JSON for other arrays and maps.",
        WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSettingsWarning = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSettingsPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Preview the settings to validate them and see the launch command.",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 1, 1, g_hwnd, (HMENU)165, g_inst, nullptr);
    g_modelSettingsPreviewButton = Button(L"Preview settings", 166);
    g_modelSettingsSave = Button(L"Apply settings", 167);
    g_modelSettingsCancel = Button(L"Cancel editing", 168);
    g_leftoverList = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY |
        LBS_NOINTEGRALHEIGHT, 0, 0, 1, 1, g_hwnd, (HMENU)169, g_inst, nullptr);
    g_leftoverDetails = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Select an unused item to inspect it.",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 1, 1, g_hwnd, (HMENU)171, g_inst, nullptr);
    g_leftoverDelete = Button(L"Delete selected item", 172);
    g_leftoverClose = Button(L"Back to models", 173);
    g_leftoverOpenButton = Button(L"Unused files", 170);
    g_modelSheetHeading = CreateWindowW(L"STATIC", L"Model details", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSheetTabs[0] = Button(L"Overview", NativeModelSheetOverviewId);
    g_modelSheetTabs[1] = Button(L"Engine", NativeModelSheetEngineId);
    g_modelSheetTabs[2] = Button(L"Storage", NativeModelSheetStorageId);
    g_modelSheetContent = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_MULTILINE |
        ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_HSCROLL, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_modelSheetEditEngine = Button(L"Edit engine settings…", NativeModelSheetEditEngineId);
    g_modelSheetClose = Button(L"Back to models", NativeModelSheetCloseId);
    SendMessageW(g_modelSheetContent, WM_SETFONT, (WPARAM)g_mono, TRUE);
    g_savings = CreateWindowW(L"STATIC", L"Open Savings to load data.",
                                WS_CHILD | SS_OWNERDRAW, 0, 0, 1, 1,
                                g_hwnd, (HMENU)152, g_inst, nullptr);
    g_savingsRefresh = Button(L"Refresh savings", 153);
    g_savingsReset = Button(L"Start over from $0", 154);
    g_priceModels = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle, 0, 0,
                                  1, 1, g_hwnd, (HMENU)155, g_inst, nullptr);
    StyleDropdown(g_priceModels);
    g_priceInput = Edit(L"", ES_AUTOHSCROLL, 156);
    g_priceCached = Edit(L"", ES_AUTOHSCROLL, 157);
    g_priceOutput = Edit(L"", ES_AUTOHSCROLL, 158);
    g_pricePeak = Edit(L"", ES_AUTOHSCROLL, 159);
    const wchar_t *priceLabelText[] = {L"Model", L"Input · USD / 1M tokens", L"Cached input · USD / 1M tokens",
                                       L"Output · USD / 1M tokens", L"Peak multiplier"};
    for (int i = 0; i < 5; ++i)
        g_priceLabels[i] = CreateWindowW(L"STATIC", priceLabelText[i], WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                         0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    SendMessageW(g_priceInput, EM_SETCUEBANNER, FALSE, (LPARAM)L"Input $ / 1M tokens");
    SendMessageW(g_priceCached, EM_SETCUEBANNER, FALSE, (LPARAM)L"Cached $ / 1M tokens");
    SendMessageW(g_priceOutput, EM_SETCUEBANNER, FALSE, (LPARAM)L"Output $ / 1M tokens");
    SendMessageW(g_pricePeak, EM_SETCUEBANNER, FALSE, (LPARAM)L"Peak multiplier");
    g_priceSave = Button(L"Save prices", 160);
    g_priceDefault = Button(L"Use built-in prices", 161);
    g_priceCancel = Button(L"Cancel", 300);
    g_settings = CreateWindowExW(0, L"STATIC", L"General settings", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr,
                                 g_inst, nullptr);
    g_themeLabel = CreateWindowW(L"STATIC", L"Theme", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_themeBox = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
        0, 0, 1, 1, g_hwnd, (HMENU)193, g_inst, nullptr);
    StyleDropdown(g_themeBox);
    g_schemeLabel = CreateWindowW(L"STATIC", L"Colours", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_schemeBox = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
        0, 0, 1, 1, g_hwnd, (HMENU)194, g_inst, nullptr);
    StyleDropdown(g_schemeBox);
    g_contextLabel = CreateWindowW(L"STATIC", L"Context size", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_contextEdit = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    StyleDropdown(g_contextEdit);
    g_portLabel = CreateWindowW(L"STATIC", L"Port", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_portEdit = Edit(L"", ES_AUTOHSCROLL);
    g_budgetLabel = CreateWindowW(L"STATIC", L"Thinking budget", WS_CHILD | SS_LEFT | SS_NOPREFIX,
                                  0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_budgetEdit = Edit(L"", ES_AUTOHSCROLL, 281);
    g_budgetSave = Button(L"Save", 280);
    g_budgetHint = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_LEFT | SS_NOPREFIX,
        0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_toolsCheck =
        CreateWindowW(L"BUTTON", L"Allow Maya to call configured MCP tools", NativeMcpToolsCheckStyle, 0,
                      0, 1, 1, g_hwnd, (HMENU)(INT_PTR)NativeMcpToolsControlId, g_inst, nullptr);
    g_mcpDetails = CreateWindowExW(0, L"EDIT", L"Checking MCP servers…",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_settingsSave = Button(L"Apply settings", 151);
    g_settingsStatus = CreateWindowW(L"STATIC", L"Context and port changes reload Maya.", WS_CHILD | WS_VISIBLE, 0, 0,
                                     1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_defaultInstructionsLabel = CreateWindowW(L"STATIC", L"Instructions for new chats", WS_CHILD | WS_VISIBLE,
                                                0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_defaultInstructions = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_WANTRETURN, 0, 0, 1, 1, g_hwnd, (HMENU)174, g_inst, nullptr);
    StyleNativeEdit(g_defaultInstructions);
    SendMessageW(g_defaultInstructions, EM_SETLIMITTEXT, 32767, 0);
    g_saveDefaultInstructions = Button(L"Save new-chat instructions", 175);
    g_clearDataButton = Button(L"Clear chats and settings…", 192);
    const wchar_t *connectTitles[] = {L"OpenAI-compatible base URL", L"Anthropic-compatible base URL", L"Model name"};
    for (int i = 0; i < 3; ++i)
    {
        g_connectTitle[i] = CreateWindowW(L"STATIC", connectTitles[i], WS_CHILD | WS_VISIBLE, 0, 0, 1, 1,
                                          g_hwnd, nullptr, g_inst, nullptr);
        g_connectValue[i] = Edit(L"", ES_AUTOHSCROLL | ES_READONLY);
        g_connectCopy[i] = Button(L"Copy", 195 + i);
    }
    g_connectAuth = CreateWindowW(L"STATIC", L"Checking Maya authentication…", WS_CHILD | WS_VISIBLE,
                                  0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_connectInfo = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_connectKeyLabel = CreateWindowW(L"STATIC", L"API key override (optional)", WS_CHILD | WS_VISIBLE,
                                      0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_connectKey = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
                                  0, 0, 1, 1, g_hwnd, (HMENU)206, g_inst, nullptr);
    StyleNativeEdit(g_connectKey);
    SendMessageW(g_connectKey, EM_SETLIMITTEXT, 2048, 0);
    g_connectKeySave = Button(L"Save key", 207);
    g_connectKeyClear = Button(L"Clear override", 214);
    g_connectExampleLabel = CreateWindowW(L"STATIC", L"First request examples (PowerShell)", WS_CHILD | WS_VISIBLE,
                                          0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_connectExamples = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
                                     0, 0, 1, 1, g_hwnd, (HMENU)198, g_inst, nullptr);
    StyleDropdown(g_connectExamples);
    for (const auto *label : {L"Claude Code", L"OpenAI-compatible client", L"curl.exe request"})
        SendMessageW(g_connectExamples, CB_ADDSTRING, 0, (LPARAM)label);
    SendMessageW(g_connectExamples, CB_SETCURSEL, 0, 0);
    g_connectCopyExample = Button(L"Copy example", 199);
    g_connectExample = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_TABSTOP | ES_MULTILINE |
        ES_AUTOVSCROLL | ES_READONLY, 0, 0, 1, 1, g_hwnd, (HMENU)200, g_inst, nullptr);
    StyleNativeEdit(g_connectExample);
    g_showThinkingCheck = CreateWindowW(L"BUTTON", L"Show thinking as it is written",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 1, 1, g_hwnd, (HMENU)176, g_inst, nullptr);
    SendMessageW(g_showThinkingCheck, BM_SETCHECK, BST_CHECKED, 0);
    g_windowsStartupCheck = CreateWindowW(L"BUTTON", L"Open Maya Manager at Windows sign-in",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 1, 1, g_hwnd, (HMENU)215, g_inst, nullptr);
    g_autoStartMayaCheck = CreateWindowW(L"BUTTON", L"Start Maya automatically at sign-in",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 1, 1, g_hwnd, (HMENU)216, g_inst, nullptr);
    g_minimizeToTrayCheck = CreateWindowW(L"BUTTON", L"Keep Maya Manager in the notification area when minimized",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 1, 1, g_hwnd, (HMENU)227, g_inst, nullptr);
    g_saveWindowsStartup = Button(L"Save startup", 217);
    g_shortcutsButton = Button(L"Keyboard shortcuts", 218);
    g_samplingPreset = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
        0, 0, 1, 1, g_hwnd, (HMENU)179, g_inst, nullptr);
    StyleDropdown(g_samplingPreset);
    g_samplingEffort = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_TABSTOP | WS_VSCROLL | NativeThemedDropdownStyle,
        0, 0, 1, 1, g_hwnd, (HMENU)180, g_inst, nullptr);
    StyleDropdown(g_samplingEffort);
    for (size_t i = 0; i < 9; ++i)
    {
        g_samplingLabels[i] = CreateWindowW(L"STATIC", SamplingLabels[i], WS_CHILD | WS_VISIBLE,
            0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
        g_samplingFields[i] = Edit(L"", ES_AUTOHSCROLL, 181 + (int)i);
    }
    g_samplingSave = Button(L"Save defaults", 177);
    g_samplingReset = Button(L"Reset to model defaults", 178);
    g_sharedDefaultsCheck = CreateWindowW(L"BUTTON", L"Use these defaults for other API apps too",
        WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 1, 1, g_hwnd, (HMENU)190, g_inst, nullptr);
    g_sharedDefaultsSummary = CreateWindowW(L"STATIC", L"API clients get them for anything they don't set themselves.",
        WS_CHILD | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_samplingStatus = CreateWindowW(L"STATIC", L"New chats use these per-family settings.",
        WS_CHILD | WS_VISIBLE, 0, 0, 1, 1, g_hwnd, nullptr, g_inst, nullptr);
    g_turboButton = Button(L"Turbo: Check", 267);
    EnableWindow(g_turboButton, FALSE);
    g_font = MakeFont(10);
    g_bold = MakeFont(11, true);
    g_brandFont = MakeFont(16, true);
    g_small = MakeFont(9);
    g_mono = MakeFont(10, false, L"Cascadia Mono");
    if (g_monitorRequests) SendMessageW(g_monitorRequests, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (g_monitorRequestsHeading) SendMessageW(g_monitorRequestsHeading, WM_SETFONT, (WPARAM)g_small, TRUE);
    for (auto h :
         {g_profileBox,  g_nav[0],   g_nav[1],      g_nav[2],        g_nav[3],       g_nav[6],        g_nav[7],        g_nav[8],        g_sidebarToggle,
          g_modelSidebar, g_supportSidebar, g_supportAbout, g_authorCredit, g_authorGithub, g_authorHuggingFace,
          g_mayaProjectCredit, g_mayaCreatorGithub, g_mayaCreatorHuggingFace, g_mayaComaintainerGithub,
          g_mayaVersionLink, g_releaseNotes, g_start, g_stop,
          g_restart,     g_update,   g_status,      g_detail,        g_undoDelete,   g_turboButton,  g_chatList,     g_chatSearch,    g_newChat,
          g_transcript,  g_jumpLatest, g_input,      g_send,        g_attach, g_attachmentMenu, g_reason, g_ablate, g_stopAnswer,
          g_models,      g_catalog, g_modelsHeading, g_catalogHeading, g_catalogDescription, g_modelAddButton, g_modelAction, g_modelDelete, g_modelDownload, g_modelCancel,  g_modelRefresh, g_modelLoad,
          g_modelSettingsButton, g_modelSettingsEdit, g_modelSettingsPreview, g_modelSettingsPreviewButton,
          g_modelSettingsSave, g_modelSettingsCancel, g_modelSettingsModeButton, g_modelSettingsHint, g_modelSettingsResetButton,
          g_modelSettingsTabs[0], g_modelSettingsTabs[1], g_modelSettingsTabs[2], g_modelSettingsTabs[3],
          g_modelSettingsWarning,
          g_leftoverList, g_leftoverDetails, g_leftoverDelete,
          g_leftoverClose, g_leftoverOpenButton, g_settings,
           g_contextLabel, g_contextEdit, g_portLabel, g_portEdit, g_budgetLabel, g_budgetEdit, g_budgetSave, g_budgetHint, g_toolsCheck, g_mcpDetails, g_settingsSave, g_settingsStatus, g_reportButton,
          g_requestToggle, g_aboutFacts, g_aboutReport, g_aboutReportCopy, g_updateSummary, g_updateNotes, g_updateInstall, GetDlgItem(g_hwnd, 260), g_dataInfo,
          g_themeLabel, g_themeBox, g_schemeLabel, g_schemeBox,
          g_defaultInstructionsLabel, g_defaultInstructions, g_saveDefaultInstructions, g_showThinkingCheck,
          g_windowsStartupCheck, g_autoStartMayaCheck, g_minimizeToTrayCheck, g_saveWindowsStartup, g_shortcutsButton, g_clearDataButton,
          g_answerOptions, g_savings, g_savingsRefresh, g_savingsReset, g_nav[4], g_priceModels, g_priceInput,
          g_nav[5], g_samplingPreset, g_samplingEffort, g_samplingSave, g_samplingReset, g_samplingStatus, g_sharedDefaultsCheck,
          g_sharedDefaultsSummary,
          g_connectAuth, g_connectInfo, g_connectKeyLabel, g_connectKey, g_connectKeySave, g_connectKeyClear,
          g_connectCopy[0], g_connectCopy[1], g_connectCopy[2],
          g_connectExampleLabel, g_connectExamples, g_connectExample, g_connectCopyExample,
          g_priceCached, g_priceOutput, g_pricePeak, g_priceSave, g_priceCancel, g_priceDefault,
          g_priceLabels[0], g_priceLabels[1], g_priceLabels[2], g_priceLabels[3], g_priceLabels[4]})
        if (h)
            SendMessageW(h, WM_SETFONT, (WPARAM)(h == g_status || h == g_sharedDefaultsSummary ? g_small : g_font), TRUE);
    for (auto h : g_samplingLabels) SendMessageW(h, WM_SETFONT, (WPARAM)g_small, TRUE);
    for (auto h : g_priceLabels) SendMessageW(h, WM_SETFONT, (WPARAM)g_small, TRUE);
    SendMessageW(g_modelSettingsWarning, WM_SETFONT, (WPARAM)g_small, TRUE);
    SendMessageW(g_authorCredit, WM_SETFONT, (WPARAM)g_bold, TRUE);
    SendMessageW(g_mayaProjectCredit, WM_SETFONT, (WPARAM)g_bold, TRUE);
    SendMessageW(g_releaseNotes, WM_SETFONT, (WPARAM)g_small, TRUE);
    SendMessageW(g_modelsHeading, WM_SETFONT, (WPARAM)g_small, TRUE);
    SendMessageW(g_catalogHeading, WM_SETFONT, (WPARAM)g_small, TRUE);
    SendMessageW(g_catalogDescription, WM_SETFONT, (WPARAM)g_small, TRUE);
    for (auto h : g_samplingFields) SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    for (auto h : g_connectTitle) SendMessageW(h, WM_SETFONT, (WPARAM)g_small, TRUE);
    for (auto h : g_connectValue) SendMessageW(h, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(g_toolsCheck, BM_SETCHECK, BST_CHECKED, 0);
    RefreshCheckboxThemes();
    RefreshChatList();
    SetPageControls();
    g_headerLayer = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW,
                                  0, 0, 1, 1, g_hwnd, (HMENU)(INT_PTR)NativeHeaderLayerId, g_inst, nullptr);
}

void DrawMonitorBanners(HDC dc, int x, int width, const Json &metrics, int scrollY)
{
    const auto banners = MonitorBanners(metrics);
    if (banners.empty())
        return;
    const auto previousFont = g_small ? SelectObject(dc, g_small) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    int top = 138 - scrollY;
    for (const auto &banner : banners)
    {
        RECT measured{0, 0, std::max(1, width - 56), 0};
        DrawTextW(dc, banner.text.c_str(), -1, &measured, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
        const int height = std::max(32, (int)(measured.bottom - measured.top) + 16);
        const RECT frame{x, top, x + width, top + height};
        const auto accent = banner.tone == MonitorBannerTone::info ? C_CYAN
            : banner.tone == MonitorBannerTone::danger ? C_RED : C_WARN;
        auto background = CreateSolidBrush(C_CARD);
        auto outline = CreatePen(PS_SOLID, 1, C_CARD_EDGE);
        const auto oldBackground = SelectObject(dc, background);
        const auto oldOutline = SelectObject(dc, outline);
        RoundRect(dc, frame.left, frame.top, frame.right, frame.bottom, 10, 10);
        SelectObject(dc, oldBackground);
        SelectObject(dc, oldOutline);
        DeleteObject(background);
        DeleteObject(outline);
        auto accentBrush = CreateSolidBrush(accent);
        RECT accentRect{x + 1, top + 7, x + 4, top + height - 7};
        FillRect(dc, &accentRect, accentBrush);
        DeleteObject(accentBrush);
        SetTextColor(dc, accent);
        SelectObject(dc, g_bold);
        RECT glyph{x + 11, top + 5, x + 31, top + height - 5};
        const wchar_t *symbol = banner.tone == MonitorBannerTone::info ? L"i" : L"!";
        DrawTextW(dc, symbol, -1, &glyph, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (g_small) SelectObject(dc, g_small);
        SetTextColor(dc, C_TEXT);
        RECT message{x + 42, top + 6, x + width - 14, top + height - 6};
        DrawTextW(dc, banner.text.c_str(), -1, &message, DT_LEFT | DT_VCENTER | DT_WORDBREAK | DT_NOPREFIX);
        top += height + 8;
    }
    if (previousFont) SelectObject(dc, previousFont);
}

void DrawCard(HDC dc, RECT r, const wchar_t *title, const std::wstring &value, const wchar_t *sub)
{
    auto b = CreateSolidBrush(C_CARD);
    auto p = CreatePen(PS_SOLID, 2, C_CARD_EDGE);
    auto ob = SelectObject(dc, b);
    auto op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, 18, 18);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_SOFT);
    SelectObject(dc, g_bold);
    RECT t{r.left + 18, r.top + 14, r.right - 14, r.top + 35};
    DrawTextW(dc, title, -1, &t, DT_LEFT | DT_SINGLELINE);
    SetTextColor(dc, C_TEXT);
    SelectObject(dc, g_bold);
    RECT v{r.left + 18, r.top + 42, r.right - 14, r.bottom - 30};
    DrawTextW(dc, value.c_str(), -1, &v, DT_LEFT | DT_SINGLELINE);
    SetTextColor(dc, C_MUTED);
    RECT s{r.left + 18, r.bottom - 26, r.right - 14, r.bottom - 8};
    DrawTextW(dc, sub, -1, &s, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
}

void DrawNeonPanel(HDC dc, RECT r, COLORREF edge = C_CYAN, int radius = 16)
{
    HBRUSH brush = CreateSolidBrush(C_CARD);
    HPEN pen = CreatePen(PS_SOLID, 2, edge);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawModelListCard(const DRAWITEMSTRUCT &item, bool catalog)
{
    if (item.itemID == (UINT)-1) return;
    const Json *model = nullptr;
    if (catalog && g_catalogData.is_object() && g_catalogData.contains("models") &&
        g_catalogData["models"].is_array())
    {
        const int modelIndex = item.itemID < g_catalogRowToModel.size()
            ? g_catalogRowToModel[item.itemID] : (int)item.itemID;
        if (modelIndex >= 0 && (size_t)modelIndex < g_catalogData["models"].size())
            model = &g_catalogData["models"][modelIndex];
    }
    if (!catalog && g_modelData.is_object() && g_modelData.contains("models") &&
        g_modelData["models"].is_array() && item.itemID < g_modelData["models"].size())
        model = &g_modelData["models"][item.itemID];

    HDC dc = item.hDC;
    RECT row = item.rcItem;
    HBRUSH background = CreateSolidBrush(C_BG);
    FillRect(dc, &row, background);
    DeleteObject(background);
    RECT card = row;
    InflateRect(&card, -4, -3);
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    DrawNeonPanel(dc, card, selected ? C_CYAN : C_CARD_EDGE, 14);
    if (selected)
    {
        HBRUSH selectedFill = CreateSolidBrush(C_CARD2);
        RECT inset = card;
        InflateRect(&inset, -3, -3);
        HBRUSH old = (HBRUSH)SelectObject(dc, selectedFill);
        RoundRect(dc, inset.left, inset.top, inset.right, inset.bottom, 11, 11);
        SelectObject(dc, old);
        DeleteObject(selectedFill);
    }

    std::string title, quant, source, date, detail;
    bool running = false;
    if (model && model->is_object())
    {
        title = model->value("model", model->value("name", model->value("id", std::string("Model"))));
        quant = model->value("quant", std::string());
        running = model->value("running", false);
        if (catalog)
        {
            detail = model->value("about", model->value("description", model->value("family", std::string())));
            source = model->value("repo", model->value("source", std::string("Maya model catalog")));
            for (const char *key : {"published_at", "released_at", "created_at", "updated_at", "date"})
                if (model->contains(key) && (*model)[key].is_string() && !(*model)[key].get<std::string>().empty())
                { date = (*model)[key].get<std::string>(); break; }
        }
        else
        {
            const char *keys[] = {"repo", "source", "repository", "download_url"};
            for (const auto *key : keys)
                if (model->contains(key) && (*model)[key].is_string() && !(*model)[key].get<std::string>().empty())
                { source = (*model)[key].get<std::string>(); break; }
            if (source.empty())
            {
                const auto folder = ModelDetailsTruthyValue(*model, "folder", "");
                const auto folderName = fs::path(folder).filename().string();
                if (folderName.find("Abliterated-Kiwimaker") != std::string::npos || title.find("Abliterated") != std::string::npos)
                    source = "Hugging Face · Kiwimaker/DeepSeek-V4.1-Flash-1017-Maya-S-abliterated-GGUF · base: peasantsmith/DeepSeek-V4.1-Flash-1017-Maya-GGUF";
                else if (folderName == "Maya-XS" || title.find("Maya-XS") != std::string::npos)
                    source = "Hugging Face · peasantsmith/DeepSeek-V4.1-Flash-1017-Maya-GGUF";
                else
                    source = folder.empty() ? "Origin not recorded" : "Origin not recorded · Local folder · " + folderName;
            }
            date = ModelSetupDate(*model, LOCALE_NAME_USER_DEFAULT);
            detail = ModelCardAbout(*model, LOCALE_NAME_USER_DEFAULT);
        }
    }
    if (title.empty())
    {
        wchar_t text[1024]{};
        SendMessageW(item.hwndItem, LB_GETTEXT, item.itemID, (LPARAM)text);
        title = Utf8(text);
    }

    const bool isDeepseek = title.find("deepseek") != std::string::npos || title.find("DeepSeek") != std::string::npos;
    RECT icon{card.left + 14, card.top + 16, card.left + 58, card.top + 60};
    if (isDeepseek && g_deepseekIcon)
        DrawIconEx(dc, icon.left, icon.top, g_deepseekIcon, 42, 42, 0, nullptr, DI_NORMAL);
    else
    {
        HBRUSH iconBrush = CreateSolidBrush(C_CARD3);
        HPEN iconPen = CreatePen(PS_SOLID, 2, C_CYAN);
        const auto oldBrush = SelectObject(dc, iconBrush);
        const auto oldPen = SelectObject(dc, iconPen);
        RoundRect(dc, icon.left, icon.top, icon.right, icon.bottom, 12, 12);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(iconBrush);
        DeleteObject(iconPen);
        std::wstring initials = title.empty() ? L"M" : W(title.substr(0, 1));
        if (!initials.empty()) initials[0] = (wchar_t)towupper(initials[0]);
        if (title.find("Qwen") != std::string::npos || title.find("qwen") != std::string::npos) initials = L"Q";
        else if (title.find("Llama") != std::string::npos || title.find("llama") != std::string::npos) initials = L"L";
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_CYAN_TEXT);
        SelectObject(dc, g_bold);
        DrawTextW(dc, initials.c_str(), -1, &icon, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    const int textLeft = card.left + 70;
    RECT titleRect{textLeft, card.top + 8, card.right - 104, card.top + 31};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_TEXT);
    SelectObject(dc, g_bold);
    const auto display = W(ModelDisplayName(title));
    DrawTextW(dc, display.c_str(), -1, &titleRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const std::wstring badgeText = catalog ? L"CATALOG" : running ? L"RUNNING" : L"READY";
    const COLORREF badgeColor = running ? C_GREEN_TEXT : catalog ? C_CYAN_TEXT : C_MUTED;
    RECT badge{card.right - 94, card.top + 9, card.right - 12, card.top + 30};
    HBRUSH badgeBrush = CreateSolidBrush(C_CARD2);
    HPEN badgePen = CreatePen(PS_SOLID, 1, running ? C_GREEN : C_CYAN);
    const auto oldBrush = SelectObject(dc, badgeBrush);
    const auto oldPen = SelectObject(dc, badgePen);
    RoundRect(dc, badge.left, badge.top, badge.right, badge.bottom, 11, 11);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(badgeBrush);
    DeleteObject(badgePen);
    SetTextColor(dc, badgeColor);
    SelectObject(dc, g_small);
    DrawTextW(dc, badgeText.c_str(), -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT meta{textLeft, card.top + 34, card.right - 12, card.top + 52};
    std::wstring metadata = quant.empty() ? W(detail) : W(quant);
    SetTextColor(dc, C_CYAN_TEXT);
    DrawTextW(dc, metadata.c_str(), -1, &meta, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    RECT origin{textLeft, card.top + 55, card.right - 12, card.top + 72};
    SetTextColor(dc, C_SOFT);
    std::string sourceLine = source.empty() ? "Maya-supported download" : source;
    if (catalog) sourceLine += date.empty() ? " · catalog date not supplied" : " · " + date;
    const auto sourceText = W(sourceLine);
    DrawTextW(dc, sourceText.c_str(), -1, &origin, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (!catalog && card.bottom - card.top >= 94)
    {
        RECT added{textLeft, card.top + 73, card.right - 12, card.bottom - 7};
        SetTextColor(dc, C_MUTED);
        const auto addedText = W(date.empty() || date == "not set" ? "Setup date not recorded" : "Set up " + date);
        DrawTextW(dc, addedText.c_str(), -1, &added, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    if (item.itemState & ODS_FOCUS) DrawFocusRect(dc, &card);
}

void DrawSavingsCards(const DRAWITEMSTRUCT &item)
{
    HDC dc = item.hDC;
    RECT area = item.rcItem;
    HBRUSH background = CreateSolidBrush(C_BG);
    FillRect(dc, &area, background);
    DeleteObject(background);
    const auto models = g_savingsData.is_object() && g_savingsData.contains("models") && g_savingsData["models"].is_array()
        ? g_savingsData["models"] : Json::array();
    double tokens = 0, requests = 0;
    for (const auto &model : models)
        if (model.is_object())
        {
            const auto values = model.value("tokens", Json::object());
            for (const char *key : {"input", "output"})
                if (values.is_object() && values.contains(key) && values[key].is_number()) tokens += values[key].get<double>();
            if (values.is_object() && values.contains("requests") && values["requests"].is_number())
                requests += values["requests"].get<double>();
        }
    const int margin = 5, gap = 12, summaryHeight = 112;
    const int width = std::max(1, (int)(area.right - area.left) - 2 * margin);
    const int summaryWidth = std::max(1, (width - 2 * gap) / 3);
    const std::wstring since = g_savingsData.value("since", Json()).is_string()
        ? W(g_savingsData.value("since", std::string())) : L"Across recorded Maya use";
    const std::wstring values[] = {W(SavingsMoney(g_savingsData.value("usd", Json()))),
                                   W(SavingsTokens(Json(tokens))), W(SavingsTokens(Json(requests)))};
    const wchar_t *labels[] = {L"SAVED VS API", L"TOKENS", L"REQUESTS"};
    for (int i = 0; i < 3; ++i)
    {
        RECT card{area.left + margin + i * (summaryWidth + gap), area.top + margin,
                  i == 2 ? area.right - margin : area.left + margin + (i + 1) * summaryWidth + i * gap,
                  area.top + margin + summaryHeight};
        DrawNeonPanel(dc, card, i == 0 ? C_CYAN : C_CARD_EDGE, 14);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_MUTED);
        SelectObject(dc, g_small);
        RECT label{card.left + 16, card.top + 14, card.right - 14, card.top + 34};
        DrawTextW(dc, labels[i], -1, &label, DT_LEFT | DT_SINGLELINE);
        SetTextColor(dc, C_TEXT);
        SelectObject(dc, g_bold);
        RECT value{card.left + 16, card.top + 39, card.right - 14, card.top + 78};
        DrawTextW(dc, values[i].c_str(), -1, &value, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, C_SOFT);
        SelectObject(dc, g_small);
        RECT note{card.left + 16, card.bottom - 26, card.right - 14, card.bottom - 8};
        DrawTextW(dc, i == 0 ? since.c_str() : L"Local model usage", -1, &note,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }

    if (models.empty())
    {
        RECT empty{area.left + margin, area.top + summaryHeight + 2 * margin, area.right - margin, area.bottom - margin};
        DrawNeonPanel(dc, empty);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_MUTED);
        DrawTextW(dc, L"No model savings recorded yet.", -1, &empty, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    const int modelGap = 12, modelHeight = 104;
    const int modelWidth = std::max(1, (width - modelGap) / 2);
    for (size_t i = 0; i < models.size(); ++i)
    {
        const auto &model = models[i];
        const int col = (int)(i % 2), row = (int)(i / 2);
        RECT card{area.left + margin + col * (modelWidth + modelGap),
                  area.top + summaryHeight + margin + row * (modelHeight + 10),
                  area.left + margin + col * (modelWidth + modelGap) + modelWidth,
                  area.top + summaryHeight + margin + row * (modelHeight + 10) + modelHeight};
        DrawNeonPanel(dc, card, C_CARD_EDGE, 14);
        const std::wstring name = model.is_object() ? W(ModelDisplayName(model.value("model", std::string("Model")))) : L"Model";
        const std::wstring quant = model.is_object() ? W(model.value("quant", std::string(""))) : L"";
        const std::wstring saved = model.is_object() ? W(SavingsMoney(model.value("usd", Json()))) : L"–";
        const auto tokenData = model.is_object() ? model.value("tokens", Json::object()) : Json::object();
        const auto modelRequests = tokenData.is_object() ? W(SavingsTokens(tokenData.value("requests", Json()))) : L"–";
        RECT title{card.left + 16, card.top + 12, card.right - 16, card.top + 34};
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_TEXT);
        SelectObject(dc, g_bold);
        DrawTextW(dc, name.c_str(), -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT detail{card.left + 16, card.top + 40, card.right - 16, card.top + 60};
        SetTextColor(dc, C_CYAN_TEXT);
        SelectObject(dc, g_small);
        const std::wstring sub = quant + L"  ·  " + saved + L" saved" +
            (model.is_object() && model.value("running", false) ? L"  ·  running" : L"");
        DrawTextW(dc, sub.c_str(), -1, &detail, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        RECT count{card.left + 16, card.top + 66, card.right - 16, card.bottom - 8};
        SetTextColor(dc, C_MUTED);
        DrawTextW(dc, (modelRequests + L" requests").c_str(), -1, &count, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

RECT PageScrollbarRect(const RECT &client, int scroll, int limit)
{
    RECT track{client.right - 10, 146, client.right - 4, client.bottom - 18};
    if (limit <= 0 || track.bottom <= track.top) return {};
    const int trackHeight = track.bottom - track.top;
    const int viewport = std::max(1, (int)client.bottom - 22);
    const int thumbHeight = std::clamp((int)((int64_t)trackHeight * viewport / (viewport + limit)), 44, trackHeight);
    const int thumbTravel = std::max(0, trackHeight - thumbHeight);
    const int thumbTop = track.top + (limit > 0 ? thumbTravel * std::clamp(scroll, 0, limit) / limit : 0);
    return {track.left, thumbTop, track.right, thumbTop + thumbHeight};
}

int CurrentPageScrollPosition() noexcept
{
    return g_page == MONITOR ? g_monitorScrollY : g_page == MODELS ? g_catalogScrollY : 0;
}

bool SetCurrentPageScrollPosition(int next)
{
    if (!g_hwnd || g_pageScrollLimit <= 0) return false;
    next = std::clamp(next, 0, g_pageScrollLimit);
    int &scroll = g_page == MONITOR ? g_monitorScrollY : g_catalogScrollY;
    if (next == scroll) return false;
    const int delta = scroll - next;
    scroll = next;
    RECT client{};
    GetClientRect(g_hwnd, &client);
    RECT viewport{SidebarWidth(), std::min(128, (int)client.bottom),
                  std::max(SidebarWidth(), (int)client.right - 12), (int)client.bottom};
    RECT exposed = viewport;
    if (viewport.bottom > viewport.top && viewport.right > viewport.left)
    {
        RECT update{};
        ScrollWindowEx(g_hwnd, 0, delta, &viewport, &viewport, nullptr, &update,
                       SW_SCROLLCHILDREN | SW_INVALIDATE);
        if (!IntersectRect(&exposed, &update, &viewport))
            exposed = viewport;
    }
    NativeLayoutTransition transition(&exposed);
    Layout();
    RECT scrollbar{std::max(0, (int)client.right - 12), std::min(128, (int)client.bottom),
                   (int)client.right, (int)client.bottom};
    InvalidateRect(g_hwnd, &scrollbar, FALSE);
    return true;
}

void DrawPageScrollbar(HDC dc, const RECT &client)
{
    if (g_pageScrollLimit <= 0) return;
    RECT track{client.right - 10, 146, client.right - 4, client.bottom - 18};
    const auto thumb = PageScrollbarRect(client, CurrentPageScrollPosition(), g_pageScrollLimit);
    HBRUSH trackBrush = CreateSolidBrush(C_CARD3);
    HBRUSH thumbBrush = CreateSolidBrush(C_CYAN);
    HPEN trackPen = CreatePen(PS_SOLID, 1, C_CARD3);
    HPEN thumbPen = CreatePen(PS_SOLID, 1, C_CYAN);
    auto oldBrush = SelectObject(dc, trackBrush);
    auto oldPen = SelectObject(dc, trackPen);
    RoundRect(dc, track.left, track.top, track.right, track.bottom, 6, 6);
    SelectObject(dc, thumbBrush);
    SelectObject(dc, thumbPen);
    RoundRect(dc, thumb.left, thumb.top, thumb.right, thumb.bottom, 6, 6);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(trackBrush);
    DeleteObject(thumbBrush);
    DeleteObject(trackPen);
    DeleteObject(thumbPen);
}

void DrawMonitorRequestCard(const DRAWITEMSTRUCT &item)
{
    const int index = (int)item.itemID;
    if (!g_metrics.is_object() || !g_metrics.contains("requests") || !g_metrics["requests"].is_array() ||
        index < 0 || (size_t)index >= g_metrics["requests"].size())
        return;
    const Json &request = g_metrics["requests"][index];
    const auto cells = MonitorRequestColumns(request);
    const std::wstring status = cells[1].empty() ? L"—" : cells[1];
    const std::string finish = request.is_object() ? request.value("finish", std::string()) : std::string();
    const COLORREF statusColor = finish == "stop" || finish == "length" ? C_GREEN
        : finish == "cancel" || finish == "disconnect" ? C_WARN : finish == "error" ? C_RED : C_MUTED;
    HDC dc = item.hDC;
    RECT row = item.rcItem;
    HBRUSH rowBrush = CreateSolidBrush(C_BG);
    FillRect(dc, &row, rowBrush);
    DeleteObject(rowBrush);
    RECT card = row;
    InflateRect(&card, -4, -3);
    const bool selected = (item.itemState & ODS_SELECTED) != 0;
    HBRUSH cardBrush = CreateSolidBrush(selected ? C_CARD2 : C_CARD);
    HPEN cardPen = CreatePen(PS_SOLID, 2, selected ? C_CYAN : C_CARD_EDGE);
    HGDIOBJ oldBrush = SelectObject(dc, cardBrush);
    HGDIOBJ oldPen = SelectObject(dc, cardPen);
    RoundRect(dc, card.left, card.top, card.right, card.bottom, 12, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(cardBrush);
    DeleteObject(cardPen);

    RECT statusMeasure{0, 0, 0, 0};
    SelectObject(dc, g_bold);
    DrawTextW(dc, status.c_str(), -1, &statusMeasure, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    const int badgeWidth = std::max(70, (int)statusMeasure.right + 26);
    RECT badge{card.right - badgeWidth - 10, card.top + 8, card.right - 10, card.top + 30};
    HBRUSH badgeBrush = CreateSolidBrush(C_CARD2);
    HPEN badgePen = CreatePen(PS_SOLID, 1, statusColor);
    oldBrush = SelectObject(dc, badgeBrush);
    oldPen = SelectObject(dc, badgePen);
    RoundRect(dc, badge.left, badge.top, badge.right, badge.bottom, 11, 11);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(badgeBrush);
    DeleteObject(badgePen);
    HBRUSH dotBrush = CreateSolidBrush(statusColor);
    HPEN dotPen = CreatePen(PS_SOLID, 1, statusColor);
    oldBrush = SelectObject(dc, dotBrush);
    oldPen = SelectObject(dc, dotPen);
    Ellipse(dc, badge.left + 7, badge.top + 8, badge.left + 13, badge.top + 14);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(dotBrush);
    DeleteObject(dotPen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, statusColor);
    RECT statusText = badge;
    statusText.left += 18;
    statusText.right -= 7;
    DrawTextW(dc, status.c_str(), -1, &statusText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

    SetTextColor(dc, C_TEXT);
    SelectObject(dc, g_bold);
    const std::wstring heading = cells[0] + L"  ·  " + cells[2];
    RECT headingRect{card.left + 14, card.top + 7, badge.left - 8, card.top + 29};
    DrawTextW(dc, heading.c_str(), -1, &headingRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SetTextColor(dc, C_SOFT);
    SelectObject(dc, g_small);
    std::wstring line = L"Prompt " + cells[3] + L"  ·  reused " + cells[4] + L"  ·  prefill " + cells[5] + L" tok/s";
    RECT details{card.left + 14, card.top + 32, card.right - 12, card.top + 49};
    DrawTextW(dc, line.c_str(), -1, &details, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    line = L"Output " + cells[6] + L"  ·  decode " + cells[7] + L" tok/s  ·  VRAM hit " + cells[8] + L"  ·  " + cells[9] + L" s";
    details.top += 17;
    details.bottom += 17;
    DrawTextW(dc, line.c_str(), -1, &details, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (item.itemState & ODS_FOCUS) DrawFocusRect(dc, &card);
}
std::wstring MonitorSpeculationCardValue(const Json &metrics)
{
    const std::wstring full = MonitorSpeculationText(metrics);
    if (full == L"off" || full == L"not reported") return full;
    const size_t separator = full.find(L" · ");
    if (separator == std::wstring::npos) return full;
    const size_t valueStart = separator + 3;
    const size_t percentEnd = full.find(L'%', valueStart);
    if (percentEnd != std::wstring::npos) return full.substr(valueStart, percentEnd - valueStart + 1);
    return full.substr(valueStart);
}

void DrawMonitorStats(HDC dc, int x, int y, int width, const Json &metrics, bool mobile)
{
    const int inset = std::min(22, std::max(0, width / 2));
    const int contentX = x + inset;
    const int contentWidth = std::max(1, width - 2 * inset);
    const int gap = 12;
    const int columns = MonitorStatsColumns(width, mobile);
    const int cellWidth = std::max(1, (contentWidth - gap * (columns - 1)) / columns);
    HPEN rule = CreatePen(PS_SOLID, 1, C_LINE);
    const auto oldPen = SelectObject(dc, rule);
    MoveToEx(dc, contentX, y + 14, nullptr);
    LineTo(dc, contentX + contentWidth, y + 14);
    SelectObject(dc, oldPen);
    DeleteObject(rule);

    const auto hitValue = MonitorVramHitValue(metrics);
    auto timeValue = MonitorTimePerTokenValue(metrics);
    if (timeValue != L"—") timeValue += L" ms";
    const auto requests = MonitorMetric(metrics.value("totals", Json::object()), "requests", 0);
    const std::wstring labels[] = {L"Time per token", L"VRAM hit", L"Requests served", L"Speculation"};
    const std::wstring values[] = {timeValue, hitValue, requests, MonitorSpeculationCardValue(metrics)};
    SetBkMode(dc, TRANSPARENT);
    for (int i = 0; i < 4; ++i)
    {
        const int column = i % columns, row = i / columns;
        const int cellX = contentX + column * (cellWidth + gap);
        const int cellY = y + 20 + row * 30;
        const int span = MonitorStatsColumnSpan(i, columns, mobile);
        const int statWidth = span == columns ? contentWidth : cellWidth;
        RECT label{cellX, cellY, cellX + statWidth, cellY + 12};
        RECT value{cellX, cellY + 12, cellX + statWidth, cellY + 30};
        SetTextColor(dc, C_MUTED);
        SelectObject(dc, g_small);
        DrawTextW(dc, labels[i].c_str(), -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SetTextColor(dc, C_TEXT);
        SelectObject(dc, g_bold);
        DrawTextW(dc, values[i].c_str(), -1, &value, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}
void DrawMonitorContextBar(HDC dc, const RECT &card, const MonitorContextUsage &context)
{
    const int left = card.left + 18, top = card.top + 82, width = card.right - card.left - 32, height = 28;
    if (width <= 0) return;
    HBRUSH track = CreateSolidBrush(C_CARD3);
    HPEN edge = CreatePen(PS_SOLID, 1, C_CARD3);
    const auto oldBrush = SelectObject(dc, track);
    const auto oldPen = SelectObject(dc, edge);
    RoundRect(dc, left, top, left + width, top + height, 14, 14);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(track);
    DeleteObject(edge);
    if (context.maximum <= 0 || context.used <= 0) return;
    int drawn = 0;
    int64_t remaining = context.used;
    const auto segment = [&](int64_t amount, COLORREF color) {
        amount = std::clamp<int64_t>(amount, 0, remaining);
        const int segmentWidth = std::clamp((int)std::llround((double)amount * width / context.maximum), 0, width - drawn);
        if (segmentWidth > 0)
        {
            RECT rect{left + drawn, top, left + drawn + segmentWidth, top + height};
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(dc, &rect, brush);
            DeleteObject(brush);
        }
        drawn += segmentWidth;
        remaining -= amount;
    };
    segment(context.reused, C_PURPLE);
    segment(context.fresh, C_CYAN);
    segment(context.written, C_GREEN);
    if (context.window > 0)
    {
        const int x = left + (int)std::llround((double)context.window * width / context.maximum);
        HPEN marker = CreatePen(PS_SOLID, 2, C_TEXT);
        const auto old = SelectObject(dc, marker);
        MoveToEx(dc, x, top - 2, nullptr);
        LineTo(dc, x, top + height + 2);
        SelectObject(dc, old);
        DeleteObject(marker);
    }
}
void DrawMonitorRightNow(HDC dc, const RECT &card)
{
    DrawCard(dc, card, L"Right now", L"", L"");
    const auto liveValue = g_metrics.value("live", Json::object());
    const auto live = liveValue.is_object() ? liveValue : Json::object();
    const auto requests = g_metrics.value("requests", Json::array());
    const Json last = requests.is_array() && !requests.empty() && requests[0].is_object()
        ? requests[0] : Json::object();
    const auto activity = MonitorRequestActivityFor(g_metrics);
    const bool reloading = g_busy == J_START || g_busy == J_RESTART || g_busy == J_UPDATE_START;
    const bool running = activity != MonitorRequestActivity::idle;
    std::wstring label = reloading ? L"Reloading" : activity == MonitorRequestActivity::reading ? L"Reading"
        : activity == MonitorRequestActivity::generating ? MonitorText(live, "phase", L"Writing") : L"Idle";
    if (label.empty()) label = L"Writing";
    if (!label.empty() && label != L"Reloading" && label != L"Reading" && label != L"Writing" &&
        label[0] >= L'a' && label[0] <= L'z') label[0] -= L'a' - L'A';
    const COLORREF tone = reloading || activity == MonitorRequestActivity::reading ? C_CYAN
        : activity == MonitorRequestActivity::generating ? C_PURPLE : C_CARD3;
    const bool compact = card.right - card.left < 360;
    RECT badge{card.left + 18, card.top + 43, card.left + 112, card.top + 67};
    HBRUSH badgeBrush = CreateSolidBrush(tone);
    HPEN badgePen = CreatePen(PS_SOLID, 1, tone);
    const auto oldBrush = SelectObject(dc, badgeBrush);
    const auto oldPen = SelectObject(dc, badgePen);
    RoundRect(dc, badge.left, badge.top, badge.right, badge.bottom, 12, 12);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(badgeBrush);
    DeleteObject(badgePen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, tone == C_CARD3 ? C_TEXT : C_BG);
    SelectObject(dc, g_bold);
    DrawTextW(dc, label.c_str(), -1, &badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    auto duration = [](const Json &object, const char *key) {
        const auto value = object.value(key, Json());
        if (!value.is_number()) return std::wstring();
        const double seconds = value.get<double>();
        if (!std::isfinite(seconds) || seconds < 0) return std::wstring();
        std::wostringstream text;
        if (seconds < 90) text << std::fixed << std::setprecision(seconds < 10 ? 1 : 0) << seconds << L" s";
        else text << (int64_t)(seconds / 60) << L" min " << (int)std::fmod(seconds, 60) << L" s";
        return std::wstring(L" · ") + text.str();
    };
    std::wstring detail;
    if (reloading) detail = L"the engine is restarting";
    else if (activity == MonitorRequestActivity::reading)
    {
        const auto total = MonitorInteger(live, "prompt_total");
        detail = total > 0 ? MonitorCountLabel(live, "prompt_read") + L" / " +
            MonitorNumberLabel((double)total) + L" prompt tokens"
            : MonitorCountLabel(live, "prompt_tokens") + L" prompt tokens";
    }
    else if (activity == MonitorRequestActivity::generating)
        detail = MonitorCountLabel(live, "generated") + L" tokens" + duration(live, "elapsed_s");
    else if (!last.empty())
        detail = L"last answer: " + MonitorCountLabel(last, "output_tokens") + L" tokens" +
            duration(last, "duration_s");
    else detail = L"waiting for a request";
    RECT detailRect{card.left + (compact ? 18 : 122), card.top + (compact ? 72 : 43),
                    card.right - 18, card.top + (compact ? 91 : 69)};
    SetTextColor(dc, C_MUTED);
    SelectObject(dc, g_small);
    DrawTextW(dc, detail.c_str(), -1, &detailRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    const int progress = reloading ? -1 : MonitorRequestProgressPercent(g_metrics, activity);
    if (progress >= 0)
    {
        const int left = card.left + 18, top = card.top + (compact ? 98 : 79);
        const int width = std::max(0, (int)(card.right - card.left) - 36);
        if (width > 0)
        {
            HBRUSH track = CreateSolidBrush(C_CARD3);
            HBRUSH fill = CreateSolidBrush(activity == MonitorRequestActivity::reading ? C_CYAN : C_PURPLE);
            HPEN pen = CreatePen(PS_SOLID, 1, C_CARD3);
            const auto oldB = SelectObject(dc, track);
            const auto oldP = SelectObject(dc, pen);
            RoundRect(dc, left, top, left + width, top + 8, 8, 8);
            SelectObject(dc, fill);
            const int fillWidth = width * progress / 100;
            if (fillWidth > 0) RoundRect(dc, left, top, left + fillWidth, top + 8, 8, 8);
            SelectObject(dc, oldB);
            SelectObject(dc, oldP);
            DeleteObject(track);
            DeleteObject(fill);
            DeleteObject(pen);
        }
    }

    const auto drawRow = [&](int y, const wchar_t *name, const std::wstring &value) {
        RECT left{card.left + 18, y, card.left + (card.right - card.left) / 2, y + 20};
        RECT right{left.right, y, card.right - 18, y + 20};
        SetTextColor(dc, C_MUTED);
        SelectObject(dc, g_small);
        DrawTextW(dc, name, -1, &left, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SetTextColor(dc, C_TEXT);
        SelectObject(dc, g_bold);
        DrawTextW(dc, value.c_str(), -1, &right, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    };
    int rowY = card.top + (progress >= 0 ? (compact ? 116 : 98) : (compact ? 104 : 86));
    if (!reloading && running && MonitorInteger(live, "prompt_total") > 0)
        drawRow(rowY, L"Prompt read", MonitorCountLabel(live, "prompt_read") +
            L" of " + MonitorNumberLabel((double)MonitorInteger(live, "prompt_total")));
    else if (!last.empty())
        drawRow(rowY, L"Reused from the cache", MonitorCountLabel(last, "reused") + L" tokens");
    rowY += 30;
    drawRow(rowY, L"Answers at once", MonitorCountLabel(live, "active"));
    rowY += 30;
    const auto queued = MonitorInteger(live, "queued", -1);
    drawRow(rowY, L"Waiting", queued < 0 ? L"—" : queued > 0 ? MonitorNumberLabel((double)queued) : L"none");
}

void DrawMonitorContextCard(HDC dc, const RECT &card)
{
    DrawCard(dc, card, L"Context", L"", L"");
    const auto context = MonitorContextUsageValues(g_metrics, g_health);
    const auto liveValue = g_metrics.value("live", Json::object());
    const auto live = liveValue.is_object() ? liveValue : Json::object();
    const auto requests = g_metrics.value("requests", Json::array());
    const bool hasLast = requests.is_array() && !requests.empty() && requests[0].is_object();
    const std::wstring what = context.live
        ? (MonitorInteger(live, "active") > 1 ? L"the newest of " + MonitorNumberLabel((double)MonitorInteger(live, "active")) + L" answers being written"
                                             : L"the request being answered")
        : (hasLast ? L"the last request" : L"");
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_MUTED);
    SelectObject(dc, g_small);
    RECT whatRect{card.left + 180, card.top + 13, card.right - 18, card.top + 34};
    DrawTextW(dc, what.c_str(), -1, &whatRect, DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (context.maximum <= 0)
    {
        RECT unavailable{card.left + 18, card.top + 45, card.right - 18, card.bottom - 12};
        DrawTextW(dc, L"Context size unavailable", -1, &unavailable, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        return;
    }

    const auto used = MonitorNumberLabel((double)context.used);
    const auto maximum = MonitorNumberLabel((double)context.maximum);
    const auto free = MonitorNumberLabel((double)std::max<int64_t>(0, context.maximum - context.used));
    RECT valueRect{card.left + 18, card.top + 39, card.left + (card.right - card.left) / 2, card.top + 67};
    SetTextColor(dc, C_TEXT);
    SelectObject(dc, g_bold);
    const std::wstring value = used + L" / " + maximum + L" tokens";
    DrawTextW(dc, value.c_str(), -1, &valueRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    const double pct = 100.0 * (double)context.used / (double)context.maximum;
    const std::wstring pctText = pct > 0 && pct < 0.1 ? L"< 0.1" : MonitorNumberLabel(pct, pct > 0 && pct < 10 ? 1 : 0);
    RECT percentRect{valueRect.right + 8, card.top + 40, card.right - 18, card.top + 66};
    SetTextColor(dc, C_MUTED);
    SelectObject(dc, g_small);
    const std::wstring percentage = pctText + L"% used · " + free + L" left";
    DrawTextW(dc, percentage.c_str(), -1, &percentRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    DrawMonitorContextBar(dc, card, context);

    const int legendY = card.top + 116;
    std::vector<std::wstring> legend;
    if (context.reusedReported)
    {
        legend.push_back(L"Reused " + MonitorNumberLabel((double)context.reused));
        legend.push_back(L"Read new " + MonitorNumberLabel((double)context.fresh));
    }
    else legend.push_back(L"Prompt " + MonitorNumberLabel((double)context.fresh));
    legend.push_back(L"Written " + MonitorNumberLabel((double)context.written));
    legend.push_back(L"Free " + free);
    const int legendWidth = std::max(1, ((int)(card.right - card.left) - 36) / (int)legend.size());
    for (size_t i = 0; i < legend.size(); ++i)
    {
        RECT item{card.left + 18 + (int)i * legendWidth, legendY,
                  i + 1 == legend.size() ? card.right - 18 : card.left + 18 + ((int)i + 1) * legendWidth - 8,
                  legendY + 18};
        const COLORREF color = context.reusedReported
            ? (i == 0 ? C_PURPLE : i == 1 ? C_CYAN : i == 2 ? C_GREEN : C_CARD3)
            : (i == 0 ? C_CYAN : i == 1 ? C_GREEN : C_CARD3);
        RECT swatch{item.left, item.top + 5, item.left + 8, item.top + 13};
        HBRUSH swatchBrush = CreateSolidBrush(color);
        FillRect(dc, &swatch, swatchBrush);
        DeleteObject(swatchBrush);
        if (color == C_CARD3)
        {
            HPEN swatchPen = CreatePen(PS_SOLID, 1, C_MUTED);
            const auto oldPen = SelectObject(dc, swatchPen);
            const auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, swatch.left, swatch.top, swatch.right, swatch.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(swatchPen);
        }
        SetTextColor(dc, context.reusedReported && i == 0 ? C_PURPLE_TEXT
                        : context.reusedReported && i == 1 ? C_CYAN_TEXT
                        : (!context.reusedReported && i == 0) ? C_CYAN_TEXT
                        : ((context.reusedReported && i == 2) || (!context.reusedReported && i == 1))
                            ? C_GREEN_TEXT : C_MUTED);
        item.left += 12;
        DrawTextW(dc, legend[i].c_str(), -1, &item, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }

    std::wstring note;
    if (!context.live && !hasLast)
        note = L"No conversation yet; context fills with messages, files and tool results. Set its limit in Settings > Chat defaults > Context size.";
    else if (context.window > 0)
    {
        const auto window = MonitorContextSizeLabel(Json(context.window));
        if (context.used > context.window)
        {
            const auto ram = MonitorNumberLabel((double)context.used * 12288.0 / 1e9, 1);
            note = L"Past the " + window + L" VRAM window: about " + ram + L" GB of cache uses RAM; the rest is copied in as needed. Set the window in Models > Engine settings > Prompts and cache.";
        }
        else note = L"VRAM window: " + window + L" stays in GPU memory; longer chats keep the rest in RAM. Set the window in Models > Engine settings > Prompts and cache.";
    }
    else
        note = L"Chats beyond this context are refused. Set the limit in Settings > Chat defaults > Context size.";
    RECT noteRect{card.left + 18, card.top + 138, card.right - 18, card.bottom - 7};
    SetTextColor(dc, C_MUTED);
    SelectObject(dc, g_small);
    DrawTextW(dc, note.c_str(), -1, &noteRect, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
}

void DrawMonitorSparkline(HDC dc, RECT area, const char *key, COLORREF color, int penStyle, bool fillArea)
{
    const auto history = g_metrics.value("history", Json::object());
    const auto points = MonitorSparklinePoints(history.value(key, Json::array()), area.right - area.left,
                                               area.bottom - area.top);
    if (points.size() < 2)
        return;
    if (fillArea)
    {
        const auto shape = MonitorSparklineAreaPoints(points, area.bottom - area.top);
        std::vector<POINT> polygon;
        polygon.reserve(shape.size());
        for (const auto &point : shape)
            polygon.push_back({area.left + point.x, area.top + point.y});
        const auto tint = [](BYTE background, BYTE line) {
            return (BYTE)((background * 88 + line * 12 + 50) / 100);
        };
        const auto fill = CreateSolidBrush(RGB(tint(GetRValue(C_CARD), GetRValue(color)),
                                               tint(GetGValue(C_CARD), GetGValue(color)),
                                               tint(GetBValue(C_CARD), GetBValue(color))));
        const auto oldBrush = SelectObject(dc, fill);
        const auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
        Polygon(dc, polygon.data(), (int)polygon.size());
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(fill);
    }
    const auto pen = CreatePen(penStyle, 2, color);
    const auto old = SelectObject(dc, pen);
    MoveToEx(dc, area.left + points[0].x, area.top + points[0].y, nullptr);
    for (size_t i = 1; i < points.size(); ++i)
        LineTo(dc, area.left + points[i].x, area.top + points[i].y);
    SelectObject(dc, old);
    DeleteObject(pen);
}
void DrawMonitorTierBar(HDC dc, int x, int y, int width, const Json &metrics)
{
    const auto now = MonitorTierNow(metrics);
    if (!now.is_object() || now.empty() || width < 180) return;
    const auto shares = MonitorTierShares(metrics);
    const auto caption = MonitorTierCaption(metrics, static_cast<double>(std::time(nullptr)));
    HBRUSH brush = CreateSolidBrush(C_CARD);
    HPEN pen = CreatePen(PS_SOLID, 1, C_CARD_EDGE);
    const RECT card{x, y, x + width, y + MonitorTierCardHeight};
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, card.left, card.top, card.right, card.bottom, 14, 14);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_MUTED);
    SelectObject(dc, g_small);
    RECT title{x + 12, y + 3, x + width - 10, y + 19};
    const std::wstring label = MonitorTierTitle(metrics) +
                               (caption.empty() ? L"" : L"  ·  " + caption);
    DrawTextW(dc, label.c_str(), -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    const int bx = x + 12, by = y + 20, bw = width - 24, bh = MonitorTierBarHeight;
    const auto fractions = MonitorTierBarFractions(metrics);
    const int vram = (int)std::lround(bw * fractions[0]);
    const int ram = std::min(std::max(0, bw - vram), (int)std::lround(bw * fractions[1]));
    const int disk = std::max(0, bw - vram - ram);
    int cursor = bx;
    for (const auto &[partWidth, color] : {std::pair<int, COLORREF>{vram, C_PURPLE}, {ram, C_CYAN}, {disk, C_GREEN}})
    {
        if (partWidth <= 0) continue;
        RECT segment{cursor, by, cursor + partWidth, by + bh};
        HBRUSH segmentBrush = CreateSolidBrush(color);
        FillRect(dc, &segment, segmentBrush);
        DeleteObject(segmentBrush);
        cursor += partWidth;
    }
    const int col = width / 3;
    const auto count = [&](const char *key) {
        const auto value = shares.value(key, Json());
        return value.is_number() ? MonitorNumberLabel(value.get<double>()) : L"—";
    };
    const auto gb = [&](const char *key, bool showMissing) {
        const auto value = shares.value(key, Json());
        if (value.is_number()) return L" · " + MonitorNumberLabel(value.get<double>(), 1) + L" GB";
        return showMissing ? std::wstring(L" · — GB") : std::wstring();
    };
    const std::wstring labels[] = {
        MonitorTierLegendLabel(metrics, 0) + L" " + count("vram") + gb("vram_gb", true),
        MonitorTierLegendLabel(metrics, 1) + L" " + count("ram") + gb("ram_gb", true),
        MonitorTierLegendLabel(metrics, 2) + L" " + count("disk") + gb("disk_gb", false)};
    for (int i = 0; i < 3; ++i)
    {
        const int itemX = x + 12 + i * col;
        RECT swatch{itemX, y + 51, itemX + 9, y + 60};
        const COLORREF color = i == 0 ? C_PURPLE : i == 1 ? C_CYAN : C_GREEN;
        HBRUSH swatchBrush = CreateSolidBrush(color);
        FillRect(dc, &swatch, swatchBrush);
        DeleteObject(swatchBrush);
        RECT item{itemX + 14, y + 48, x + 12 + (i + 1) * col - 4, y + 65};
        SetTextColor(dc, i == 0 ? C_PURPLE_TEXT : i == 1 ? C_CYAN_TEXT : C_GREEN_TEXT);
        DrawTextW(dc, labels[i].c_str(), -1, &item, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}
void DrawMonitorHardwareHistory(HDC dc, int x, int y, int width, const Json &metrics)
{
    const auto hardware = metrics.value("hardware", Json::object());
    struct Chart { const char *key; const wchar_t *label; std::wstring value; COLORREF color; };
    const auto util = MonitorMetric(hardware, "gpu_util", 0);
    auto vram = MonitorGigabytes(hardware.value("gpu_mem_used", Json()));
    const auto vramTotal = hardware.value("gpu_mem_total", Json());
    if (vramTotal.is_number()) vram += L" / " + MonitorGigabytes(vramTotal);
    const auto power = MonitorMetric(hardware, "gpu_power", 0);
    const auto temp = MonitorMetric(hardware, "gpu_temp", 0);
    auto pcieGen = MonitorMetric(hardware, "gpu_pcie_gen_max", 0);
    if (pcieGen == L"—") pcieGen = MonitorMetric(hardware, "gpu_pcie_gen", 0);
    const auto pcieWidth = MonitorMetric(hardware, "gpu_pcie_width", 0);
    std::wstring pcie = L"—";
    if (pcieGen != L"—") pcie = L"Gen" + pcieGen;
    if (pcieWidth != L"—") pcie += L" x" + pcieWidth;
    const auto cpu = MonitorMetric(hardware, "cpu", 0);
    auto ram = MonitorGigabytes(hardware.value("ram_used", Json()));
    const auto ramTotal = hardware.value("ram_total", Json());
    if (ramTotal.is_number()) ram += L" / " + MonitorGigabytes(ramTotal);
    const auto disk = MonitorDiskReadRate(hardware);
    const auto staticHardware = metrics.value("hardware_static", Json::object());
    const auto rawGpuName = staticHardware.value("gpu_name", Json());
    const auto gpuName = rawGpuName.is_string() ? W(rawGpuName.get<std::string>()) : std::wstring{};
    const auto gpuReadings = hardware.value("gpus", Json::array());
    const bool gpuEach = gpuReadings.is_array() && gpuReadings.size() > 1 && MonitorGpuRows(metrics).size() > 1;
    const auto diskWrite = MonitorDiskWriteLabel(hardware);
    const Chart charts[] = {
        {"gpu_util", L"GPU active", util + L"%", C_CYAN}, {"gpu_mem_used", L"VRAM", vram, C_PURPLE},
        {"gpu_power", L"Power", power + L" W", RGB(224, 176, 64)}, {"gpu_temp", L"GPU temp", temp + L" °C", C_RED},
        {"gpu_pcie_rx_mb", L"PCIe", pcie, C_CYAN}, {"cpu", L"CPU", cpu + L"%", C_GREEN},
        {"ram_used", L"System RAM", ram, C_CYAN}, {"disk_read_mb", L"Disk read", disk, C_GREEN}};
    const int gap = 12;
    const int cellWidth = std::max(1, (width - 3 * gap) / 4);
    const int cellHeight = 102;
    for (int i = 0; i < 8; ++i)
    {
        const int col = i % 4, row = i / 4;
        const int cx = x + col * (cellWidth + gap), cy = y + row * (cellHeight + 10);
        RECT card{cx, cy, col == 3 ? x + width : cx + cellWidth, cy + cellHeight};
        DrawNeonPanel(dc, card, i < 4 ? C_CARD_EDGE : C_LINE, 14);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, C_MUTED);
        SelectObject(dc, g_small);
        RECT title{card.left + 14, cy + 10, card.right - 12, cy + 29};
        DrawTextW(dc, charts[i].label, -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        SetTextColor(dc, C_TEXT);
        SelectObject(dc, g_bold);
        RECT value{card.left + 14, cy + 30, card.right - 12, cy + 58};
        DrawTextW(dc, charts[i].value.c_str(), -1, &value, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        const auto subtext = MonitorHardwareSubtext(metrics, charts[i].key, gpuEach, gpuName);
        int sparklineTop = cy + 62;
        if (!subtext.empty())
        {
            RECT detailLine{card.left + 14, cy + 59, card.right - 12, cy + 75};
            SetTextColor(dc, C_SOFT);
            SelectObject(dc, g_small);
            DrawTextW(dc, subtext.c_str(), -1, &detailLine,
                      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
            sparklineTop = cy + 77;
        }
        DrawMonitorSparkline(dc, {card.left + 14, sparklineTop, card.right - 12, card.bottom - 9}, charts[i].key,
                             charts[i].color, PS_SOLID, true);
    }
}
void DrawMonitorGpuMap(HDC dc, int x, int y, int width, const Json &metrics, int opened, int selected,
                       const MonitorGpuSegmentHit &hover, const std::wstring &tip, POINT tipPoint)
{
    const auto rows = MonitorGpuRows(metrics);
    if (rows.empty())
        return;
    struct Part { const char *key; const wchar_t *label; COLORREF color; };
    static const Part parts[] = {
        {"weights", L"weights", RGB(57, 135, 229)}, {"kv", L"KV", RGB(25, 158, 112)},
        {"act", L"activations", RGB(201, 133, 0)}, {"prompt", L"prompt", RGB(26, 163, 190)},
        {"tp", L"tensor", RGB(85, 102, 208)}, {"drafter", L"drafter", RGB(213, 81, 129)},
        {"pool", L"experts", C_PURPLE}, {"helper", L"helper", RGB(162, 115, 209)},
        {"vision", L"vision", RGB(0, 131, 0)}, {"rest", L"CUDA/rest", C_MUTED},
        {"other", L"other apps", RGB(217, 89, 38)}, {"used", L"in use", C_PURPLE},
        {"free", L"free", C_CARD3}};
    const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
    const int height = MonitorGpuPanelHeight(rows.size(), fold && opened >= 0);
    RECT panel{x, y, x + width, y + height};
    auto brush = CreateSolidBrush(C_CARD);
    auto pen = CreatePen(PS_SOLID, 1, C_CARD_EDGE);
    auto oldBrush = SelectObject(dc, brush);
    auto oldPen = SelectObject(dc, pen);
    RoundRect(dc, panel.left, panel.top, panel.right, panel.bottom, 16, 16);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, C_TEXT);
    SelectObject(dc, g_small);
    RECT title{x + 14, y + 5, x + width - 12, y + 20};
    DrawTextW(dc, rows.size() == 1 ? L"WHAT FILLS THE GPU" : L"WHAT FILLS EACH GPU", -1, &title,
              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    const auto hardware = metrics.value("hardware", Json::object());
    const auto readings = hardware.value("gpus", Json::array());
    int legendX = x + 14;
    for (const auto &part : parts)
    {
        bool present = std::string(part.key) == "free";
        for (const auto &gpu : rows)
        {
            const auto values = gpu.value("parts", Json::object());
            if (std::string(part.key) == "used" && gpu.value("plain", false))
                present = true;
            else if (std::string(part.key) != "used" && std::string(part.key) != "free" &&
                values.is_object() && values.contains(part.key) && values[part.key].is_number() &&
                values[part.key].get<double>() > 0)
                present = true;
        }
        if (!present)
            continue;
        RECT swatch{legendX, y + 24, legendX + 10, y + 34};
        if (std::string(part.key) == "free")
        {
            auto swatchBrush = CreateSolidBrush(C_CARD);
            FillRect(dc, &swatch, swatchBrush);
            DeleteObject(swatchBrush);
            auto swatchPen = CreatePen(PS_SOLID, 1, C_MUTED);
            auto oldPen = SelectObject(dc, swatchPen);
            auto oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, swatch.left, swatch.top, swatch.right, swatch.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(swatchPen);
        }
        else
        {
            auto swatchBrush = CreateSolidBrush(part.color);
            FillRect(dc, &swatch, swatchBrush);
            DeleteObject(swatchBrush);
        }
        SIZE extent{};
        GetTextExtentPoint32W(dc, part.label, (int)wcslen(part.label), &extent);
        RECT label{legendX + 14, y + 22, legendX + 14 + extent.cx, y + 36};
        SetTextColor(dc, C_MUTED);
        DrawTextW(dc, part.label, -1, &label, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        legendX += 14 + extent.cx + 12;
    }

    const int usedWidth = 112;
    const int barX = x + 14;
    const int barWidth = std::max(1, width - 28);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const auto &gpu = rows[i];
        const int rowY = y + 39 + (int)i * MonitorGpuRowHeight +
                         (fold && opened >= 0 && (int)i > opened ? 46 : 0);
        int index = gpu.value("dev", -1);
        if (gpu.contains("index") && gpu["index"].is_number())
            index = gpu["index"].get<int>();
        std::wstring heading = L"GPU " + std::to_wstring(index);
        const auto name = MonitorText(gpu, "name", L"");
        if (!name.empty())
            heading += L" · " + name;
        if (fold) heading += (int)i == opened ? L"  ·  Details open" : L"  ·  Click for details";
        if (gpu.value("measured", std::string()) == "start")
            heading += L" · model-start estimate";
        SetTextColor(dc, C_SOFT);
        RECT nameRect{x + 14, rowY, x + width - usedWidth - 14, rowY + 15};
        if (!fold)
            DrawTextW(dc, heading.c_str(), -1, &nameRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        if (fold && (int)i == selected && GetFocus() == g_hwnd)
        {
            RECT focusRect{x + 8, rowY - 2, x + width - 8, rowY + (opened == (int)i ? 108 : 55)};
            DrawFocusRect(dc, &focusRect);
        }

        const auto total = gpu.value("total", Json());
        const auto free = gpu.value("free", Json());
        const double capacity = total.is_number() ? std::max(0.0, total.get<double>()) : 0.0;
        const double freeBytes = free.is_number() ? std::max(0.0, free.get<double>()) : 0.0;
        if (capacity > 0)
        {
            RECT bar{barX, rowY + 16, barX + barWidth, rowY + 16 + MonitorGpuBarHeight};
            auto background = CreateSolidBrush(C_CARD3);
            FillRect(dc, &bar, background);
            DeleteObject(background);
            const auto values = gpu.value("parts", Json::object());
            double remaining = capacity;
            int drawn = 0;
            for (const auto &part : parts)
            {
                double bytes = 0;
                if (part.key == std::string("used") && gpu.value("plain", false))
                    bytes = values.value("used", 0.0);
                else if (part.key == std::string("free"))
                    bytes = freeBytes;
                else if (values.is_object() && values.contains(part.key) && values[part.key].is_number())
                    bytes = values[part.key].get<double>();
                bytes = std::min(remaining, std::max(0.0, bytes));
                if (bytes <= 0)
                    continue;
                int pixels = std::min(barWidth - drawn, (int)std::lround(barWidth * bytes / capacity));
                if (pixels <= 0)
                    continue;
                RECT segment{bar.left + drawn, bar.top, bar.left + drawn + pixels, bar.bottom};
                auto segmentBrush = CreateSolidBrush(part.color);
                FillRect(dc, &segment, segmentBrush);
                DeleteObject(segmentBrush);
                if (hover.row == (int)i && hover.key == part.key)
                {
                    auto hoverPen = CreatePen(PS_SOLID, 2, C_TEXT);
                    auto oldHoverPen = SelectObject(dc, hoverPen);
                    auto oldHoverBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
                    Rectangle(dc, segment.left, segment.top, segment.right, segment.bottom);
                    SelectObject(dc, oldHoverBrush);
                    SelectObject(dc, oldHoverPen);
                    DeleteObject(hoverPen);
                }
                drawn += pixels;
                remaining -= bytes;
                if (drawn >= barWidth)
                    break;
            }
            const auto usedText = MonitorGigabytes(Json(std::max(0.0, capacity - freeBytes))) + L" / " + MonitorGigabytes(total);
            RECT usedRect{x + width - usedWidth - 8, rowY, x + width - 10, rowY + 15};
            SetTextColor(dc, C_TEXT);
            DrawTextW(dc, usedText.c_str(), -1, &usedRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
        }

        std::wstring sensors;
        size_t tempValueOffset = 0, tempValueLength = 0;
        auto tempTone = MonitorGpuTemperatureTone::normal;
        if (readings.is_array())
            for (const auto &reading : readings)
                if (reading.is_object() && reading.value("index", -2) == index)
                {
                    const auto add = [&](const wchar_t *label, const char *key, const wchar_t *suffix) {
                        if (!reading.contains(key) || !reading[key].is_number())
                            return;
                        if (!sensors.empty()) sensors += L"  ·  ";
                        sensors += label;
                        const auto value = MonitorMetric(reading, key, 0);
                        if (std::string(key) == "temp")
                        {
                            tempValueOffset = sensors.size();
                            tempValueLength = value.size();
                            tempTone = MonitorGpuTemperatureLevel(reading);
                        }
                        sensors += value;
                        sensors += suffix;
                    };
                    add(L"Load ", "util", L"%");
                    add(L"Temp ", "temp", L" °C");
                    add(L"Power ", "power", L" W");
                    if (reading.contains("pcie_gen_max") && reading["pcie_gen_max"].is_number())
                    {
                        if (!sensors.empty()) sensors += L"  ·  ";
                        sensors += L"PCIe Gen" + MonitorMetric(reading, "pcie_gen_max", 0);
                        if (reading.contains("pcie_width") && reading["pcie_width"].is_number())
                            sensors += L" x" + MonitorMetric(reading, "pcie_width", 0);
                        if (reading.contains("pcie_rx_mb") && reading["pcie_rx_mb"].is_number())
                            sensors += L" · " + MonitorDataRate(reading["pcie_rx_mb"]);
                    }
                    break;
                }
        if (!sensors.empty())
        {
            RECT sensorRect{barX, rowY + 46, x + width - 12, rowY + 63};
            SetTextColor(dc, C_MUTED);
            DrawTextW(dc, sensors.c_str(), -1, &sensorRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (tempValueLength && tempValueOffset < sensors.size())
            {
                SIZE prefix{};
                if (GetTextExtentPoint32W(dc, sensors.c_str(), (int)tempValueOffset, &prefix))
                {
                    SetTextColor(dc, tempTone == MonitorGpuTemperatureTone::hot ? C_RED_TEXT
                                      : tempTone == MonitorGpuTemperatureTone::warm ? C_WARN : C_MUTED);
                    RECT tempRect = sensorRect;
                    tempRect.left += prefix.cx;
                    DrawTextW(dc, sensors.c_str() + tempValueOffset, (int)tempValueLength, &tempRect,
                              DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                }
            }
        }
        if (fold && (int)i == opened)
        {
            RECT details{x + 14, rowY + 64, x + width - 12, rowY + 104};
            SetTextColor(dc, C_MUTED);
            SelectObject(dc, g_small);
            const auto detail = MonitorGpuExpandedText(gpu, metrics.value("engine", Json::object()));
            DrawTextW(dc, detail.c_str(), -1, &details, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        }
    }
    if (!tip.empty())
    {
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, g_small);
        SIZE extent{};
        GetTextExtentPoint32W(dc, tip.c_str(), (int)tip.size(), &extent);
        const int tipWidth = std::min(width - 16, std::max(280, (int)extent.cx + 18));
        RECT measured{0, 0, tipWidth - 16, 0};
        DrawTextW(dc, tip.c_str(), -1, &measured, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
        const int tipHeight = std::min(84, std::max(34, (int)measured.bottom + 12));
        int left = tipPoint.x + 12;
        if (left + tipWidth > x + width - 8) left = tipPoint.x - tipWidth - 12;
        left = std::clamp(left, x + 8, x + width - tipWidth - 8);
        int top = tipPoint.y - tipHeight - 5;
        if (top < y + 37) top = tipPoint.y + 17;
        if (top + tipHeight > y + height - 4) top = y + height - tipHeight - 4;
        RECT box{left, top, left + tipWidth, top + tipHeight};
        auto tipBrush = CreateSolidBrush(C_CARD2);
        auto tipPen = CreatePen(PS_SOLID, 1, C_LINE);
        auto oldTipBrush = SelectObject(dc, tipBrush);
        auto oldTipPen = SelectObject(dc, tipPen);
        RoundRect(dc, box.left, box.top, box.right, box.bottom, 8, 8);
        SelectObject(dc, oldTipBrush);
        SelectObject(dc, oldTipPen);
        DeleteObject(tipBrush);
        DeleteObject(tipPen);
        SetTextColor(dc, C_TEXT);
        RECT text{box.left + 9, box.top + 5, box.right - 7, box.bottom - 5};
        DrawTextW(dc, tip.c_str(), -1, &text, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
}
bool HandleShortcut(const MSG &msg)
{
    if (msg.message != WM_KEYDOWN) return false;
    if (msg.wParam == VK_ESCAPE && NativeEscapeShouldYieldToPopup(NativeComboPopupOpen(g_hwnd))) return false;
    HWND segment = nullptr;
    if (g_page == SETTINGS)
        segment = GetFocus() == g_themeBox ? g_themeBox : GetFocus() == g_schemeBox ? g_schemeBox : nullptr;
    else if (g_page == CHAT_DEFAULTS)
        segment = GetFocus() == g_samplingPreset ? g_samplingPreset
                  : GetFocus() == g_samplingEffort ? g_samplingEffort : nullptr;
    if (segment && !(GetKeyState(VK_MENU) & 0x8000) && !SendMessageW(segment, CB_GETDROPPEDSTATE, 0, 0))
    {
        const int current = (int)SendMessageW(segment, CB_GETCURSEL, 0, 0);
        int count = (int)SendMessageW(segment, CB_GETCOUNT, 0, 0);
        if (segment == g_samplingPreset) --count; // Custom is a display state, not a web preset.
        const int next = NativeSegmentSelectionAfterKey(current, count, (unsigned)msg.wParam);
        if (next >= 0)
        {
            if (next != current)
            {
                SendMessageW(segment, CB_SETCURSEL, next, 0);
                SendMessageW(g_hwnd, WM_COMMAND,
                             MAKEWPARAM(GetDlgCtrlID(segment), CBN_SELCHANGE), (LPARAM)segment);
            }
            return true;
        }
        if (msg.wParam == VK_HOME || msg.wParam == VK_END) return true;
    }
    if (g_page == MODELS && g_modelSettingsOpen && !g_modelSettingsAdvanced &&
        !(GetKeyState(VK_MENU) & 0x8000))
        for (const auto &field : g_modelSettingFields)
            if (GetFocus() == field.control && field.kind == ModelSettingField::CHOICE &&
                (field.key == "draft" || field.key == "context") &&
                !SendMessageW(field.control, CB_GETDROPPEDSTATE, 0, 0))
            {
                const int current = (int)SendMessageW(field.control, CB_GETCURSEL, 0, 0);
                const int count = (int)SendMessageW(field.control, CB_GETCOUNT, 0, 0);
                const int next = NativeModelSettingSelectionAfterKey(
                    current, count, (unsigned)msg.wParam, field.key == "draft");
                if (next >= 0)
                {
                    if (next != current)
                    {
                        SendMessageW(field.control, CB_SETCURSEL, next, 0);
                        SendMessageW(g_hwnd, WM_COMMAND,
                                     MAKEWPARAM(field.id, CBN_SELCHANGE), (LPARAM)field.control);
                    }
                    return true;
                }
                if (field.key == "draft" && (msg.wParam == VK_HOME || msg.wParam == VK_END)) return true;
                break;
            }
    if ((GetKeyState(VK_MENU) & 0x8000) &&
        !NativeAltShortcutMayContinue((unsigned)msg.wParam, GetFocus() == g_input)) return false;
    if (g_page == MODELS && g_modelSheetOpen)
    {
        if (msg.wParam == VK_ESCAPE)
        {
            SendMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(NativeModelSheetCloseId, BN_CLICKED),
                         (LPARAM)g_modelSheetClose);
            return true;
        }
        if (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT)
            for (int i = 0; i < 3; ++i)
                if (GetFocus() == g_modelSheetTabs[i])
                {
                    const int next = ModelSheetNextTab(i, msg.wParam == VK_RIGHT ? 1 : -1,
                                                       IsWindowEnabled(g_modelSheetTabs[1]) != FALSE);
                    const UINT ids[] = {NativeModelSheetOverviewId, NativeModelSheetEngineId, NativeModelSheetStorageId};
                    SendMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(ids[next], BN_CLICKED),
                                 (LPARAM)g_modelSheetTabs[next]);
                    SetFocus(g_modelSheetTabs[next]);
                    return true;
                }
    }
    if (g_modelSettingsOpen && !g_modelSettingsAdvanced && (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT))
        for (int i = 0; i < 4; ++i)
            if (GetFocus() == g_modelSettingsTabs[i])
            {
                const int direction = msg.wParam == VK_RIGHT ? 1 : -1;
                std::array<bool, 4> available{};
                for (const auto &field : g_modelSettingFields) available[field.group] = true;
                const int next = NextModelSettingsTab(i, direction, available);
                if (next >= 0)
                {
                    SelectModelSettingsTab(next);
                    SetFocus(g_modelSettingsTabs[next]);
                    Layout();
                }
                return true;
            }
    if ((msg.wParam == VK_DOWN || msg.wParam == VK_UP) && GetFocus() == g_chatSearch)
    {
        const int selected = NativeChatSearchMoveSelection(
            (int)SendMessageW(g_chatList, LB_GETCURSEL, 0, 0), (int)g_visibleChats.size(),
            msg.wParam == VK_DOWN, [](int row) { return g_visibleChats[row].chat >= 0; });
        if (selected >= 0) SendMessageW(g_chatList, LB_SETCURSEL, selected, 0);
        return true;
    }
    if (msg.wParam == VK_RETURN && GetFocus() == g_chatSearch &&
        !(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000))
    {
        int selectedChatRow = (int)SendMessageW(g_chatList, LB_GETCURSEL, 0, 0);
        if (selectedChatRow < 0 || selectedChatRow >= (int)g_visibleChats.size() ||
            g_visibleChats[selectedChatRow].chat < 0)
            selectedChatRow = NativeChatSearchMoveSelection(
                -1, (int)g_visibleChats.size(), true, [](int row) { return g_visibleChats[row].chat >= 0; });
        const auto action = NativeChatSearchEnter(selectedChatRow >= 0);
        if (action == NativeChatSearchEnterAction::OPEN_SELECTED)
        {
            SendMessageW(g_chatList, LB_SETCURSEL, selectedChatRow, 0);
            SendMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(120, LBN_SELCHANGE), (LPARAM)g_chatList);
            SetFocus(g_input);
        }
        return true;
    }
    const bool control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    const auto shortcut = NativeShortcutForKey((unsigned)msg.wParam, control, shift);
    if (shortcut == NativeShortcut::SEND && GetFocus() == g_input)
    {
        HIMC ime = ImmGetContext(g_input);
        const bool composing = ime && ImmGetCompositionStringW(ime, GCS_COMPSTR, nullptr, 0) > 0;
        if (ime) ImmReleaseContext(g_input, ime);
        const int digitizer = GetSystemMetrics(SM_DIGITIZER);
        const bool coarsePointer = (digitizer & NID_READY) &&
                                   (digitizer & (NID_INTEGRATED_TOUCH | NID_EXTERNAL_TOUCH));
        if (NativeEnterMaySubmit(composing, coarsePointer))
        {
            SendMessageW(g_hwnd, WM_COMMAND, 122, 0);
            return true;
        }
        return false;
    }
    if (msg.wParam == VK_RETURN && GetFocus() == g_budgetEdit && !shift && !control)
    {
        if (IsWindowVisible(g_budgetSave) && IsWindowEnabled(g_budgetSave))
            SendMessageW(g_hwnd, WM_COMMAND, MAKEWPARAM(280, BN_CLICKED), (LPARAM)g_budgetSave);
        return true;
    }
    if (shortcut == NativeShortcut::NEWLINE && GetFocus() == g_input) return false;
    switch (shortcut)
    {
    case NativeShortcut::NEW_CHAT: SendMessageW(g_hwnd, WM_COMMAND, 126, 0); return true;
    case NativeShortcut::SEARCH:
        if (NativeSearchShouldClose(GetFocus() == g_chatSearch))
        {
            SetWindowTextW(g_chatSearch, L"");
            SetFocus(g_input);
            return true;
        }
        if (NativeSearchNeedsChatPage(g_page == CHAT)) ShowPage(CHAT);
        SetFocus(g_chatSearch);
        SendMessageW(g_chatSearch, EM_SETSEL, 0, -1);
        {
            const int first = NativeChatSearchMoveSelection(
                -1, (int)g_visibleChats.size(), true, [](int row) { return g_visibleChats[row].chat >= 0; });
            if (first >= 0) SendMessageW(g_chatList, LB_SETCURSEL, first, 0);
        }
        return true;
    case NativeShortcut::SETTINGS: ShowPage(IsSettingsPage(g_page) ? g_settingsReturnPage : SETTINGS); return true;
    case NativeShortcut::TOGGLE_SIDEBAR:
        ToggleSidebar();
        return true;
    case NativeShortcut::ESCAPE:
        switch (NativeEscapeForState(IsSettingsPage(g_page), g_page == CHAT, GetFocus() == g_chatSearch,
                                     GetWindowTextLengthW(g_chatSearch) > 0, g_editMessage >= 0, g_busy == J_CHAT))
        {
        case NativeEscapeAction::CLOSE_SETTINGS: ShowPage(g_settingsReturnPage); break;
        case NativeEscapeAction::CLEAR_SEARCH:
            SetWindowTextW(g_chatSearch, L"");
            SetFocus(g_input);
            break;
        case NativeEscapeAction::CANCEL_EDIT: CancelChatEdit(); break;
        case NativeEscapeAction::STOP_CHAT: StopChat(); break;
        case NativeEscapeAction::NONE: return false;
        }
        return true;
    default: return false;
    }
}
void ShowCodeMenu(HWND owner, LPARAM screen)
{
    POINT point{GET_X_LPARAM(screen), GET_Y_LPARAM(screen)};
    if (point.x == -1 && point.y == -1)
    {
        CHARRANGE selection{};
        SendMessageW(g_transcript, EM_EXGETSEL, 0, (LPARAM)&selection);
        const auto packed = SendMessageW(g_transcript, EM_POSFROMCHAR, selection.cpMin, 0);
        POINT caret{GET_X_LPARAM(packed), GET_Y_LPARAM(packed)};
        point = caret;
        ClientToScreen(g_transcript, &point);
    }
    POINT client = point;
    ScreenToClient(g_transcript, &client);
    const auto character = (LONG)SendMessageW(g_transcript, EM_CHARFROMPOS, 0, (LPARAM)&client);
    const auto found = std::find_if(g_codeBlocks.begin(), g_codeBlocks.end(), [&](const NativeCodeBlock &block) {
        return character >= block.range.cpMin && character < block.range.cpMax;
    });
    if (found == g_codeBlocks.end())
    {
        if (g_chat < 0 || g_chat >= (int)g_chats.size()) return;
        const int messageIndex = NativeMessageAt(g_userMessageRanges, character);
        if (messageIndex >= 0)
        {
            HMENU menu = CreatePopupMenu();
            if (!menu) return;
            AppendMenuW(menu, MF_STRING, 4201, L"Edit and send again");
            AppendMenuW(menu, MF_STRING, 4202, L"Copy message");
            const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
            DestroyMenu(menu);
            if (command == 4201) EditUserMessage(messageIndex);
            else if (command == 4202)
                SetText(g_status, CopyUnicodeText(owner, W(g_chats[g_chat].messages[messageIndex].text))
                                      ? L"Message copied." : L"Could not copy message.");
            return;
        }
        const int answerIndex = NativeMessageAt(g_assistantMessageRanges, character);
        if (answerIndex < 0) return;
        const auto &answer = g_chats[g_chat].messages[answerIndex];
        if (answer.text.empty() && answer.meta.empty()) return;
        HMENU menu = CreatePopupMenu();
        if (!menu) return;
        if (!answer.text.empty()) AppendMenuW(menu, MF_STRING, 4301, L"Copy answer");
        if (!answer.meta.empty()) AppendMenuW(menu, MF_STRING, 4302, L"Answer details");
        const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
        DestroyMenu(menu);
        if (command == 4301)
            SetText(g_status, CopyUnicodeText(owner, W(answer.text)) ? L"Answer copied to clipboard."
                                                                       : L"Could not copy the answer.");
        else if (command == 4302)
            MessageBoxW(owner, W(answer.meta).c_str(), L"Answer details", MB_OK | MB_ICONINFORMATION);
        return;
    }
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuW(menu, MF_STRING, 4101, L"Copy code");
    AppendMenuW(menu, MF_STRING, 4102, L"Open wrapped code view…");
    AppendMenuW(menu, MF_STRING, 4103, L"Save code as…");
    if (MayaHasHtmlPreview(found->complete, found->language, found->source))
        AppendMenuW(menu, MF_STRING, 4104, L"Preview HTML in browser…");
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0, owner, nullptr);
    DestroyMenu(menu);
    if (command == 4101)
    {
        SetText(g_status, CopyUnicodeText(owner, found->source) ? L"Code copied." : L"Could not copy code.");
    }
    else if (command == 4102)
    {
        SetText(g_status, ShowCodeViewer(owner, found->language, found->source)
                             ? L"Opened code block viewer. Toggle Wrap long lines in that window."
                             : L"Could not open code block viewer.");
    }
    else if (command == 4103)
    {
        const auto extension = MayaCodeExtension(found->language, found->source);
        wchar_t path[32768]{};
        const auto filename = MayaCodeFilename(extension);
        wcscpy_s(path, filename.c_str());
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = owner;
        ofn.lpstrFile = path;
        ofn.nMaxFile = (DWORD)std::size(path);
        ofn.lpstrFilter = L"Code files\0*.*\0All files\0*.*\0\0";
        ofn.lpstrDefExt = extension == L"Dockerfile" ? nullptr : extension.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn)) return;
        const auto bytes = U(found->source);
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            SetText(g_status, GetLastError() == ERROR_FILE_EXISTS ? L"That file exists; choose a new name." : L"Could not create the code file.");
            return;
        }
        DWORD written = 0;
        const bool ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr) && written == bytes.size();
        CloseHandle(file);
        SetText(g_status, ok ? L"Code file saved." : L"The code file could not be fully written.");
    }
    else if (command == 4104)
    {
        const auto path = fs::temp_directory_path() / (L"MayaPreview-" + NewId() + L".html");
        const auto bytes = MayaSandboxPreviewDocument(U(found->source));
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            SetText(g_status, L"Could not create the HTML preview file.");
            return;
        }
        DWORD written = 0;
        const bool ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr) && written == bytes.size();
        CloseHandle(file);
        if (!ok || (INT_PTR)ShellExecuteW(owner, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL) <= 32)
            SetText(g_status, L"Could not open the HTML preview.");
        else
            SetText(g_status, L"Preview opened in a sandboxed browser frame; it is isolated from Manager but can load external web resources.");
    }
}

void InvalidateMonitorGpuPanel(HWND h)
{
    if (g_page != MONITOR)
        return;
    RECT client{};
    if (!GetClientRect(h, &client))
        return;
    const int pageX = SidebarContentLeft();
    const auto bounds = MonitorPageBoundsFor(pageX, std::max(0, (int)client.right - pageX - 28));
    const auto rows = MonitorGpuRows(g_metrics);
    if (rows.empty() || bounds.width <= 0)
        return;
    const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
    const auto region = MonitorGpuPanelRegionFor(bounds.left,
                                                  MonitorPageContentTop(bounds.width, g_metrics) - g_monitorScrollY,
                                                  bounds.width,
                                                  rows.size(), fold && g_monitorGpuOpen >= 0);
    const RECT dirty{region.left, region.top, region.right, region.bottom};
    InvalidateRect(h, &dirty, FALSE);
}

void InvalidateMonitorThroughput(HWND h)
{
    if (!h || g_page != MONITOR) return;
    RECT client{};
    if (!GetClientRect(h, &client)) return;
    const int pageX = SidebarContentLeft();
    const auto bounds = MonitorPageBoundsFor(pageX, std::max(0, (int)client.right - pageX - 28));
    const int pageTop = MonitorPageContentTop(bounds.width, g_metrics) - g_monitorScrollY;
    const auto rows = MonitorGpuRows(g_metrics);
    const auto regions = MonitorPageRefreshRegionsFor(bounds.left, pageTop, bounds.width, rows.size(),
        g_monitorGpuOpen >= 0, true, false, false, false, false, false);
    if (!regions.empty())
    {
        const auto &region = regions.front();
        const RECT dirty{region.left, region.top, region.right, region.bottom};
        InvalidateRect(h, &dirty, FALSE);
    }
}

bool UpdateMonitorVramHitExpiryTimer()
{
    if (!g_hwnd) return false;
    const bool fresh = MonitorVramHitTierFresh(g_metrics);
    const bool expired = g_monitorVramHitFresh && !fresh;
    g_monitorVramHitFresh = fresh;
    const auto tiers = MonitorTierNow(g_metrics);
    const auto time = tiers.value("time", Json());
    if (!fresh)
    {
        KillTimer(g_hwnd, 13);
        return expired;
    }
    const double secondsLeft = time.get<double>() + 31.0 - (double)std::time(nullptr);
    const auto milliseconds = (UINT)std::clamp(std::ceil(secondsLeft * 1000.0), 1.0, 60000.0);
    if (!SetTimer(g_hwnd, 13, milliseconds, nullptr))
        InvalidateMonitorThroughput(g_hwnd);
    return expired;
}

void UpdateMonitorView(const Json *previousMetrics = nullptr, const Json *previousHealth = nullptr)
{
    const bool vramHitExpired = UpdateMonitorVramHitExpiryTimer();
    g_monitorPrefillPeak = UpdateMonitorPrefillPeak(g_metrics, g_monitorPrefillPeakState);
    if (g_monitorRequests)
    {
        const auto requestRows = [](const Json &metrics) -> const Json *
        {
            return metrics.is_object() && metrics.contains("requests") ? &metrics["requests"] : nullptr;
        };
        const Json *currentRequests = requestRows(g_metrics);
        const Json *oldRequests = previousMetrics ? requestRows(*previousMetrics) : nullptr;
        const bool requestsChanged = !previousMetrics ||
            (currentRequests ? (!oldRequests || *currentRequests != *oldRequests) : oldRequests != nullptr);
        const int count = currentRequests && currentRequests->is_array()
            ? (int)std::min<size_t>(currentRequests->size(), (size_t)std::numeric_limits<int>::max()) : 0;
        const int previousCount = ListView_GetItemCount(g_monitorRequests);
        if (requestsChanged || previousCount != count)
        {
            ListView_SetItemCountEx(g_monitorRequests, count, LVSICF_NOSCROLL);
            if (count > 0) ListView_RedrawItems(g_monitorRequests, 0, count - 1);
            InvalidateRect(g_monitorRequests, nullptr, FALSE);
            if (g_page == MONITOR && previousCount != count)
            {
                NativeLayoutTransition transition;
                Layout();
            }
        }
    }
    const auto kept = MonitorInteger(g_metrics, "requests_kept");
    if (g_allRequests || kept > 12)
    {
        SetText(g_requestToggle, g_allRequests ? L"Show last 12" : L"Show all " + std::to_wstring(kept));
        ShowWindow(g_requestToggle, SW_SHOW);
    }
    else
        ShowWindow(g_requestToggle, SW_HIDE);
    if (g_page == MONITOR)
    {
        auto changes = previousMetrics
            ? MonitorPageRefreshChangesFor(*previousMetrics, g_metrics,
                previousHealth ? *previousHealth : g_health, g_health)
            : MonitorPageRefreshChanges{true, true, true, true, true, true};
        if (vramHitExpired) changes.throughput = true;
        RECT client{};
        GetClientRect(g_hwnd, &client);
        const int pageX = SidebarContentLeft();
        const auto bounds = MonitorPageBoundsFor(pageX, std::max(0, (int)client.right - pageX - 28));
        const auto rows = MonitorGpuRows(g_metrics);
        const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
        const int monitorContentTop = MonitorPageContentTop(bounds.width, g_metrics);
        const int previousContentTop = previousMetrics
            ? MonitorPageContentTop(bounds.width, *previousMetrics) : monitorContentTop;
        if (previousContentTop != monitorContentTop)
        {
            Layout();
            GetClientRect(g_hwnd, &client);
            const auto updatedBounds = MonitorPageBoundsFor(pageX, std::max(0, (int)client.right - pageX - 28));
            const auto region = MonitorPageLayoutRefreshRegionFor(bounds, updatedBounds, 128, client.bottom);
            const RECT dirty{region.left, region.top, region.right, region.bottom};
            InvalidateRect(g_hwnd, &dirty, FALSE);
        }
        else if (previousMetrics && MonitorWarnings(*previousMetrics) != MonitorWarnings(g_metrics))
        {
            const RECT dirty{bounds.left, 128 - g_monitorScrollY, bounds.left + bounds.width,
                             monitorContentTop - g_monitorScrollY};
            InvalidateRect(g_hwnd, &dirty, FALSE);
        }
        const auto regions = MonitorPageRefreshRegionsFor(
            bounds.left, monitorContentTop - g_monitorScrollY, bounds.width, rows.size(), fold && g_monitorGpuOpen >= 0,
            changes.throughput, changes.rightNow, changes.context, changes.tiers, changes.gpu,
            changes.hardware);
        for (const auto &region : regions)
        {
            const RECT dirty{region.left, region.top, region.right, region.bottom};
            InvalidateRect(g_hwnd, &dirty, FALSE);
        }
    }
}

void ReconcileMonitorGpuRows(size_t previousRows, bool previousExpanded)
{
    const auto rows = MonitorGpuRows(g_metrics);
    const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
    g_monitorGpuOpen = fold ? MonitorGpuOpenIndex(rows, g_monitorGpuOpenKey) : -1;
    if (g_monitorGpuSelected >= (int)rows.size()) g_monitorGpuSelected = -1;
    if (g_page == MONITOR && MonitorPanelGeometryChanged(previousRows, previousExpanded,
                                                         rows.size(), g_monitorGpuOpen >= 0))
        Layout();
}

void ContinueAfterChat()
{
    if (g_busy != J_NONE)
        return;
    if (g_modelSwitchRestartAfterChat)
    {
        g_modelSwitchRestartAfterChat = false;
        SetText(g_status, L"Current answer finished. Maya is restarting to load the selected model…");
        BeginJob(J_RESTART, L"restart");
    }
    else if (g_modelSettingsRestartAfterChat)
    {
        g_modelSettingsRestartAfterChat = false;
        SetText(g_status, L"Current answer finished. Maya is restarting to apply engine settings…");
        BeginJob(J_STATUS);
    }
    else if (g_refreshModelsAfterChat)
    {
        g_refreshModelsAfterChat = false;
        BeginJob(J_MODELS);
    }
}

void RequestModelSwitchToFile(const std::string &file)
{
    if (file.empty() || !g_modelData.contains("models") || !g_modelData["models"].is_array()) return;
    const auto &models = g_modelData["models"];
    const auto target = std::find_if(models.begin(), models.end(), [&](const Json &model) {
        return model.is_object() && model.value("file", std::string()) == file;
    });
    if (target == models.end() || !ModelSwitchEnabled(*target,
            ModelSwitchBlockedByWork(),
            g_modelSwitchRequestInFlight || g_switchRestartWatching)) return;

    const Json model = *target;
    const bool restarts = g_modelData.value("restarts", false);
    std::string current;
    const auto running = std::find_if(models.begin(), models.end(), [](const Json &candidate) {
        return candidate.is_object() && candidate.value("running", false);
    });
    if (running != models.end())
    {
        current = running->value("model", std::string("Model"));
        const auto quant = running->value("quant", std::string());
        if (!quant.empty()) current += " " + quant;
    }
    auto prompt = W(ModelSwitchConfirmationText(model, current, restarts, g_busy == J_CHAT));
    if (!restarts)
        prompt += L"\r\n\r\nWin-Manager will restart Maya after saving this selection so the chosen model loads.";
    if (MessageBoxW(g_hwnd, prompt.c_str(), L"Switch model", MB_YESNO | MB_ICONQUESTION) != IDYES) return;

    auto profile = g_profile;
    g_modelSwitchRequestInFlight = true;
    UpdateLeftoverDetails();
    UpdateModelSwitchButton();
    SetLifecycleButtons(g_online, true);
    std::thread([file, profile] {
        try
        {
            MayaHttp api(Configuration(g_root, profile));
            auto result = api.Post(L"/api/models/switch", {{"file", file}});
            PostDone(J_SWITCH, result);
        }
        catch (const std::exception &e)
        {
            PostDone(J_SWITCH, {}, e.what());
        }
    }).detach();
}

bool SelectManagerProfileForModelSwitch(const Json &result)
{
    const auto file = result.value("to", std::string());
    if (file != "maya-ds4.json" && file != "maya-ds4-abliterated.json")
        return false;
    const auto selected = W(file);
    if (g_profile != selected)
    {
        const auto previous = g_profile;
        g_profile = selected;
        if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
        {
            g_profile = previous;
            return false;
        }
        if (g_profileBox)
            SendMessageW(g_profileBox, CB_SETCURSEL, ChatProfileComboIndex(g_profile), 0);
        g_mcpData = Json::object();
        g_mcpLoadAttempted = false;
        g_api.reset();
    }
    return true;
}

void StartMayaUpdateFromPreflight(HWND owner, bool continueAfterCheck = false, bool autoStart = false)
{
    if (g_busy != J_NONE || g_updateWatching)
    {
        SetText(g_status, L"Wait for the current Maya operation to finish before starting an update.");
        return;
    }
    if (!g_updateChecked || !g_updateInstallVisible)
    {
        g_updateStartAfterCheck = continueAfterCheck;
        g_updateAutoStartAfterCheck = autoStart;
        BeginJob(J_UPDATE_CHECK, L"update-check");
        return;
    }
    g_updateStartAfterCheck = false;
    g_updateAutoStartAfterCheck = false;

    const auto details = g_updateFreshInstall ? W(MayaFreshInstallPreflightText(g_updateCheckResult))
                                               : W(MayaUpdatePreflightText(g_updateCheckResult));
    if (!g_updateCanInstall)
    {
        MessageBoxW(owner, details.c_str(), L"Maya setup/update preflight", MB_OK | MB_ICONWARNING);
        return;
    }
    if (!autoStart)
    {
        const auto prompt = details + L"\r\n\r\n" +
            (g_updateFreshInstall ? L"Launch Maya's setup now?" : L"Start the Maya update now?");
        if (MessageBoxW(owner, prompt.c_str(), g_updateFreshInstall ? L"Install Maya" : L"Install Maya update",
                        MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
            return;
    }
    if (g_updateFreshInstall)
    {
        BeginJob(J_UPDATE_START, L"install-maya");
        return;
    }

    const auto update = g_updateCheckResult.value("update", Json::object());
    const auto latest = update.value("latest", Json::object());
    g_updateTarget = latest.value("version", std::string());
    BeginJob(J_UPDATE_START, L"update-start");
}

void ShowChatModelMenu(HWND owner, HWND anchor)
{
    if (!anchor) return;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    const auto model = g_health.value("model", std::string("model"));
    const auto label = g_health.value("model_label", model);
    const auto version = g_health.value("version", std::string());
    AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 10, W(label).c_str());
    if (!version.empty()) AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 11,
        (L"On this machine · Project Maya v" + W(version)).c_str());

    const bool hasModels = g_modelData.contains("models") && g_modelData["models"].is_array();
    std::vector<std::string> modelFiles;
    if (!hasModels)
    {
        RequestModelsWhenIdle(true);
        AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 12, L"Installed models are loading…");
    }
    else if (g_modelData["models"].empty())
        AppendMenuW(menu, MF_STRING | MF_DISABLED | MF_GRAYED, 12, L"No installed models");
    else
    {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        const auto &models = g_modelData["models"];
        modelFiles.reserve(models.size());
        for (size_t i = 0; i < models.size(); ++i)
        {
            const auto &entry = models[i];
            const auto file = entry.value("file", std::string());
            modelFiles.push_back(file);
            const bool running = entry.value("running", false);
            const bool enabled = !file.empty() && ModelSwitchEnabled(entry,
                ModelSwitchBlockedByWork(),
                g_modelSwitchRequestInFlight || g_switchRestartWatching);
            std::wstring item = running ? L"Current: " : L"Switch to ";
            item += W(NativeChatModelButtonName(entry.value("model", std::string("model"))));
            const auto quant = entry.value("quant", std::string());
            if (!quant.empty()) item += L" · " + W(quant);
            UINT flags = MF_STRING;
            if (running) flags |= MF_CHECKED | MF_GRAYED;
            else if (!enabled) flags |= MF_GRAYED;
            AppendMenuW(menu, flags, NativeChatModelMenuCommandBase + (UINT)i, item.c_str());
        }
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const auto context = (unsigned long long)g_health.value("max_context", 0LL);
    std::wstring contextLabel = L"Context size\t" +
        (context ? std::to_wstring((context + 500) / 1000) + L"K" : L"—");
    AppendMenuW(menu, MF_STRING, 1, contextLabel.c_str());
    const auto sampling = ResolveChatSampling(g_samplingPrefs, model, g_health.value("defaults", Json::object()));
    AppendMenuW(menu, MF_STRING, 2, (L"Sampling\t" + W(ChatSamplingPresetName(sampling))).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    const auto count = hasModels ? std::to_wstring(g_modelData["models"].size()) + L" set up" : L"Switch, settings";
    AppendMenuW(menu, MF_STRING, 3, (L"Models\t" + count).c_str());

    RECT bounds{};
    GetWindowRect(anchor, &bounds);
    g_chatModelMenuOpen = true;
    g_chatModelMenuReopen = false;
    const int selected = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_WORKAREA,
                                             bounds.left, bounds.bottom, 0, owner, nullptr);
    g_chatModelMenuOpen = false;
    const bool reopen = NativeChatModelMenuReopenAfterLoad(g_chatModelMenuReopen, selected) &&
                        g_modelData.contains("models") && g_modelData["models"].is_array();
    g_chatModelMenuReopen = false;
    const int modelIndex = NativeChatModelMenuModelIndexFor(selected, modelFiles.size());
    const std::string selectedFile = modelIndex >= 0 ? modelFiles[(size_t)modelIndex] : std::string();
    DestroyMenu(menu);
    if (reopen)
    {
        ShowChatModelMenu(owner, anchor);
        return;
    }
    if (!selectedFile.empty())
    {
        RequestModelSwitchToFile(selectedFile);
        return;
    }
    switch (NativeChatModelMenuActionFor(selected))
    {
    case NativeChatModelMenuAction::CONTEXT: ShowPage(SETTINGS); break;
    case NativeChatModelMenuAction::SAMPLING: ShowPage(CHAT_DEFAULTS); break;
    case NativeChatModelMenuAction::MODELS: ShowPage(MODELS); break;
    case NativeChatModelMenuAction::NONE: break;
    }
}

LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (g_taskbarCreatedMessage && m == g_taskbarCreatedMessage)
    {
        AddManagerTrayIcon(h);
        return 0;
    }
    if (m == WM_SETCURSOR && (HWND)w == g_detail && LOWORD(l) == HTCLIENT)
    {
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    }
    if (m == WM_GETMINMAXINFO && g_compactMode)
    {
        RECT bounds{0, 0, NativeCompactClientWidth, NativeCompactClientHeight};
        AdjustWindowRectEx(&bounds, (DWORD)GetWindowLongPtrW(h, GWL_STYLE), GetMenu(h) != nullptr,
                           (DWORD)GetWindowLongPtrW(h, GWL_EXSTYLE));
        auto *size = (MINMAXINFO *)l;
        size->ptMinTrackSize.x = bounds.right - bounds.left;
        size->ptMinTrackSize.y = bounds.bottom - bounds.top;
        return 0;
    }
    if (m == WM_DROPFILES)
    {
        auto drop = reinterpret_cast<HDROP>(w);
        POINT point{};
        if (g_page == CHAT && DragQueryPoint(drop, &point))
        {
            ClientToScreen(h, &point);
            RECT transcript{}, chatHeader{};
            if (GetWindowRect(g_transcript, &transcript) && GetWindowRect(g_chatOptions, &chatHeader) &&
                MayaChatDropTarget(point.x, point.y, transcript.left, chatHeader.top))
            {
                std::vector<fs::path> files;
                const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
                for (UINT i = 0; i < count; ++i)
                {
                    const UINT n = DragQueryFileW(drop, i, nullptr, 0);
                    if (!n || n >= 32768) continue;
                    std::wstring path(n + 1, L'\0');
                    if (DragQueryFileW(drop, i, path.data(), n + 1)) { path.resize(n); files.emplace_back(path); }
                }
                AddAttachmentPaths(files);
            }
        }
        DragFinish(drop);
        return 0;
    }
    if (m == WM_CREATE)
    {
        g_hwnd = h;
        try { g_minimizeToTray = NativeMinimizeToTray(); } catch (...) { g_minimizeToTray = false; }
        AddManagerTrayIcon(h);
        DragAcceptFiles(h, TRUE);
        CreateUi();
        LoadChatPreferences();
        Layout();
        UpdateConnectPage();
        LoadFolders();
        LoadChats();
        if (g_chat < 0)
            NewChat();
        else
        {
            RefreshChatList();
            RenderChat();
        }
        SetTimer(h, 1, NativeStatusPollIntervalMs(false), nullptr);
        SetTimer(h, 2, g_metricsPollIntervalMs, nullptr);
        SetTimer(h, 10, 10000, nullptr);
        LoadStatusData();
        return 0;
    }
    if (m == WM_SIZE)
    {
        const UINT pollInterval = NativeStatusPollIntervalMs(w == SIZE_MINIMIZED);
        if (pollInterval != g_statusPollIntervalMs)
        {
            g_statusPollIntervalMs = pollInterval;
            SetTimer(h, 1, g_statusPollIntervalMs, nullptr);
        }
        UpdateMetricsPollTimer(w == SIZE_MINIMIZED, GetForegroundWindow() == h);
        Layout();
        UpdateComposerHeight();
        if (w == SIZE_MINIMIZED && g_minimizeToTray && g_trayIconAdded)
            ShowWindow(h, SW_HIDE);
        return 0;
    }
    if (m == NativeTrayMessageId)
    {
        const UINT event = LOWORD(l);
        if (event == WM_LBUTTONUP || event == WM_LBUTTONDBLCLK)
        {
            ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
        }
        else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU)
        {
            HMENU menu = CreatePopupMenu();
            if (menu)
            {
                AppendMenuW(menu, MF_STRING, NativeTrayOpenId, L"Open Maya Manager");
                AppendMenuW(menu, MF_STRING, NativeTrayCompactId, g_compactMode ? L"Expand Manager" : L"Compact controller");
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(menu, MF_STRING, NativeTrayExitId, L"Exit Maya Manager");
                POINT point{};
                GetCursorPos(&point);
                SetForegroundWindow(h);
                const UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                                   point.x, point.y, 0, h, nullptr);
                DestroyMenu(menu);
                if (choice) SendMessageW(h, WM_COMMAND, MAKEWPARAM(choice, 0), 0);
            }
        }
        return 0;
    }
    if (m == WM_ACTIVATE)
        UpdateMetricsPollTimer(IsIconic(h) != FALSE, LOWORD(w) != WA_INACTIVE);
    if ((g_page == MONITOR || g_page == MODELS) && g_pageScrollLimit > 0 &&
        (m == WM_MOUSEWHEEL || (m == WM_VSCROLL && l == 0) ||
         (m == WM_KEYDOWN && (w == VK_NEXT || w == VK_PRIOR))))
    {
        RECT client{};
        GetClientRect(h, &client);
        int next = CurrentPageScrollPosition();
        if (m == WM_MOUSEWHEEL)
            next -= GET_WHEEL_DELTA_WPARAM(w) * 48 / WHEEL_DELTA;
        else if (m == WM_KEYDOWN)
            next += w == VK_NEXT ? std::max(48, (int)client.bottom - 200) : -std::max(48, (int)client.bottom - 200);
        else
        {
            switch (LOWORD(w))
            {
            case SB_LINEUP: next -= 40; break;
            case SB_LINEDOWN: next += 40; break;
            case SB_PAGEUP: next -= std::max(48, (int)client.bottom - 200); break;
            case SB_PAGEDOWN: next += std::max(48, (int)client.bottom - 200); break;
            default: return 0;
            }
        }
        SetCurrentPageScrollPosition(next);
        return 0;
    }
    if (m == WM_LBUTTONDOWN && g_pageScrollLimit > 0)
    {
        RECT client{};
        GetClientRect(h, &client);
        const int x = GET_X_LPARAM(l), y = GET_Y_LPARAM(l);
        RECT track{client.right - 10, 146, client.right - 4, client.bottom - 18};
        if (PtInRect(&track, POINT{x, y}))
        {
            const auto thumb = PageScrollbarRect(client, CurrentPageScrollPosition(), g_pageScrollLimit);
            if (PtInRect(&thumb, POINT{x, y}))
            {
                g_pageScrollDragging = true;
                g_pageScrollDragOffset = y - thumb.top;
                SetCapture(h);
            }
            else
            {
                const int step = std::max(48, (int)client.bottom - 200);
                SetCurrentPageScrollPosition(CurrentPageScrollPosition() + (y < thumb.top ? -step : step));
            }
            return 0;
        }
    }
    if (m == WM_MOUSEMOVE && g_pageScrollDragging)
    {
        RECT client{};
        GetClientRect(h, &client);
        const RECT thumb = PageScrollbarRect(client, CurrentPageScrollPosition(), g_pageScrollLimit);
        const int trackTop = 146, trackBottom = (int)client.bottom - 18;
        const int travel = std::max(1, trackBottom - trackTop - ((int)thumb.bottom - (int)thumb.top));
        const int thumbTop = std::clamp(GET_Y_LPARAM(l) - g_pageScrollDragOffset,
                                        trackTop, trackTop + travel);
        const int next = (thumbTop - trackTop) * g_pageScrollLimit / travel;
        SetCurrentPageScrollPosition(next);
        return 0;
    }
    if (m == WM_LBUTTONUP && g_pageScrollDragging)
    {
        g_pageScrollDragging = false;
        ReleaseCapture();
        return 0;
    }
    if (m == WM_CAPTURECHANGED && g_pageScrollDragging)
    {
        g_pageScrollDragging = false;
        return 0;
    }
    if ((m == WM_SETTINGCHANGE || m == WM_THEMECHANGED) && g_themeChoice == "system" &&
        NativeSystemThemeChanged(g_systemLightTheme, SystemUsesLightTheme()))
    {
        ApplyAppearance();
        return 0;
    }
    if (m == WM_MOUSEMOVE)
    {
        if (!g_monitorMouseTracking)
        {
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, h, 0};
            g_monitorMouseTracking = TrackMouseEvent(&tracking) != FALSE;
        }
        MonitorGpuSegmentHit hit;
        std::wstring tip;
        POINT point{};
        if (g_page == MONITOR)
        {
            RECT client{};
            GetClientRect(h, &client);
            const int startX = SidebarContentLeft();
            const auto bounds = MonitorPageBoundsFor(startX, std::max(0, (int)client.right - startX - 28));
            const auto rows = MonitorGpuRows(g_metrics);
            hit = MonitorGpuSegmentAt(rows, GET_X_LPARAM(l), GET_Y_LPARAM(l), bounds.left,
                                      MonitorGpuMapTop(MonitorPageContentTop(bounds.width, g_metrics) - g_monitorScrollY,
                                                       bounds.width),
                                      bounds.width, g_monitorGpuOpen);
            if (hit.row >= 0)
            {
                int index = rows[hit.row].value("dev", hit.row);
                if (rows[hit.row].contains("index") && rows[hit.row]["index"].is_number())
                    index = rows[hit.row]["index"].get<int>();
                std::wostringstream label;
                label << L"GPU " << index << L" · " << hit.label << L" · "
                      << MonitorGigabytes(Json(hit.bytes)) << L" · " << std::fixed << std::setprecision(1)
                      << (100.0 * hit.bytes / hit.total) << L"% of the card";
                const auto hint = MonitorGpuSegmentHint(rows[hit.row], g_metrics, hit.key);
                if (!hint.empty()) label << L"\r\n" << hint;
                tip = label.str();
                point = POINT{GET_X_LPARAM(l), GET_Y_LPARAM(l)};
            }
        }
        if (hit.row != g_monitorGpuHover.row || hit.key != g_monitorGpuHover.key)
        {
            g_monitorGpuHover = hit;
            g_monitorGpuHoverText = std::move(tip);
            g_monitorGpuHoverPoint = point;
            InvalidateMonitorGpuPanel(h);
        }
        return DefWindowProcW(h, m, w, l);
    }
    if (m == WM_MOUSELEAVE)
    {
        g_monitorMouseTracking = false;
        if (g_monitorGpuHover.row >= 0)
        {
            g_monitorGpuHover = {};
            g_monitorGpuHoverText.clear();
            InvalidateMonitorGpuPanel(h);
        }
        return 0;
    }
    if (g_page == MONITOR && (m == WM_LBUTTONDOWN || m == WM_KEYDOWN))
    {
        const auto rows = MonitorGpuRows(g_metrics);
        const bool fold = rows.size() > 1 && !rows.front().value("plain", false);
        if (fold && m == WM_LBUTTONDOWN)
        {
            RECT client{};
            GetClientRect(h, &client);
            const int startX = SidebarContentLeft();
            const auto bounds = MonitorPageBoundsFor(startX, std::max(0, (int)client.right - startX - 28));
            const int index = MonitorGpuRowAt(GET_X_LPARAM(l), GET_Y_LPARAM(l), bounds.left,
                                              MonitorGpuMapTop(MonitorPageContentTop(bounds.width, g_metrics) - g_monitorScrollY,
                                                               bounds.width),
                                              bounds.width, rows.size(), g_monitorGpuOpen);
            if (index >= 0)
            {
                g_monitorGpuSelected = index;
                SetMonitorGpuOpen(index);
                SetFocus(h);
                {
                    NativeLayoutTransition transition;
                    Layout();
                }
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
        if (fold && m == WM_KEYDOWN && GetFocus() == h)
        {
            if (w == VK_DOWN || w == VK_UP)
            {
                if (g_monitorGpuSelected < 0) g_monitorGpuSelected = g_monitorGpuOpen >= 0 ? g_monitorGpuOpen : 0;
                else g_monitorGpuSelected = (g_monitorGpuSelected + (w == VK_DOWN ? 1 : (int)rows.size() - 1)) % (int)rows.size();
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
            if ((w == VK_RETURN || w == VK_SPACE) && g_monitorGpuSelected >= 0 &&
                g_monitorGpuSelected < (int)rows.size())
            {
                SetMonitorGpuOpen(g_monitorGpuSelected);
                {
                    NativeLayoutTransition transition;
                    Layout();
                }
                InvalidateRect(h, nullptr, FALSE);
                return 0;
            }
        }
    }
    if (m == WM_CONTEXTMENU && (HWND)w == g_chatList)
    {
        ShowChatMenu(h, l);
        return 0;
    }
    if (m == WM_CONTEXTMENU && (HWND)w == g_transcript)
    {
        ShowCodeMenu(h, l);
        return 0;
    }
    if (m == WM_ERASEBKGND)
    {
        // The parent paints its background through WM_PAINT's back buffer on every page.
        // A separate erase pass between child visibility changes makes page navigation flash.
        return 1;
    }
    if (m == WM_PAINT)
    {
        PAINTSTRUCT ps{};
        HDC target = BeginPaint(h, &ps);
        RECT r{};
        GetClientRect(h, &r);
        RECT dirty{};
        const bool hasDirty = IntersectRect(&dirty, &ps.rcPaint, &r) != FALSE;
        const int dirtyWidth = hasDirty ? dirty.right - dirty.left : 0;
        const int dirtyHeight = hasDirty ? dirty.bottom - dirty.top : 0;
        HDC buffer = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ previousBitmap = nullptr;
        HDC dc = target;
        if (dirtyWidth > 0 && dirtyHeight > 0)
        {
            buffer = CreateCompatibleDC(target);
            bitmap = CreateCompatibleBitmap(target, dirtyWidth, dirtyHeight);
            if (buffer && bitmap)
            {
                previousBitmap = SelectObject(buffer, bitmap);
                if (previousBitmap && previousBitmap != HGDI_ERROR &&
                    SetWindowOrgEx(buffer, dirty.left, dirty.top, nullptr))
                    dc = buffer;
                else
                {
                    if (previousBitmap && previousBitmap != HGDI_ERROR) SelectObject(buffer, previousBitmap);
                    DeleteObject(bitmap);
                    DeleteDC(buffer);
                    buffer = nullptr;
                    bitmap = nullptr;
                    previousBitmap = nullptr;
                }
            }
            else
            {
                if (bitmap) DeleteObject(bitmap);
                if (buffer) DeleteDC(buffer);
                buffer = nullptr;
                bitmap = nullptr;
            }
        }
        FillRect(dc, &r, g_bgBrush);
        RECT side{0, 0, SidebarWidth(), r.bottom};
        FillRect(dc, &side, g_sideBrush);
        const int pageX = SidebarContentLeft();
        const int contentX = SidebarWidth() + 24;
        const int bodyY = 128;
        const int bodyW = std::max(0, (int)r.right - SidebarWidth() - 48);
        const int bodyH = std::max(0, (int)r.bottom - bodyY - 22);
        if (g_page == CHAT)
        {
            const auto chatColumn = NativeChatColumnFor(contentX, bodyW, 220, 768);
            const int composerTop = bodyY + bodyH - g_inputHeight - 52;
            DrawNeonPanel(dc, {chatColumn.left - 4, bodyY + 34, chatColumn.left + chatColumn.width + 4,
                               std::max(bodyY + 40, composerTop - 50)}, C_CARD_EDGE, 16);
            DrawNeonPanel(dc, {chatColumn.left, composerTop - 42, chatColumn.left + chatColumn.width,
                               bodyY + bodyH - 4}, C_CYAN, 16);
        }
        else if (g_page == SETTINGS)
        {
            const int leftWidth = std::max(1, bodyW * 54 / 100);
            const int mcpLeft = contentX + leftWidth + 14;
            const int mcpWidth = std::max(1, bodyW - leftWidth - 14);
            DrawNeonPanel(dc, {contentX, bodyY, contentX + leftWidth, bodyY + 282}, C_CYAN, 16);
            DrawNeonPanel(dc, {contentX, bodyY + 292, contentX + leftWidth, bodyY + 498}, C_CARD_EDGE, 16);
            DrawNeonPanel(dc, {mcpLeft, bodyY + 292, mcpLeft + mcpWidth, bodyY + 494}, C_CYAN, 16);
            DrawNeonPanel(dc, {contentX, bodyY + 500, contentX + bodyW, (int)r.bottom - 8}, C_CARD_EDGE, 16);
        }
        else if (g_page == CHAT_DEFAULTS)
        {
            DrawNeonPanel(dc, {contentX, bodyY + 38, contentX + bodyW, bodyY + 136}, C_CARD_EDGE, 16);
            const int sampleGap = 24, sampleWidth = std::max(1, (bodyW - 2 * sampleGap) / 3);
            for (int i = 0; i < 9; ++i)
            {
                const int col = i % 3, row = i / 3;
                const int sx = contentX + col * (sampleWidth + sampleGap), sy = bodyY + 148 + row * 120;
                DrawNeonPanel(dc, {sx, sy - 7, sx + sampleWidth, sy + 72}, C_CARD_EDGE, 12);
            }
            DrawNeonPanel(dc, {contentX, bodyY + bodyH - 86, contentX + bodyW, bodyY + bodyH - 8}, C_CARD_EDGE, 14);
        }
        else if (g_page == CONNECT)
        {
            for (int i = 0; i < 3; ++i)
            {
                const int cy = bodyY + 42 + i * 80;
                DrawNeonPanel(dc, {contentX, cy - 8, contentX + bodyW, cy + 68}, C_CARD_EDGE, 14);
            }
            DrawNeonPanel(dc, {contentX, bodyY + 280, contentX + bodyW, bodyY + 360}, C_CARD_EDGE, 14);
            DrawNeonPanel(dc, {contentX, bodyY + 364, contentX + bodyW, bodyY + 440}, C_CYAN, 14);
            DrawNeonPanel(dc, {contentX, bodyY + 448, contentX + bodyW, (int)r.bottom - 8}, C_CARD_EDGE, 14);
        }
        else if (g_page == ABOUT)
        {
            const bool wide = bodyW >= 760;
            const int teamHeight = 86;
            const int rowY = bodyY + teamHeight + 12;
            const int rowHeight = std::min(320, std::max(272, bodyH - 548));
            const int factsWidth = wide ? bodyW * 57 / 100 : bodyW;
            const int authorX = wide ? contentX + factsWidth + 14 : contentX;
            const int authorY = wide ? rowY : rowY + rowHeight + 10;
            const int authorWidth = wide ? bodyW - factsWidth - 14 : bodyW;
            const int authorHeight = wide ? rowHeight : 132;
            DrawNeonPanel(dc, {contentX, bodyY, contentX + bodyW, bodyY + teamHeight}, C_CARD_EDGE, 16);
            DrawNeonPanel(dc, {contentX, rowY, contentX + factsWidth, rowY + rowHeight}, C_CARD_EDGE, 16);
            DrawNeonPanel(dc, {authorX, authorY, authorX + authorWidth, authorY + authorHeight}, C_CYAN, 16);
            const int rowsBottom = wide ? rowY + rowHeight : authorY + authorHeight;
            const int updateY = rowsBottom + 12;
        const int changelogHeight = g_aboutUpdateCardHeight;
            DrawNeonPanel(dc, {contentX, updateY, contentX + bodyW, updateY + changelogHeight}, C_CARD_EDGE, 16);
            if (g_aboutLogVisible)
                DrawNeonPanel(dc, {contentX, updateY + changelogHeight + 72, contentX + bodyW, (int)r.bottom - 8}, C_CARD_EDGE, 16);
        DrawNeonPanel(dc, {contentX, updateY + changelogHeight + 12,
                           contentX + bodyW, updateY + changelogHeight + 60}, C_CYAN, 16);
        }
        else if (g_page == DATA)
        {
            DrawNeonPanel(dc, {contentX, bodyY, contentX + bodyW, bodyY + 188}, C_CARD_EDGE, 16);
        }
        if (g_page == SAVINGS)
        {
            const int pricingTop = bodyY + bodyH - 166;
            DrawNeonPanel(dc, {contentX, pricingTop, contentX + bodyW, bodyY + bodyH - 8}, C_CARD_EDGE, 16);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_MUTED);
            SelectObject(dc, g_small);
            RECT title{contentX + 16, pricingTop + 7, contentX + bodyW - 16, pricingTop + 24};
            DrawTextW(dc, L"MODEL PRICING · USD PER 1M TOKENS", -1, &title, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
        }
        if (g_page == MONITOR)
        {
            const auto bounds = MonitorPageBoundsFor(pageX, std::max(0, (int)r.right - pageX - 28));
            const int w = bounds.width;
            DrawMonitorBanners(dc, bounds.left, w, g_metrics, g_monitorScrollY);
            int x = bounds.left, y = MonitorPageContentTopFor(dc, w, g_metrics) - g_monitorScrollY;
            const auto sections = MonitorPageSectionsFor(y, w);
            const int gap = 16;
            const int throughputWidth = sections.stacked ? w : 520 + std::max(0, w - gap - 520 - 300) * 2 / 3;
            const int rightNowX = sections.stacked ? x : x + throughputWidth + gap;
            const int rightNowWidth = sections.stacked ? w : std::max(1, w - throughputWidth - gap);
            const RECT throughputCard{x, y, x + throughputWidth, y + sections.throughputHeight};
            const RECT rightNowCard{rightNowX, sections.rightNowTop, rightNowX + rightNowWidth,
                                    sections.rightNowTop + sections.rightNowHeight};
            DrawCard(dc, throughputCard, L"Throughput", L"", L"");
            DrawMonitorRightNow(dc, rightNowCard);

            const auto liveValue = g_metrics.value("live", Json::object());
            const auto live = liveValue.is_object() ? liveValue : Json::object();
            const auto requests = g_metrics.value("requests", Json::array());
            const bool hasLast = requests.is_array() && !requests.empty() && requests[0].is_object();
            const auto decode = MonitorHeroValue(g_metrics, g_health, "decode");
            const auto prefill = g_monitorPrefillPeak.visible ? MonitorPrefillPeakValue(g_monitorPrefillPeak)
                                                               : MonitorHeroValue(g_metrics, g_health, "prefill");
            const auto peak = MonitorPeakDecodeValue(g_metrics);
            const auto state = MonitorText(live, "state", L"idle");
            const auto decodeHint = MonitorInteger(live, "active") > 1
                ? MonitorNumberLabel((double)MonitorInteger(live, "writing")) + L" writing"
                : state == L"generating" ? L"writing now" : state == L"reading" ? L"reading the prompt"
                : hasLast ? L"last request" : L"waiting for a request";
            const auto prefillHint = state == L"reading" ? L"reading now"
                : g_monitorPrefillPeak.visible ? g_monitorPrefillPeak.label
                : state != L"idle" ? L"this request" : hasLast ? L"last request" : L"waiting";
            const int innerLeft = throughputCard.left + 18;
            const int speedGap = 18;
            const int speedWidth = std::max(1, (throughputWidth - 54 - speedGap) / 2);
            for (int i = 0; i < 2; ++i)
            {
                const int sx = innerLeft + i * (speedWidth + speedGap);
                RECT labelRect{sx, y + 39, sx + speedWidth, y + 56};
                RECT valueRect{sx, y + 55, sx + speedWidth, y + 88};
                RECT noteRect{sx, y + 90, sx + speedWidth, y + 106};
                SetBkMode(dc, TRANSPARENT);
                SetTextColor(dc, C_MUTED);
                SelectObject(dc, g_small);
                const std::wstring label = i == 0 ? L"Decode · " + decodeHint : L"Prefill · " + prefillHint;
                DrawTextW(dc, label.c_str(), -1, &labelRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
                SetTextColor(dc, i == 0 ? C_TEXT : C_CYAN_TEXT);
                SelectObject(dc, g_bold);
                const std::wstring value = (i == 0 ? decode : prefill) + L" tok/s";
                DrawTextW(dc, value.c_str(), -1, &valueRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                SetTextColor(dc, C_MUTED);
                SelectObject(dc, g_small);
                const std::wstring note = i == 0 ? L"Peak " + peak + L" tok/s" : L"reused tokens excluded";
                DrawTextW(dc, note.c_str(), -1, &noteRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, g_small);
            const int legendY = y + 121;
            HPEN decodeLegend = CreatePen(PS_SOLID, 2, C_PURPLE);
            HGDIOBJ oldLegendPen = SelectObject(dc, decodeLegend);
            MoveToEx(dc, x + 18, legendY, nullptr);
            LineTo(dc, x + 38, legendY);
            SelectObject(dc, oldLegendPen);
            DeleteObject(decodeLegend);
            SetTextColor(dc, C_PURPLE_TEXT);
            RECT decodeLegendText{x + 44, y + 112, x + 104, y + 130};
            DrawTextW(dc, L"Decode", -1, &decodeLegendText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            HPEN prefillLegend = CreatePen(PS_DASH, 2, C_CYAN);
            oldLegendPen = SelectObject(dc, prefillLegend);
            MoveToEx(dc, x + 116, legendY, nullptr);
            LineTo(dc, x + 136, legendY);
            SelectObject(dc, oldLegendPen);
            DeleteObject(prefillLegend);
            SetTextColor(dc, C_CYAN_TEXT);
            RECT prefillLegendText{x + 142, y + 112, x + 202, y + 130};
            DrawTextW(dc, L"Prefill", -1, &prefillLegendText, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            SetTextColor(dc, C_MUTED);
            RECT periodLabel{x + throughputWidth - 126, y + 112,
                             x + throughputWidth - 18, y + 130};
            DrawTextW(dc, L"Last minute", -1, &periodLabel, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
            const RECT chart{x + 18, y + 132, x + throughputWidth - 18, y + 260};
            DrawMonitorSparkline(dc, chart, "tok_s", C_PURPLE, PS_SOLID, true);
            DrawMonitorSparkline(dc, chart, "prefill_tok_s_mean", C_CYAN, PS_DASH, false);
            DrawMonitorStats(dc, x, y + 264, throughputWidth, g_metrics, r.right <= 767);
            const RECT contextCard{x, sections.contextTop, x + w, sections.contextTop + sections.contextHeight};
            DrawMonitorContextCard(dc, contextCard);
            DrawMonitorTierBar(dc, x, sections.tiersTop, w, g_metrics);
            const int gpuPanel = MonitorGpuPanelHeight(MonitorGpuRows(g_metrics).size(), g_monitorGpuOpen >= 0);
            const int gpuMapY = sections.gpuMapTop;
            DrawMonitorGpuMap(dc, x, gpuMapY, w, g_metrics, g_monitorGpuOpen, g_monitorGpuSelected,
                              g_monitorGpuHover, g_monitorGpuHoverText, g_monitorGpuHoverPoint);
            DrawMonitorHardwareHistory(dc, x, gpuMapY + gpuPanel + 8, w, g_metrics);
        }
        if (g_page == SETTINGS)
        {
            const int leftWidth = std::max(1, bodyW * 54 / 100);
            RECT mcp{pageX + leftWidth + 28, bodyY + 298, r.right - 24, bodyY + 320};
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, C_MUTED);
            DrawTextW(dc, L"MCP SERVERS AND TOOLS", -1, &mcp, DT_SINGLELINE);
        }
        DrawPageScrollbar(dc, r);
        if (buffer)
        {
            BitBlt(target, dirty.left, dirty.top, dirtyWidth, dirtyHeight,
                   buffer, dirty.left, dirty.top, SRCCOPY);
            SelectObject(buffer, previousBitmap);
            DeleteObject(bitmap);
            DeleteDC(buffer);
        }
        EndPaint(h, &ps);
        return 0;
    }
    if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLOREDIT || m == WM_CTLCOLORLISTBOX || m == WM_CTLCOLORBTN)
    {
        HDC dc = (HDC)w;
        const HWND control = (HWND)l;
        bool mutedLabel = control == g_themeLabel || control == g_schemeLabel || control == g_contextLabel ||
            control == g_portLabel || control == g_budgetLabel || control == g_budgetHint ||
            control == g_defaultInstructionsLabel || control == g_connectKeyLabel ||
            control == g_connectExampleLabel || control == g_connectInfo || control == g_connectAuth ||
            control == g_settingsStatus || control == g_sharedDefaultsSummary || control == g_reasonValue ||
            control == g_mayaProjectCredit || control == g_updateSummary || control == g_releaseNotes ||
            control == g_modelSettingsHint || control == g_modelSettingsEnvInfo || control == g_catalogDescription;
        for (auto label : g_samplingLabels) mutedLabel = mutedLabel || control == label;
        for (auto label : g_connectTitle) mutedLabel = mutedLabel || control == label;
        for (auto label : g_priceLabels) mutedLabel = mutedLabel || control == label;
        SetTextColor(dc, mutedLabel ? C_MUTED : C_TEXT);
        if (m == WM_CTLCOLORSTATIC && control == g_detail) SetTextColor(dc, C_CYAN_TEXT);
        if (m == WM_CTLCOLORSTATIC && g_modelSettingsOpen)
            for (const auto &field : g_modelSettingFields)
                if (field.label == control && field.changed)
                {
                    SetTextColor(dc, C_PURPLE_TEXT);
                    break;
                }
        if (m == WM_CTLCOLOREDIT)
        {
            SetBkColor(dc, C_CARD2);
            return (LRESULT)g_fieldBrush;
        }
        SetBkColor(dc, C_CARD);
        return (LRESULT)g_cardBrush;
    }
    if (m == WM_MEASUREITEM)
    {
        auto measure = (MEASUREITEMSTRUCT *)l;
        if (measure->CtlType == ODT_LISTBOX && measure->CtlID == 140)
        {
            measure->itemHeight = 118;
            return TRUE;
        }
        if (measure->CtlType == ODT_LISTBOX && measure->CtlID == 141)
        {
            measure->itemHeight = 88;
            return TRUE;
        }
        if (measure->CtlID == 202 && measure->CtlType == ODT_LISTVIEW)
        {
            measure->itemHeight = MonitorRequestCardHeight;
            return TRUE;
        }
    }
    if (m == WM_DRAWITEM)
    {
        auto d = (DRAWITEMSTRUCT *)l;
        if (d->CtlType == ODT_STATIC && d->CtlID == NativeHeaderLayerId)
        {
            DrawManagerHeader(d->hDC, d->rcItem.right, d->rcItem.bottom);
            return TRUE;
        }
        if (d->CtlType == ODT_LISTBOX && (d->CtlID == 140 || d->CtlID == 141))
        {
            DrawModelListCard(*d, d->CtlID == 141);
            return TRUE;
        }
        if (d->CtlType == ODT_STATIC && d->CtlID == 152)
        {
            DrawSavingsCards(*d);
            return TRUE;
        }
        if (d->CtlID == 202 && d->CtlType == ODT_LISTVIEW)
        {
            DrawMonitorRequestCard(*d);
            return TRUE;
        }
        if (d->CtlID == 110 && d->CtlType == ODT_COMBOBOX)
        {
            const bool field = (d->itemState & ODS_COMBOBOXEDIT) != 0;
            const bool selected = (d->itemState & ODS_SELECTED) != 0;
            HBRUSH brush = CreateSolidBrush(field ? C_CARD2 : selected ? C_OPEN : C_CARD);
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            if (field)
            {
                HPEN pen = CreatePen(PS_SOLID, 1, C_LINE);
                HGDIOBJ oldPen = SelectObject(d->hDC, pen);
                HGDIOBJ oldBrush = SelectObject(d->hDC, GetStockObject(HOLLOW_BRUSH));
                RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 8, 8);
                SelectObject(d->hDC, oldBrush);
                SelectObject(d->hDC, oldPen);
                DeleteObject(pen);
            }
            wchar_t label[256]{};
            if (d->itemID == (UINT)-1) GetWindowTextW(g_profileBox, label, (int)std::size(label));
            else SendMessageW(g_profileBox, CB_GETLBTEXT, d->itemID, (LPARAM)label);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, (d->itemState & ODS_DISABLED) ? C_MUTED : C_TEXT);
            SelectObject(d->hDC, g_font);
            RECT text = d->rcItem;
            text.left += field ? 12 : 8;
            text.right -= field ? 30 : 8;
            DrawTextW(d->hDC, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (field)
            {
                HPEN arrow = CreatePen(PS_SOLID, 2, C_PURPLE_TEXT);
                HGDIOBJ oldPen = SelectObject(d->hDC, arrow);
                const int x = d->rcItem.right - 15, y = (d->rcItem.top + d->rcItem.bottom) / 2;
                MoveToEx(d->hDC, x - 3, y - 1, nullptr);
                LineTo(d->hDC, x, y + 2);
                LineTo(d->hDC, x + 3, y - 1);
                SelectObject(d->hDC, oldPen);
                DeleteObject(arrow);
            }
            if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
            return TRUE;
        }
        if (d->CtlType == ODT_COMBOBOX && d->hwndItem != g_profileBox)
        {
            const bool field = (d->itemState & ODS_COMBOBOXEDIT) != 0 || d->itemID == (UINT)-1;
            const bool selected = (d->itemState & ODS_SELECTED) != 0;
            const bool disabled = (d->itemState & ODS_DISABLED) != 0;
            HBRUSH brush = CreateSolidBrush(disabled ? C_SIDE : field ? C_CARD2 : selected ? C_OPEN : C_CARD);
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            if (field)
            {
                HPEN pen = CreatePen(PS_SOLID, (d->itemState & ODS_FOCUS) ? 2 : 1,
                                     (d->itemState & ODS_FOCUS) ? C_PURPLE : C_LINE);
                HGDIOBJ oldPen = SelectObject(d->hDC, pen);
                HGDIOBJ oldBrush = SelectObject(d->hDC, GetStockObject(HOLLOW_BRUSH));
                RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 8, 8);
                SelectObject(d->hDC, oldBrush);
                SelectObject(d->hDC, oldPen);
                DeleteObject(pen);
            }
            wchar_t label[512]{};
            if (d->itemID == (UINT)-1) GetWindowTextW(d->hwndItem, label, (int)std::size(label));
            else SendMessageW(d->hwndItem, CB_GETLBTEXT, d->itemID, (LPARAM)label);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, disabled ? C_MUTED : selected ? C_PURPLE_TEXT : C_TEXT);
            SelectObject(d->hDC, g_font);
            RECT text = d->rcItem;
            text.left += field ? 12 : 10;
            text.right -= field ? 30 : 10;
            DrawTextW(d->hDC, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (field)
            {
                HPEN arrow = CreatePen(PS_SOLID, 2, C_PURPLE_TEXT);
                HGDIOBJ oldPen = SelectObject(d->hDC, arrow);
                const int x = d->rcItem.right - 15, y = (d->rcItem.top + d->rcItem.bottom) / 2;
                MoveToEx(d->hDC, x - 3, y - 1, nullptr);
                LineTo(d->hDC, x, y + 2);
                LineTo(d->hDC, x + 3, y - 1);
                SelectObject(d->hDC, oldPen);
                DeleteObject(arrow);
            }
            return TRUE;
        }
        const bool modelTab = d->CtlID >= 282 && d->CtlID <= 285;
        if (modelTab || d->CtlID == 203)
        {
            const int index = modelTab ? (int)d->CtlID - 282 : -1;
            const bool selected = modelTab && index == g_modelSettingsTab;
            const auto marker = modelTab ? g_modelSettingsTabMarkers[index]
                : g_modelSettingsEnvironmentChanged ? NativeModelSettingsTabMarker::CHANGED
                                                    : NativeModelSettingsTabMarker::NONE;
            const wchar_t *label = modelTab ? ModelSettingGroupNames[index]
                : g_modelSettingsAdvanced ? L"Show native fields" : L"Show Advanced JSON";
            HBRUSH brush = CreateSolidBrush(C_CARD);
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            HPEN pen = CreatePen(PS_SOLID, modelTab && selected ? 2 : 1,
                                 modelTab ? (selected ? C_PURPLE : C_LINE) : C_LINE);
            HPEN oldPen = (HPEN)SelectObject(d->hDC, pen);
            if (modelTab)
            {
                const int indicatorY = d->rcItem.bottom - (selected ? 2 : 1);
                MoveToEx(d->hDC, d->rcItem.left, indicatorY, nullptr);
                LineTo(d->hDC, d->rcItem.right, indicatorY);
            }
            else
            {
                HBRUSH oldBrush = (HBRUSH)SelectObject(d->hDC, GetStockObject(HOLLOW_BRUSH));
                RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 8, 8);
                SelectObject(d->hDC, oldBrush);
            }
            SelectObject(d->hDC, oldPen);
            DeleteObject(pen);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, modelTab ? (selected ? C_TEXT : C_MUTED) : C_SOFT);
            SelectObject(d->hDC, modelTab && selected ? g_bold : g_font);
            RECT text = d->rcItem;
            text.left += 12;
            if (marker != NativeModelSettingsTabMarker::NONE) text.right -= 22;
            DrawTextW(d->hDC, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (marker != NativeModelSettingsTabMarker::NONE)
            {
                const COLORREF color = marker == NativeModelSettingsTabMarker::WARNING ? C_WARN : C_PURPLE;
                RECT dot{d->rcItem.right - 16, d->rcItem.top + (d->rcItem.bottom - d->rcItem.top - 8) / 2,
                         d->rcItem.right - 8, d->rcItem.top + (d->rcItem.bottom - d->rcItem.top + 8) / 2};
                HBRUSH dotBrush = CreateSolidBrush(color);
                HPEN dotPen = CreatePen(PS_SOLID, 1, color);
                HBRUSH oldDotBrush = (HBRUSH)SelectObject(d->hDC, dotBrush);
                HPEN oldDotPen = (HPEN)SelectObject(d->hDC, dotPen);
                Ellipse(d->hDC, dot.left, dot.top, dot.right, dot.bottom);
                SelectObject(d->hDC, oldDotBrush);
                SelectObject(d->hDC, oldDotPen);
                DeleteObject(dotBrush);
                DeleteObject(dotPen);
            }
            if (d->itemState & ODS_FOCUS)
            {
                RECT focus = d->rcItem;
                InflateRect(&focus, -3, -3);
                DrawFocusRect(d->hDC, &focus);
            }
            return TRUE;
        }
        if (d->CtlID == NativeStatusCardId)
        {
            HBRUSH fill = CreateSolidBrush(C_CARD2);
            HPEN border = CreatePen(PS_SOLID, 1, C_LINE);
            HGDIOBJ oldBrush = SelectObject(d->hDC, fill);
            HGDIOBJ oldPen = SelectObject(d->hDC, border);
            RoundRect(d->hDC, d->rcItem.left, d->rcItem.top, d->rcItem.right, d->rcItem.bottom, 12, 12);
            SelectObject(d->hDC, oldBrush);
            SelectObject(d->hDC, oldPen);
            DeleteObject(fill);
            DeleteObject(border);
            const COLORREF stateColor = g_busy == J_RESTART ? C_WARN : g_online ? C_GREEN : C_MUTED;
            HBRUSH dot = CreateSolidBrush(stateColor);
            HPEN dotPen = CreatePen(PS_SOLID, 1, stateColor);
            HGDIOBJ oldDotBrush = SelectObject(d->hDC, dot);
            HGDIOBJ oldDotPen = SelectObject(d->hDC, dotPen);
            const int centerY = (d->rcItem.top + d->rcItem.bottom) / 2;
            Ellipse(d->hDC, d->rcItem.left + 10, centerY - 4, d->rcItem.left + 18, centerY + 4);
            SelectObject(d->hDC, oldDotBrush);
            SelectObject(d->hDC, oldDotPen);
            DeleteObject(dot);
            DeleteObject(dotPen);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, C_TEXT);
            SelectObject(d->hDC, g_font);
            RECT text = d->rcItem;
            text.left += 26;
            wchar_t label[512]{};
            GetWindowTextW(d->hwndItem, label, (int)std::size(label));
            DrawTextW(d->hDC, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (d->itemState & ODS_FOCUS)
            {
                RECT focus = d->rcItem;
                InflateRect(&focus, -1, -1);
                DrawFocusRect(d->hDC, &focus);
            }
            return TRUE;
        }
        if (d->CtlID >= 5000 && d->CtlID < 5016)
        {
            const int index = (int)d->CtlID - 5000;
            const bool selected = index == g_monitorGpuOpen;
            HBRUSH brush = CreateSolidBrush(selected ? C_CARD2 : C_CARD);
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, selected ? C_TEXT : C_SOFT);
            SelectObject(d->hDC, g_small);
            RECT text = d->rcItem;
            text.left += 2;
            wchar_t label[256]{};
            GetWindowTextW(d->hwndItem, label, (int)std::size(label));
            DrawTextW(d->hDC, label, -1, &text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            if (d->itemState & ODS_FOCUS)
            {
                RECT focus = d->rcItem;
                InflateRect(&focus, -1, -1);
                DrawFocusRect(d->hDC, &focus);
            }
            return TRUE;
        }
        if (d->CtlID == 120)
        {
            const bool selected = (d->itemState & ODS_SELECTED) != 0;
            HBRUSH brush = CreateSolidBrush(C_SIDE);
            FillRect(d->hDC, &d->rcItem, brush);
            DeleteObject(brush);
            if (d->itemID >= g_visibleChats.size()) return TRUE;
            RECT card = d->rcItem;
            InflateRect(&card, -3, -2);
            DrawNeonPanel(d->hDC, card, selected ? C_CYAN : C_CARD_EDGE, 12);
            if (selected)
            {
                RECT inset = card;
                InflateRect(&inset, -3, -3);
                HBRUSH selectedFill = CreateSolidBrush(C_OPEN);
                HGDIOBJ previous = SelectObject(d->hDC, selectedFill);
                RoundRect(d->hDC, inset.left, inset.top, inset.right, inset.bottom, 9, 9);
                SelectObject(d->hDC, previous);
                DeleteObject(selectedFill);
            }
            wchar_t label[512]{};
            SendMessageW(g_chatList, LB_GETTEXT, d->itemID, (LPARAM)label);
            const auto &row = g_visibleChats[d->itemID];
            SetBkMode(d->hDC, TRANSPARENT);
            RECT title = d->rcItem;
            title.left += 10; title.right -= 6;
            if (row.chat < 0)
            {
                SetTextColor(d->hDC, C_MUTED);
                SelectObject(d->hDC, g_bold);
                DrawTextW(d->hDC, label, -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            else
            {
                title.top += 3; title.bottom = title.top + 20;
                SetTextColor(d->hDC, selected ? C_PURPLE_TEXT : C_TEXT);
                SelectObject(d->hDC, g_font);
                DrawTextW(d->hDC, label, -1, &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                RECT snippet = d->rcItem;
                snippet.left += 10; snippet.right -= 6; snippet.top += 21;
                SetTextColor(d->hDC, C_MUTED);
                SelectObject(d->hDC, g_small);
                DrawTextW(d->hDC, row.snippet.c_str(), -1, &snippet,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
            return TRUE;
        }
        bool nav = d->CtlID >= 101 && d->CtlID <= 109, sel = nav && d->CtlID == (int)g_page + 101;
        wchar_t label[256]{};
        GetWindowTextW(d->hwndItem, label, (int)std::size(label));
        const bool authorLink = d->CtlID == NativeAuthorGithubId || d->CtlID == NativeAuthorHuggingFaceId ||
            d->CtlID == NativeMayaCreatorGithubId || d->CtlID == NativeMayaCreatorHuggingFaceId ||
            d->CtlID == NativeMayaComaintainerGithubId || d->CtlID == NativeMayaVersionReleaseId;
        const bool githubBadge = d->CtlID == NativeAuthorGithubId || d->CtlID == NativeMayaCreatorGithubId ||
            d->CtlID == NativeMayaComaintainerGithubId || d->CtlID == NativeMayaVersionReleaseId;
        const bool composerButton = d->CtlID == 122 || d->CtlID == 123 || d->CtlID == 265 ||
                                    d->CtlID == NativeChatReasoningControlId || d->CtlID == 125;
        const bool pressed = (d->itemState & ODS_SELECTED) != 0;
        const bool runningStart = d->CtlID == 111 && wcscmp(label, L"Running") == 0;
        const bool primaryStart = d->CtlID == 111 && !runningStart;
        const bool stopAction = d->CtlID == 112;
        const bool restartAction = d->CtlID == 113;
        const bool updateAction = d->CtlID == 114;
        const bool safePriceAction = d->CtlID == 160 || d->CtlID == 161;
        const bool dangerAction = !safePriceAction && (stopAction || restartAction || d->CtlID == 125 ||
            (wcsstr(label, L"Save") != nullptr) || (wcsstr(label, L"Apply") != nullptr) ||
            (wcsstr(label, L"Reset") != nullptr) || (wcsstr(label, L"Delete") != nullptr) ||
            (wcsstr(label, L"Clear") != nullptr) || (wcsstr(label, L"Install") != nullptr) ||
            (wcsstr(label, L"Switch") != nullptr) || (wcsstr(label, L"switch") != nullptr));
        const bool compactAction = d->CtlID == NativeCompactToggleId;
        const bool sidebarToggle = d->CtlID == 263;
        const bool modelSidebar = d->CtlID == NativeModelSidebarId;
        const bool controllerAction = primaryStart || runningStart || stopAction || restartAction || updateAction || compactAction;
        const bool disabled = (d->itemState & ODS_DISABLED) != 0;
        const bool hovered = g_hoverButton == d->hwndItem && !disabled;
        COLORREF fill = primaryStart ? C_PURPLE : runningStart ? C_CARD2 : authorLink ? C_CARD2
            : sel ? C_OPEN : d->itemState & ODS_SELECTED ? C_CARD2 : C_SIDE;
        if (composerButton && pressed) fill = C_OPEN;
        if (hovered)
            fill = primaryStart ? RGB(158, 144, 255) : runningStart ? RGB(39, 67, 59)
                 : dangerAction ? RGB(66, 38, 48) : RGB(70, 58, 28);
        if (pressed && !disabled && !runningStart && !primaryStart)
            fill = dangerAction ? RGB(79, 35, 45) : hovered ? RGB(82, 68, 31) : C_CARD2;
        if (disabled && controllerAction && !runningStart) fill = C_SIDE;
        COLORREF outline = composerButton && pressed ? C_CYAN
            : runningStart ? C_GREEN : authorLink ? C_CYAN : modelSidebar ? C_CARD_EDGE : nav && sel ? C_CYAN
            : primaryStart ? C_PURPLE
            : stopAction && !disabled ? C_RED : restartAction && !disabled ? C_CYAN
            : updateAction || compactAction ? C_PURPLE : nav ? fill : C_LINE;
        if (hovered && !runningStart) outline = dangerAction ? C_RED : C_WARN;
        HBRUSH surface = nav || (d->CtlID == 263 && g_sidebarOpen) || d->CtlID == NativeSavingsSidebarId ||
                         d->CtlID == NativeModelSidebarId || d->CtlID == NativeSupportSidebarId ? g_sideBrush
                         : d->CtlID == 175 || controllerAction ? g_cardBrush : g_bgBrush;
        FillRect(d->hDC, &d->rcItem, surface);
        if (!disabled)
        {
            RECT shadow = d->rcItem;
            shadow.top += 2;
            HBRUSH shadowBrush = CreateSolidBrush(C_LINE);
            HPEN shadowPen = CreatePen(PS_SOLID, 1, C_LINE);
            HGDIOBJ oldShadowBrush = SelectObject(d->hDC, shadowBrush);
            HGDIOBJ oldShadowPen = SelectObject(d->hDC, shadowPen);
            RoundRect(d->hDC, shadow.left, shadow.top, shadow.right, shadow.bottom, 10, 10);
            SelectObject(d->hDC, oldShadowBrush);
            SelectObject(d->hDC, oldShadowPen);
            DeleteObject(shadowBrush);
            DeleteObject(shadowPen);
        }
        RECT face = d->rcItem;
        if (!disabled && !pressed) face.bottom -= 2;
        else if (!disabled && pressed) OffsetRect(&face, 0, 1);
        HBRUSH b = CreateSolidBrush(fill);
        HBRUSH ob = (HBRUSH)SelectObject(d->hDC, b);
        HPEN p = CreatePen(PS_SOLID, runningStart || hovered ? 2 : 1, outline);
        HPEN op = (HPEN)SelectObject(d->hDC, p);
        RoundRect(d->hDC, face.left, face.top, face.right, face.bottom, 10, 10);
        SelectObject(d->hDC, ob);
        SelectObject(d->hDC, op);
        DeleteObject(b);
        DeleteObject(p);
        if (nav && !g_sidebarOpen)
        {
            const int index = d->CtlID - 101;
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, sel ? C_CYAN_TEXT : C_MUTED);
            SelectObject(d->hDC, g_sidebarIconFont ? g_sidebarIconFont : g_bold);
            DrawTextW(d->hDC, SidebarNavGlyphs[index], -1, &d->rcItem,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
            return TRUE;
        }
        if (sidebarToggle)
        {
            const wchar_t *glyph = g_sidebarOpen ? L"\xE76B" : L"\xE76C";
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, C_CYAN_TEXT);
            SelectObject(d->hDC, g_sidebarIconFont ? g_sidebarIconFont : g_bold);
            DrawTextW(d->hDC, glyph, -1, &d->rcItem,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
            return TRUE;
        }
        if (modelSidebar)
        {
            if (g_deepseekIcon)
            {
                const int iconSize = g_sidebarOpen ? 32 : 28;
                const int x = g_sidebarOpen ? d->rcItem.left + 10 : d->rcItem.left + (d->rcItem.right - d->rcItem.left - iconSize) / 2;
                const int y = d->rcItem.top + (d->rcItem.bottom - d->rcItem.top - iconSize) / 2;
                DrawIconEx(d->hDC, x, y, g_deepseekIcon, iconSize, iconSize, 0, nullptr, DI_NORMAL);
            }
            if (g_sidebarOpen)
            {
                RECT modelText = d->rcItem;
                modelText.left += 50;
                modelText.right -= 8;
                SetBkMode(d->hDC, TRANSPARENT);
                SetTextColor(d->hDC, C_TEXT);
                SelectObject(d->hDC, g_bold);
                DrawTextW(d->hDC, label, -1, &modelText,
                          DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
            }
            if (d->itemState & ODS_FOCUS) DrawFocusRect(d->hDC, &d->rcItem);
            return TRUE;
        }
        const bool supportHeart = d->CtlID == NativeSupportSidebarId;
        const bool supportMessage = d->CtlID == NativeSupportAboutId;
        const bool modelSelector = d->CtlID == NativeGreetingModelButtonId || d->CtlID == NativeChatModelButtonId;
        const bool reasoningSelector = d->CtlID == NativeChatReasoningControlId;
        if (supportHeart) wcscpy_s(label, L"♥");
        else if (d->CtlID == 108 && g_updateAvailable) wcscpy_s(label, L"About and updates");
        SetBkMode(d->hDC, TRANSPARENT);
        SetTextColor(d->hDC, hovered ? (dangerAction ? C_RED_TEXT : C_WARN)
            : authorLink ? C_CYAN_TEXT : composerButton && pressed ? C_CYAN_TEXT
            : disabled && controllerAction && !runningStart ? C_MUTED
            : runningStart ? C_GREEN_TEXT : primaryStart ? C_TEXT : stopAction ? C_RED_TEXT
            : restartAction ? C_CYAN_TEXT : supportHeart || sel || updateAction || compactAction ? C_PURPLE_TEXT : C_SOFT);
        SelectObject(d->hDC, supportHeart || runningStart || controllerAction || composerButton || hovered ? g_bold : g_font);
        RECT r = d->rcItem;
        if (pressed && !disabled) OffsetRect(&r, 0, 1);
        if (authorLink)
        {
            const int cy = (d->rcItem.top + d->rcItem.bottom) / 2;
            RECT badge{d->rcItem.left + 8, cy - 11, d->rcItem.left + 34, cy + 11};
            HBRUSH badgeBrush = CreateSolidBrush(C_CARD);
            HPEN badgePen = CreatePen(PS_SOLID, 1, C_CYAN);
            HGDIOBJ oldBadgeBrush = SelectObject(d->hDC, badgeBrush);
            HGDIOBJ oldBadgePen = SelectObject(d->hDC, badgePen);
            RoundRect(d->hDC, badge.left, badge.top, badge.right, badge.bottom, 7, 7);
            SelectObject(d->hDC, oldBadgeBrush);
            SelectObject(d->hDC, oldBadgePen);
            DeleteObject(badgeBrush);
            DeleteObject(badgePen);
            SetBkMode(d->hDC, TRANSPARENT);
            SetTextColor(d->hDC, C_CYAN_TEXT);
            SelectObject(d->hDC, g_small);
            DrawTextW(d->hDC, githubBadge ? L"GH" : L"HF", -1, &badge,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }
        if (!supportHeart) r.left += authorLink ? 42 : runningStart ? 30 : 16;
        if (runningStart)
        {
            const int y = (d->rcItem.top + d->rcItem.bottom) / 2;
            RECT dot{d->rcItem.left + 12, y - 4, d->rcItem.left + 20, y + 4};
            HBRUSH dotBrush = CreateSolidBrush(C_GREEN);
            HPEN dotPen = CreatePen(PS_SOLID, 1, C_GREEN);
            HGDIOBJ oldBrush = SelectObject(d->hDC, dotBrush);
            HGDIOBJ oldPen = SelectObject(d->hDC, dotPen);
            Ellipse(d->hDC, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(d->hDC, oldBrush);
            SelectObject(d->hDC, oldPen);
            DeleteObject(dotBrush);
            DeleteObject(dotPen);
        }
        if (modelSelector || reasoningSelector) r.right -= 22;
        const UINT textFormat = supportHeart ? DT_CENTER | DT_VCENTER | DT_SINGLELINE
                               : supportMessage ? DT_LEFT | DT_VCENTER | DT_WORDBREAK
                                                : DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
        DrawTextW(d->hDC, label, -1, &r, textFormat);
        if (modelSelector || reasoningSelector)
        {
            HPEN arrow = CreatePen(PS_SOLID, 2, C_PURPLE_TEXT);
            if (arrow)
            {
                HGDIOBJ previous = SelectObject(d->hDC, arrow);
                const int x = d->rcItem.right - 14;
                const int y = (d->rcItem.top + d->rcItem.bottom) / 2;
                MoveToEx(d->hDC, x - 3, y - 1, nullptr);
                LineTo(d->hDC, x, y + 2);
                LineTo(d->hDC, x + 3, y - 1);
                SelectObject(d->hDC, previous);
                DeleteObject(arrow);
            }
        }
        if (d->CtlID == 108 && g_updateAvailable)
        {
            RECT dot{d->rcItem.right - 22, d->rcItem.top + 15, d->rcItem.right - 12, d->rcItem.top + 25};
            HBRUSH dotBrush = CreateSolidBrush(C_RED);
            HBRUSH oldBrush = (HBRUSH)SelectObject(d->hDC, dotBrush);
            HPEN dotPen = CreatePen(PS_SOLID, 1, C_RED), oldPen = (HPEN)SelectObject(d->hDC, dotPen);
            Ellipse(d->hDC, dot.left, dot.top, dot.right, dot.bottom);
            SelectObject(d->hDC, oldBrush); SelectObject(d->hDC, oldPen);
            DeleteObject(dotBrush); DeleteObject(dotPen);
        }
        if (d->itemState & ODS_FOCUS)
        {
            RECT focus = d->rcItem;
            InflateRect(&focus, -3, -3);
            DrawFocusRect(d->hDC, &focus);
        }
        return TRUE;
    }
    if (m == WM_TIMER)
    {
        StartQueuedModelRefreshWhenIdle();
        if (w == NativeRestartTimerId)
        {
            if (g_busy != J_RESTART || !g_restartStartedAt)
            {
                KillTimer(h, NativeRestartTimerId);
                return 0;
            }
            const auto elapsed = (GetTickCount64() - g_restartStartedAt) / 1000;
            wchar_t label[96]{};
            swprintf_s(label, L"Restarting Maya… %02llu:%02llu elapsed",
                       (unsigned long long)(elapsed / 60), (unsigned long long)(elapsed % 60));
            SetText(g_status, label);
            return 0;
        }
        if (w == 1)
        {
            if (g_busy == J_NONE)
                LoadStatusData();
        }
        if (w == 2 && g_busy == J_NONE)
            RequestLiveMetrics(MonitorRequestsQueryAll(g_page == MONITOR, g_allRequests));
        if (w == 13)
        {
            if (UpdateMonitorVramHitExpiryTimer()) InvalidateMonitorThroughput(h);
            return 0;
        }
        if (w == 3)
            FlushStream();
        if (w == 4 && g_pullActive && g_busy == J_NONE)
            BeginJob(J_PULL_STATUS);
        if (w == 5)
        {
            KillTimer(h, 5);
            RequestContextCount();
        }
        if (w == 12)
        {
            KillTimer(h, 12);
            if (!g_sharedDefaultsSavePending) return 0;
            if (!g_sharedDefaultsOn) { g_sharedDefaultsSavePending = false; return 0; }
            if (!NativeSharedDefaultsSyncReady(g_sharedDefaultsSavePending, g_sharedDefaultsOn, g_busy != J_NONE))
            { SetTimer(h, 12, 500, nullptr); return 0; }
            g_sharedDefaultsSavePending = false;
            BeginJob(J_SHARED_SAVE);
            return 0;
        }
        if (w == 7)
        {
            KillTimer(h, 7);
            RefreshChatList();
            if (GetFocus() == g_chatSearch)
            {
                const int first = NativeChatSearchMoveSelection(
                    -1, (int)g_visibleChats.size(), true, [](int row) { return g_visibleChats[row].chat >= 0; });
                if (first >= 0) SendMessageW(g_chatList, LB_SETCURSEL, first, 0);
            }
        }
        if (w == 8) FinishExpiredPendingChatDeletes();
        if (w == 9 && g_modelSettingsRestartWatching && g_busy == J_NONE)
            LoadStatusData();
        if (w == 11)
        {
            KillTimer(h, 11);
            if (g_modelSettingsOpen)
            {
                if (g_busy != J_NONE && g_busy != J_CHAT)
                    SetTimer(h, 11, 300, nullptr);
                else
                {
                    const auto draft = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
                    if (!draft.is_discarded() && draft.is_object() && CurrentModelSettingsChanged(draft))
                        RequestModelSettingsPreview();
                }
            }
        }
        if (w == 10 && NativeMcpRefreshEligible(g_page == SETTINGS || g_page == MONITOR,
                                                 g_busy == J_NONE || g_busy == J_CHAT,
                                                 g_mcpPollInFlight.load()))
            BeginJob(J_MCP, nullptr, true);
        if (w == 10) CheckForUpdatesIfNeeded();
        if (w == 6 && g_busy == J_CHAT)
            RequestLiveMetrics(MonitorRequestsQueryAll(g_page == MONITOR, g_allRequests));
        return 0;
    }
    if (m == WM_NOTIFY)
    {
        const auto *header = (NMHDR *)l;
        if (header->hwndFrom == g_monitorRequests && header->code == LVN_GETDISPINFO)
        {
            auto *display = (NMLVDISPINFOW *)l;
            g_monitorCellText = L"—";
            if ((display->item.mask & LVIF_TEXT) && g_metrics.is_object() &&
                g_metrics.contains("requests") && g_metrics["requests"].is_array() &&
                display->item.iItem >= 0 && (size_t)display->item.iItem < g_metrics["requests"].size())
            {
                try
                {
                    g_monitorCellText = MonitorRequestText(g_metrics["requests"][display->item.iItem]);
                }
                catch (...)
                {
                    g_monitorCellText = L"—";
                }
            }
            display->item.pszText = g_monitorCellText.data();
            return 0;
        }
        if (header->hwndFrom == g_transcript && header->code == EN_LINK)
        {
            const auto *link = (ENLINK *)l;
            if (link->msg == WM_LBUTTONUP)
                for (const auto &[range, key] : g_thoughtLinks)
                    if (ThoughtLinkHit(range.cpMin, range.cpMax, link->chrg.cpMin, link->chrg.cpMax))
                    {
                        const bool autoExpand = g_liveThought.active && g_liveThought.key == key && g_showThinking;
                        const bool wasExpanded = ThoughtIsExpanded(g_expandedThoughts, g_collapsedThoughts, key,
                                                                   autoExpand);
                        ToggleThoughtExpansion(g_expandedThoughts, g_collapsedThoughts, key, wasExpanded);
                        RenderChat();
                        return 1;
                    }
            if (link->msg == WM_LBUTTONUP)
                for (const auto &[range, key] : g_toolLinks)
                    if (link->chrg.cpMin < range.cpMax && link->chrg.cpMax > range.cpMin)
                    {
                        if (g_expandedTools.count(key)) g_expandedTools.erase(key);
                        else g_expandedTools.insert(key);
                        RenderChat();
                        return 1;
                    }
            if (link->msg == WM_LBUTTONUP)
                for (const auto &[range, url] : g_markdownLinks)
                    if (link->chrg.cpMin < range.cpMax && link->chrg.cpMax > range.cpMin)
                    {
                        ShellExecuteW(h, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        return 1;
                    }
            if (link->msg == WM_LBUTTONUP)
                for (const auto &[range, path] : g_attachmentLinks)
                    if (link->chrg.cpMin < range.cpMax && link->chrg.cpMax > range.cpMin)
                    {
                        const auto attachmentPath = path;
                        const auto kind = MayaAttachmentType(attachmentPath);
                        if (kind == MayaAttachmentKind::VIDEO)
                        {
                            if (g_inlineVideoPath == attachmentPath && maya_video::IsInlineOpenFor(g_transcript, attachmentPath))
                            {
                                maya_video::CloseInline();
                                g_inlineVideoPath.clear();
                                RenderChat();
                                SetText(g_status, L"Inline video closed.");
                                return 1;
                            }
                            maya_video::CloseInline();
                            g_inlineVideoPath = attachmentPath;
                            RenderChat();
                            const auto videoLink = std::find_if(g_attachmentLinks.begin(), g_attachmentLinks.end(),
                                [&](const auto &entry) { return entry.second == attachmentPath; });
                            bool opened = videoLink != g_attachmentLinks.end() &&
                                          maya_video::OpenInline(g_transcript, attachmentPath, videoLink->first.cpMin);
                            if (!opened)
                            {
                                g_inlineVideoPath.clear();
                                RenderChat();
                                opened = OpenAttachmentFile(h, attachmentPath);
                            }
                            SetText(g_status, opened ? (g_inlineVideoPath.empty() ? L"Video player opened." : L"Video playing inline.") :
                                L"Windows could not preview or open that video.");
                            return 1;
                        }
                        const bool opened = OpenAttachmentFile(h, attachmentPath);
                        SetText(g_status, opened ? (kind == MayaAttachmentKind::IMAGE ? L"Image preview opened." : L"Attachment opened.") :
                            L"Windows could not preview or open that attachment.");
                        return 1;
                    }
        }
    }
    if (m == WM_LIVE_METRICS)
    {
        std::unique_ptr<Json> metrics((Json *)l);
        g_metricsInFlight = false;
        if (metrics->is_object() && metrics->contains("poll_error"))
        {
            const auto previousConnection = g_apiConnection;
            g_apiConnection = NativeApiConnectionAfterMetricsPoll(
                g_apiConnection, g_metricPollFailures, metrics->value("http_status", 0u), false);
            UpdateMetricsPollTimer(IsIconic(h) != FALSE, GetForegroundWindow() == h);
            if (previousConnection != g_apiConnection)
            {
                SetText(g_status, StatusText());
                UpdateGreeting();
            }
            return 0;
        }
        const auto previousConnection = g_apiConnection;
        g_apiConnection = NativeApiConnectionAfterMetricsPoll(g_apiConnection, g_metricPollFailures, 0, true);
        UpdateMetricsPollTimer(IsIconic(h) != FALSE, GetForegroundWindow() == h);
        if (!NativeMetricsPayloadIsObject(*metrics))
        {
            if (previousConnection != g_apiConnection)
            {
                SetText(g_status, StatusText());
                UpdateGreeting();
            }
            return 0;
        }
        const auto previousRows = MonitorGpuRows(g_metrics).size();
        const bool previousExpanded = g_monitorGpuOpen >= 0;
        Json previousMetrics = std::move(g_metrics);
        g_metrics = std::move(*metrics);
        ReconcileMonitorGpuRows(previousRows, previousExpanded);
        if (g_page == ABOUT) UpdateAboutFacts();
        if (g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].id == g_jobChatId &&
            g_metrics.contains("live") && g_metrics["live"].is_object())
        {
            const auto live = g_metrics["live"];
            if (live.contains("prompt_tokens") && live["prompt_tokens"].is_number())
            {
                const uint64_t prompt = live["prompt_tokens"].get<uint64_t>();
                const uint64_t generated = live.value("generated", uint64_t(0));
                ShowContextCount(prompt + generated, g_health.value("max_context", 0), false);
            }
        }
        if (g_page == MONITOR) UpdateMonitorView(&previousMetrics);
        if (previousConnection != g_apiConnection)
        {
            SetText(g_status, StatusText());
            UpdateGreeting();
        }
        return 0;
    }
    if (m == WM_COMMAND && LOWORD(w) == NativeChatAblationControlId && HIWORD(w) == BN_CLICKED && g_page == CHAT)
    {
        ChooseChatAblation();
        return 0;
    }
    if (m == WM_COMMAND && LOWORD(w) == NativeChatReasoningControlId && HIWORD(w) == BN_CLICKED && g_page == CHAT)
    {
        ShowChatReasoningMenu(h);
        return 0;
    }
    if (m == WM_COMMAND)
    {
        const int id = LOWORD(w);
        if (id == NativeApiAddressId && HIWORD(w) == STN_CLICKED)
        {
            OpenHeaderApiAddress(h);
            return 0;
        }
        if (id == NativeTrayOpenId)
        {
            ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
            return 0;
        }
        if (id == NativeTrayCompactId)
        {
            ToggleCompactController();
            ShowWindow(h, SW_RESTORE);
            SetForegroundWindow(h);
            return 0;
        }
        if (id == NativeTrayExitId)
        {
            SendMessageW(h, WM_CLOSE, 0, 0);
            return 0;
        }
        if (id == NativeCompactToggleId && HIWORD(w) == BN_CLICKED)
        {
            ToggleCompactController();
            return 0;
        }
        if (!MayaShouldDispatchButtonCommand(HIWORD(w), (HWND)l)) return 0;
        if (id == NativeSavingsSidebarId && HIWORD(w) == BN_CLICKED)
        {
            ShowPage(SAVINGS);
            return 0;
        }
        if (id == NativeModelSidebarId && HIWORD(w) == BN_CLICKED)
        {
            ShowPage(MONITOR);
            return 0;
        }
        if ((id == NativeSupportSidebarId || id == NativeSupportAboutId) && HIWORD(w) == BN_CLICKED)
        {
            if ((INT_PTR)ShellExecuteW(h, L"open", NativeSupportUrl, nullptr, nullptr, SW_SHOWNORMAL) <= 32)
                SetText(g_status, L"Could not open GitHub Sponsors.");
            return 0;
        }
        if (HIWORD(w) == BN_CLICKED && (id == NativeAuthorGithubId || id == NativeAuthorHuggingFaceId ||
            id == NativeMayaCreatorGithubId || id == NativeMayaCreatorHuggingFaceId ||
            id == NativeMayaComaintainerGithubId || id == NativeMayaVersionReleaseId))
        {
            const wchar_t *url = id == NativeAuthorGithubId ? NativeAuthorGithubUrl
                : id == NativeAuthorHuggingFaceId ? NativeAuthorHuggingFaceUrl
                : id == NativeMayaCreatorGithubId ? NativeMayaCreatorGithubUrl
                : id == NativeMayaCreatorHuggingFaceId ? NativeMayaCreatorHuggingFaceUrl
                : id == NativeMayaComaintainerGithubId ? NativeMayaComaintainerGithubUrl
                : g_mayaReleaseUrl.c_str();
            if (*url && (INT_PTR)ShellExecuteW(h, L"open", url, nullptr, nullptr, SW_SHOWNORMAL) <= 32)
                SetText(g_status, id == NativeMayaVersionReleaseId
                    ? L"Could not open the installed Maya release page." : L"Could not open that profile.");
            return 0;
        }
        const bool hasSelectedChat = g_chat >= 0 && g_chat < (int)g_chats.size();
        if (HIWORD(w) == BN_CLICKED &&
            NativeChatModelButtonShouldOpen(id, g_page == CHAT, hasSelectedChat,
                                             hasSelectedChat && g_chats[g_chat].messages.empty()))
        {
            ShowChatModelMenu(h, id == NativeGreetingModelButtonId ? g_greetingModel : g_chatModelMenu);
            return 0;
        }
        if (id == NativeChatOptionsId && HIWORD(w) == BN_CLICKED && g_page == CHAT && g_chat >= 0 && g_chat < (int)g_chats.size())
        {
            RECT r{};
            GetWindowRect(g_chatOptions, &r);
            ShowChatMenu(h, MAKELPARAM(r.left, r.bottom), g_chat);
            return 0;
        }
        if (id == 215 && HIWORD(w) == BN_CLICKED)
        {
            const bool enabled = SendMessageW(g_windowsStartupCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (!enabled) SendMessageW(g_autoStartMayaCheck, BM_SETCHECK, BST_UNCHECKED, 0);
            EnableWindow(g_autoStartMayaCheck, enabled);
            return 0;
        }
        if (id == 216 && HIWORD(w) == BN_CLICKED && SendMessageW(g_autoStartMayaCheck, BM_GETCHECK, 0, 0) == BST_CHECKED)
        {
            SendMessageW(g_windowsStartupCheck, BM_SETCHECK, BST_CHECKED, 0);
            EnableWindow(g_autoStartMayaCheck, TRUE);
            return 0;
        }
        if (id == 217 && HIWORD(w) == BN_CLICKED)
        {
            try { SaveWindowsStartupControls(); }
            catch (const std::exception &e) { SetText(g_settingsStatus, W(e.what())); }
            return 0;
        }
        if (id == 218 && HIWORD(w) == BN_CLICKED)
        {
            MessageBoxW(h,
                L"Send — Enter\r\nNew line — Shift+Enter\r\nNew chat — Ctrl+Shift+O\r\nSearch chats — Ctrl+K\r\nStop the answer — Esc\r\nSettings — Ctrl+,\r\nShow or hide the sidebar — Ctrl+Shift+S",
                L"Keyboard shortcuts", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (id >= 5000 && id < 5016 && HIWORD(w) == BN_CLICKED && g_page == MONITOR)
        {
            const int index = id - 5000;
            g_monitorGpuSelected = index;
            SetMonitorGpuOpen(index);
            {
                NativeLayoutTransition transition;
                Layout();
            }
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        }
        if (id >= 270 && id <= 273 && g_page == CHAT && g_chat >= 0 && g_chat < (int)g_chats.size() &&
            g_chats[g_chat].messages.empty())
        {
            SetWindowTextW(g_input, NativeChatIdeas(g_turboData.value("on", false))[id - 270].prompt);
            SetFocus(g_input);
            ScheduleContextMeter();
            return 0;
        }
        if (id == 267 && g_page == CHAT_DEFAULTS && g_busy == J_NONE)
        {
            const bool on = !g_turboData.value("on", false);
            if (on)
            {
                std::wstring warning = L"Turbo trades answer quality and precision for speed. It uses greedy decoding and the fastest available thinking setting (Qwen stays at Low), and may enable expert skipping and aggressive drafting. Answers can be less accurate. Use it only when speed matters more than correctness.";
                const auto plan = g_turboData.value("plan", Json::object());
                for (auto it = plan.begin(); it != plan.end(); ++it)
                    warning += L"\r\n" + W(it.key()) + L": " + W(it.value().value("from", Json(nullptr)).dump()) + L" → " +
                               W(it.value().value("to", Json(nullptr)).dump());
                if (g_turboData.contains("env") && g_turboData["env"].is_object() && !g_turboData["env"].empty())
                    warning += L"\r\nEngine expert-skipping settings will also change.";
                warning += L"\r\n\r\nTurn Turbo off to restore your previous settings.";
                if (MessageBoxW(h, warning.c_str(), L"Enable Turbo mode?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
                    return 0;
                if (!g_turboSnapshot.is_object())
                {
                    const auto family = ChatSamplingFamily(g_health.value("model", std::string()));
                    const auto thinking = g_chat >= 0 && g_chat < (int)g_chats.size()
                        ? g_chats[g_chat].thinking : std::string();
                    const auto entered = NativeTurboEnter(g_samplingPrefs, family, thinking, ChatSamplingEfforts(family));
                    g_turboSnapshot = entered["snapshot"];
                    g_samplingPrefs = entered["sampling"];
                    if (g_chat >= 0 && g_chat < (int)g_chats.size())
                    {
                        g_chats[g_chat].thinking = g_turboSnapshot.value("fast", std::string());
                        SaveChatMetadata(g_chats[g_chat]);
                    }
                    if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
                    {
                        RestoreNativeTurboPrefs();
                        return 0;
                    }
                    LoadChatDefaultsControls();
                    ApplyChatThinking();
                }
            }
            g_turboRequest = {{"on", on}};
            g_turboRequestOn = on;
            SetText(g_samplingStatus, on ? L"Enabling Turbo…" : L"Restoring previous settings…");
            BeginJob(J_TURBO_SET);
            UpdateTurboButton();
            return 0;
        }
        if (id == 266)
        {
            UndoPendingChatDelete();
            return 0;
        }
        if (id == 264)
        {
            SendMessageW(g_transcript, EM_SETSEL, -1, -1);
            SendMessageW(g_transcript, EM_SCROLLCARET, 0, 0);
            UpdateJumpLatest();
            return 0;
        }
        if (id == 121 && HIWORD(w) == EN_VSCROLL)
        {
            UpdateJumpLatest();
            maya_video::RepositionInline(g_transcript);
            return 0;
        }
        if (id >= 195 && id <= 197)
        {
            const auto &value = g_connectValue[id - 195];
            const bool copied = CopyUnicodeText(h, W(ControlText(value)));
            SetText(g_status, copied ? L"Connection value copied." : L"Could not copy the connection value.");
            return 0;
        }
        if (id == 198 && HIWORD(w) == CBN_SELCHANGE)
        {
            UpdateConnectPage();
            return 0;
        }
        if (id == 199)
        {
            const bool copied = CopyUnicodeText(h, W(ControlText(g_connectExample)));
            SetText(g_status, copied ? L"Example copied. Replace the API-key placeholder before using it."
                                     : L"Could not copy the example.");
            return 0;
        }
        if ((id == 207 || id == 214) && g_page == CONNECT)
        {
            if (id == 207)
            {
                const int n = GetWindowTextLengthW(g_connectKey);
                std::wstring key((size_t)n + 1, L'\0');
                GetWindowTextW(g_connectKey, key.data(), n + 1);
                key.resize((size_t)n);
                while (!key.empty() && iswspace(key.back())) key.pop_back();
                size_t first = 0;
                while (first < key.size() && iswspace(key[first])) ++first;
                key.erase(0, first);
                if (!MayaApiKeyTextValid(key))
                {
                    SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
                    SetText(g_connectAuth, L"Enter a valid API key (up to 2048 characters, no line breaks).");
                    return 0;
                }
                const bool saved = SaveMayaApiKeyOverride(key);
                SecureZeroMemory(key.data(), key.size() * sizeof(wchar_t));
                if (!saved)
                {
                    SetText(g_connectAuth, L"Windows Credential Manager could not save the API key.");
                    return 0;
                }
                SetWindowTextW(g_connectKey, L"");
                SetText(g_status, L"API key saved in Windows Credential Manager.");
            }
            else if (!ClearMayaApiKeyOverride())
            {
                SetText(g_connectAuth, L"Windows could not clear the saved API key override.");
                return 0;
            }
            else
            {
                SetWindowTextW(g_connectKey, L"");
                SetText(g_status, L"API key override cleared; the selected profile key is used if configured.");
            }
            g_api.reset();
            UpdateConnectPage();
            return 0;
        }
        if (id >= 101 && id <= 109)
        {
            ShowPage((Page)(id - 101));
            return 0;
        }
        if (NativeStatusCardClicked(id, HIWORD(w)))
        {
            ShowPage(MONITOR);
            return 0;
        }
        if (id == 220 && HIWORD(w) == BN_CLICKED && g_page == ABOUT)
        {
            StartMayaUpdateFromPreflight(h);
            return 0;
        }
        if (id == 211 && g_page == ABOUT && !g_updateNotesUrl.empty())
        {
            const auto opened = (INT_PTR)ShellExecuteW(h, L"open", g_updateNotesUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (opened <= 32) SetText(g_status, L"Could not open the GitHub release notes.");
            return 0;
        }
        if ((id == 210 || id == 260) && g_page == ABOUT)
        {
            if (id == 260 && g_aboutLogVisible)
            {
                g_aboutLogVisible = false;
                ShowWindow(g_aboutReport, SW_HIDE);
                SetWindowTextW(GetDlgItem(h, 260), L"Show report");
            }
            else
            {
                if (id == 260) g_aboutLogVisible = true;
                g_aboutCopyRequested = id == 210;
                g_aboutReportRequest = true;
                SetWindowTextW(GetDlgItem(h, 260), L"Loading report…");
                EnableWindow(GetDlgItem(h, 260), FALSE);
                EnableWindow(GetDlgItem(h, 210), FALSE);
                BeginJob(J_REPORT);
            }
            return 0;
        }
        if (id == 110 && HIWORD(w) == CBN_SELCHANGE)
        {
            const auto previous = g_profile;
            g_profile =
                SendMessageW(g_profileBox, CB_GETCURSEL, 0, 0) == 0 ? L"maya-ds4.json" : L"maya-ds4-abliterated.json";
            if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
            {
                g_profile = previous;
                SendMessageW(g_profileBox, CB_SETCURSEL, ChatProfileComboIndex(g_profile), 0);
                SetText(g_status, L"Profile selection was not saved; the previous profile remains selected.");
                return 0;
            }
            g_mcpData = Json::object();
            g_mcpLoadAttempted = false;
            g_api.reset();
            LoadStatusData();
            ScheduleContextMeter();
            return 0;
        }
        if (id == 263)
        {
            ToggleSidebar();
            return 0;
        }
        if (id == 111)
        {
            BeginJob(J_START, L"start");
            return 0;
        }
        if (id == 112)
        {
            BeginJob(J_STOP, L"stop");
            return 0;
        }
        if (id == 113)
        {
            if (MessageBoxW(h, L"Restart Maya now? This interrupts its current generation.", L"Restart Maya",
                            MB_YESNO | MB_ICONWARNING) == IDYES)
                BeginJob(J_RESTART, L"restart");
            return 0;
        }
        if (id == 114)
        {
            if (g_updateAvailable)
                StartMayaUpdateFromPreflight(h, true, true);
            else
                BeginJob(J_UPDATE_CHECK, L"update-check");
            return 0;
        }
        if (id == 146)
        {
            BeginJob(J_REPORT);
            return 0;
        }
        if (id == NativeModelSetupCommandControlId && HIWORD(w) == BN_CLICKED)
        {
            HMENU menu = CreatePopupMenu();
            if (!menu || !AppendMenuW(menu, MF_STRING, 1, L"Windows — START-MAYA.bat --setup") ||
                !AppendMenuW(menu, MF_STRING, 2, L"Linux/macOS — ./maya.sh --setup"))
            {
                if (menu) DestroyMenu(menu);
                SetText(g_status, L"Could not open the manual setup commands.");
                return 0;
            }
            RECT button{};
            GetWindowRect(g_modelSetupCommand, &button);
            const UINT choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                               button.left, button.bottom, 0, h, nullptr);
            DestroyMenu(menu);
            const auto command = NativeModelSetupCommand(choice);
            if (command)
            {
                const bool copied = CopyUnicodeText(h, command);
                SetText(g_status, copied ? L"Copied " + std::wstring(command) + L"."
                                          : L"Could not copy the setup command.");
            }
            return 0;
        }
        if (id == 201)
        {
            if (g_busy != J_NONE && g_busy != J_CHAT)
                return 0;
            g_allRequests = !g_allRequests;
            UpdateMonitorView(&g_metrics, &g_health);
            if (g_busy == J_NONE) BeginJob(J_REFRESH);
            return 0;
        }
        if (id == 153)
        {
            BeginJob(J_SAVINGS);
            return 0;
        }
        if (id == 154)
        {
            if (MessageBoxW(h, L"Reset every saved token count and its calculated cost to $0? Custom prices will stay.",
                            L"Reset Maya savings", MB_YESNO | MB_ICONWARNING) == IDYES)
                BeginJob(J_SAVINGS_RESET);
            return 0;
        }
        if (id == 155 && HIWORD(w) == CBN_SELCHANGE)
        {
            LoadPriceFields();
            return 0;
        }
        if (id >= 156 && id <= 159 && HIWORD(w) == EN_CHANGE)
        {
            UpdatePriceDraftControls();
            return 0;
        }
        if (id == 160)
        {
            try
            {
                const int index = (int)SendMessageW(g_priceModels, CB_GETCURSEL, 0, 0);
                if (index < 0 || !g_savingsData.contains("price_list") || index >= (int)g_savingsData["price_list"].size())
                    throw std::runtime_error("Load Savings and select a model first.");
                const auto &p = g_savingsData["price_list"][index];
                const auto defaults = p.value("default", Json::object());
                const bool peak = defaults.is_object() && defaults.contains("peak") && defaults["peak"].is_number();
                std::vector<std::string> values{ControlText(g_priceInput), ControlText(g_priceCached),
                                                ControlText(g_priceOutput)};
                if (peak) values.push_back(ControlText(g_pricePeak));
                g_priceRequest = SavingsPriceRequest(g_savingsModel, values, peak);
                BeginJob(J_SAVINGS_SAVE_PRICE);
            }
            catch (const std::exception &e)
            {
                SetText(g_status, W(e.what()));
            }
            return 0;
        }
        if (id == 161)
        {
            if (!g_savingsModel.empty())
            {
                g_priceRequest = {{"model", g_savingsModel}, {"price", nullptr}};
                BeginJob(J_SAVINGS_DEFAULT_PRICE);
            }
            return 0;
        }
        if (id == 300)
        {
            g_priceDrafts.erase(g_savingsModel);
            LoadPriceFields();
            return 0;
        }
        if (id == 122)
        {
            SendChat();
            return 0;
        }
        if (id == 126)
        {
            NewChat();
            return 0;
        }
        if (id == 129)
        {
            EditLastUserMessage();
            return 0;
        }
        if (id == 131)
        {
            RegenerateLastAnswer();
            return 0;
        }
        if (id == 132)
        {
            CancelChatEdit();
            return 0;
        }
        if (id == 133 && g_chat >= 0 && g_chat < (int)g_chats.size() && !g_chats[g_chat].messages.empty())
        {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Copy last answer");
            AppendMenuW(menu, MF_STRING, 2, L"Answer details");
            RECT r{};
            GetWindowRect(g_answerOptions, &r);
            const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, r.left, r.bottom, 0, h, nullptr);
            DestroyMenu(menu);
            const auto &answer = g_chats[g_chat].messages.back();
            if (selected == 1)
                SetText(g_status, !answer.text.empty() && CopyUnicodeText(h, W(answer.text))
                                      ? L"Answer copied to clipboard."
                                      : L"There is no answer text to copy, or the clipboard is unavailable.");
            else if (selected == 2)
                MessageBoxW(h, answer.meta.empty() ? L"Maya did not return token or timing details."
                                                   : W(answer.meta).c_str(),
                            L"Answer details", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (id == 128)
        {
            ShowFolderMenu(h);
            return 0;
        }
        if (id == 127 && HIWORD(w) == EN_CHANGE)
        {
            SetTimer(h, 7, 150, nullptr);
            return 0;
        }
        if (id == 0 && HIWORD(w) == EN_CHANGE && (HWND)l == g_input)
        {
            UpdateComposerHeight();
            ScheduleContextMeter();
            return 0;
        }
        if (id == 265)
        {
            ShowAttachmentMenu(h);
            return 0;
        }
        if (id == 123)
        {
            HMENU menu = CreatePopupMenu();
            if (!menu) return 0;
            AppendMenuW(menu, MF_STRING, 1, L"Select file");
            const auto servers = g_mcpData.is_object() && g_mcpData.contains("servers") && g_mcpData["servers"].is_array()
                                     ? g_mcpData["servers"] : Json::array();
            if (NativeComposerShowsTools(servers.size()))
            {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                const int count = g_mcpData.contains("tools") && g_mcpData["tools"].is_number_integer()
                                      ? g_mcpData["tools"].get<int>() : 0;
                const auto label = count > 0 ? L"MCP tools (" + std::to_wstring(count) + L" available)"
                                             : std::wstring(L"MCP tools (none connected)");
                AppendMenuW(menu, MF_STRING | (g_useTools ? MF_CHECKED : 0), 2, label.c_str());
            }
            RECT menuRect{};
            GetWindowRect(g_attach, &menuRect);
            const UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                                 menuRect.left, menuRect.top, 0, h, nullptr);
            DestroyMenu(menu);
            if (selected == 2)
            {
                const bool previous = g_useTools;
                g_useTools = !g_useTools;
                if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
                {
                    g_useTools = previous;
                    SendMessageW(g_toolsCheck, BM_SETCHECK, g_useTools ? BST_CHECKED : BST_UNCHECKED, 0);
                    SetText(g_status, L"MCP tools setting could not be saved.");
                }
                else
                {
                    SendMessageW(g_toolsCheck, BM_SETCHECK, g_useTools ? BST_CHECKED : BST_UNCHECKED, 0);
                    SetText(g_status, g_useTools ? L"MCP tools enabled for new requests." : L"MCP tools disabled for new requests.");
                }
                return 0;
            }
            if (selected != 1) return 0;
            OPENFILENAMEW ofn{sizeof(ofn)};
            wchar_t path[32768]{};
            ofn.hwndOwner = h;
            ofn.lpstrFile = path;
            ofn.nMaxFile = 32768;
            ofn.lpstrFilter =
                L"Chat attachments\0*.png;*.jpg;*.jpeg;*.webp;*.gif;*.bmp;*.tif;*.tiff;*.mp4;*.m4v;*.mov;*.webm;*.mkv;*.avi;*.mpeg;*.mpg;*.3gp;*.ogv;*.txt;*.md;*.json;*.log;*.csv;*.py;*.cpp;*.h;*.pdf;*.docx;*.xlsx;*.pptx;*.odt;*.ods;*.odp;*.epub;*.rtf\0All "
                L"files\0*.*\0\0";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_ALLOWMULTISELECT | OFN_EXPLORER;
            if (GetOpenFileNameW(&ofn))
                AddAttachmentPaths(MayaParseFileSelection(path));
            return 0;
        }
        if (id == 125)
        {
            StopChat();
            return 0;
        }
        if (id == 140 && (HIWORD(w) == LBN_SELCHANGE || HIWORD(w) == LBN_DBLCLK))
        {
            g_modelSel = (int)SendMessageW(g_models, LB_GETCURSEL, 0, 0);
            UpdateModelLoadButton();
            UpdateModelSwitchButton();
            UpdateModelDeleteButton();
            UpdateModelSettingsButton();
            if (HIWORD(w) == LBN_DBLCLK) OpenModelSheet(g_modelSel);
            return 0;
        }
        if (id == NativeModelSheetOverviewId || id == NativeModelSheetEngineId || id == NativeModelSheetStorageId)
        {
            if (!g_modelSheetOpen) return 0;
            g_modelSheetTab = id == NativeModelSheetOverviewId ? 0 : id == NativeModelSheetEngineId ? 1 : 2;
            UpdateModelSheetView();
            if (g_modelSheetTab == 2) RequestModelSheetStoragePlan();
            SetPageControls();
            Layout();
            SetFocus(g_modelSheetTabs[g_modelSheetTab]);
            return 0;
        }
        if (id == NativeModelSheetEditEngineId)
        {
            if (!g_modelSheetOpen || g_modelSheetTab != 1) return 0;
            g_modelSel = ModelSheetModelIndex();
            if (g_modelSel < 0) return 0;
            g_modelSheetOpen = false;
            SetPageControls();
            Layout();
            SendMessageW(h, WM_COMMAND, MAKEWPARAM(163, BN_CLICKED), (LPARAM)g_modelSettingsButton);
            return 0;
        }
        if (id == NativeModelSheetCloseId)
        {
            if (!g_modelSheetOpen) return 0;
            g_modelSheetOpen = false;
            SetPageControls();
            Layout();
            SetFocus(g_models);
            return 0;
        }
        if (id == 212 && g_page == MODELS && !g_modelSettingsOpen && !g_leftoverOpen)
        {
            g_modelsAddOpen = !g_modelsAddOpen;
            SetPageControls();
            Layout();
            if (g_modelsAddOpen)
            {
                if (g_busy == J_NONE) BeginJob(J_CATALOG);
                else
                {
                    g_catalogFetchPending = true;
                    SetText(g_status, L"The supported model catalog will load when the current action finishes.");
                }
            }
            return 0;
        }
        if (id == 141 && HIWORD(w) == LBN_SELCHANGE)
        {
            g_catalogSel = CatalogModelIndexForListRow(g_catalogRowToModel,
                                                       (int)SendMessageW(g_catalog, LB_GETCURSEL, 0, 0));
            UpdateCatalogDownloadButton();
            return 0;
        }
        if (id == 142)
        {
            if (g_modelSel < 0 || !g_modelData.contains("models") || !g_modelData["models"].is_array() ||
                g_modelSel >= (int)g_modelData["models"].size())
                return 0;
            RequestModelSwitchToFile(g_modelData["models"][g_modelSel].value("file", std::string()));
            return 0;
        }
        if (id == 147)
        {
            if (g_modelSheetOpen) g_modelSel = ModelSheetModelIndex();
            if (g_busy != J_NONE || g_modelSel < 0 || !g_modelData.contains("models") ||
                !g_modelData["models"].is_array() || g_modelSel >= (int)g_modelData["models"].size() ||
                !ModelDeleteEnabled(g_modelData["models"][g_modelSel], g_modelData["models"],
                                    g_modelData.value("restarts", false)))
                return 0;
            g_deleteFile = g_modelSheetOpen ? g_modelSheetFile
                                             : g_modelData["models"][g_modelSel].value("file", std::string());
            g_deleteThen.clear();
            g_modelSheetPlanPreview = false;
            BeginJob(J_MODEL_DELETE_PLAN);
            return 0;
        }
        if (id == 162)
        {
            if (g_busy != J_NONE || !g_modelData.value("can_unload", false) || g_modelSel < 0 ||
                !g_modelData.contains("models") || g_modelSel >= (int)g_modelData["models"].size() ||
                !g_modelData["models"][g_modelSel].value("running", false))
                return 0;
            const bool loaded = g_modelData.value("loaded", false);
            if (!loaded || MessageBoxW(h, L"Unload the running model and release its GPU memory?", L"Unload Maya model",
                                       MB_YESNO | MB_ICONQUESTION) == IDYES)
                BeginJob(loaded ? J_UNLOAD : J_LOAD);
            return 0;
        }
        if (id == 170)
        {
            if (g_busy != J_NONE) return 0;
            g_leftoverOpen = true;
            PopulateLeftovers();
            SetPageControls();
            Layout();
            return 0;
        }
        if (id == 169 && HIWORD(w) == LBN_SELCHANGE)
        {
            UpdateLeftoverDetails();
            return 0;
        }
        if (id == 172)
        {
            const auto leftovers = g_modelData.contains("leftovers") && g_modelData["leftovers"].is_object()
                ? g_modelData["leftovers"] : Json::object();
            const auto items = leftovers.contains("items") && leftovers["items"].is_array()
                ? leftovers["items"] : Json::array();
            const int index = (int)SendMessageW(g_leftoverList, LB_GETCURSEL, 0, 0);
            const bool selected = items.is_array() && index >= 0 && index < (int)items.size();
            if (!ModelLeftoverDeleteEnabled(selected, g_busy != J_NONE, g_modelSwitchRequestInFlight,
                                             g_switchRestartWatching, g_modelSettingsRestartWatching)) return 0;
            const auto &item = items[index];
            const auto path = item.value("path", std::string());
            const auto name = item.value("name", std::string("unused item"));
            std::wstring prompt = L"Delete " + W(name) + (item.value("folder", false)
                ? L" and everything inside it?\n\n" : L"?\n\n");
            prompt += L"Size: " + ByteSize(item.value("bytes", uint64_t(0))) + L"\nPath: " + W(path);
            if (item.value("pack", false)) prompt += L"\nContains a Maya-built model pack.";
            if (item.value("hub", false)) prompt += L"\nContains a Hugging Face download.";
            prompt += L"\n\nNo model on Maya's list uses this path. This cannot be undone.";
            if (MessageBoxW(h, prompt.c_str(), L"Delete unused model files?",
                            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
                return 0;
            try
            {
                g_leftoverPath = path;
                (void)ModelLeftoverRequest(g_leftoverPath);
                EnableWindow(g_leftoverDelete, FALSE);
                BeginJob(J_MODEL_LEFTOVER_DELETE);
            }
            catch (const std::exception &e) { SetText(g_status, W(e.what())); }
            return 0;
        }
        if (id == 173)
        {
            g_leftoverOpen = false;
            SetPageControls();
            Layout();
            return 0;
        }
        if (id == 163)
        {
            if (g_busy != J_NONE || g_modelSel < 0 || !g_modelData.contains("models") ||
                g_modelSel >= (int)g_modelData["models"].size())
                return 0;
            const auto &model = g_modelData["models"][g_modelSel];
            if (!ModelSettingsButtonEnabled(model)) return 0;
            if (!model.contains("settings") || !model["settings"].is_object())
            {
                SetText(g_status, L"Maya did not provide an editable settings object for this model.");
                return 0;
            }
            g_modelSettingsFile = model.value("file", std::string());
            g_modelSettingsOpen = true;
            g_modelSettingsAdvanced = false;
            g_modelSettingsEnvironmentOpen = false;
            g_modelSettingsPreviewValid = false;
            g_modelSettingsPreviewText.clear();
            g_modelSettingsChoices = model.value("choices", Json::object());
            g_modelSettingsSetup = model.value("setup", Json());
            g_modelSettingsContextRange = model.value("context_range", Json{{"min", 4096}, {"max", 262144}});
            PopulateModelContextChoices(g_modelSettingsChoices, model["settings"], g_modelSettingsContextRange);
            BuildModelSettingsFields(model["settings"], g_modelSettingsChoices, g_modelData.value("gpus", Json::array()));
            LoadModelSettingsEnvironmentRows(model["settings"]);
            SetText(g_modelSettingsEdit, W(model["settings"].dump(2)));
            const auto command = model.value("command", Json());
            SetText(g_modelSettingsPreview, command.is_null()
                ? L"Edit settings to preview the launch command. Maya checks changes automatically."
                : W(ModelSettingsCommandPreviewText(command, command) +
                    "\nCurrent launch command. Maya checks changes automatically."));
            SelectModelSettingsTab(0);
            SetText(g_modelSettingsModeButton, L"Show Advanced JSON");
            UpdateModelSettingsContextHint(model["settings"]);
            EnableWindow(g_modelSettingsResetButton, NativeModelSettingsResetEnabled(g_modelSettingsSetup.is_object(), false));
            EnableWindow(g_modelSettingsSave, FALSE);
            EnableWindow(g_modelSettingsPreviewButton, TRUE);
            SetPageControls();
            Layout();
            return 0;
        }
        if (id == 277)
        {
            if (!g_modelSettingsOpen || !NativeModelSettingsResetEnabled(
                    g_modelSettingsSetup.is_object(), g_modelSettingsRequestInFlight != J_NONE))
                return 0;
            g_modelSettingsPreviewValid = false;
            g_modelSettingsPreviewText.clear();
            BuildModelSettingsFields(g_modelSettingsSetup, g_modelSettingsChoices, g_modelData.value("gpus", Json::array()));
            LoadModelSettingsEnvironmentRows(g_modelSettingsSetup);
            SetText(g_modelSettingsEdit, W(g_modelSettingsSetup.dump(2)));
            SelectModelSettingsTab(g_modelSettingsTab);
            SetText(g_modelSettingsPreview, L"Settings restored to the setup values. Preview them with Maya before applying.");
            if (!g_modelSettingsAdvanced) UpdateModelSettingsContextHint(g_modelSettingsSetup);
            else SetText(g_modelSettingsHint, L"Advanced JSON includes arrays and maps. Preview with Maya before applying.");
            EnableModelSettingsInputs(true);
            EnableWindow(g_modelSettingsSave, FALSE);
            EnableWindow(g_modelSettingsPreviewButton, TRUE);
            EnableWindow(g_modelSettingsModeButton, TRUE);
            EnableWindow(g_modelSettingsCancel, TRUE);
            SetText(g_status, L"Settings reset to setup values; Maya will check the launch command automatically.");
            Layout();
            return 0;
        }
        if (id >= 282 && id <= 285 && HIWORD(w) == BN_CLICKED && g_modelSettingsOpen && !g_modelSettingsAdvanced)
        {
            const int tab = id - 282;
            if (std::any_of(g_modelSettingFields.begin(), g_modelSettingFields.end(),
                            [tab](const auto &field) { return field.group == tab; }))
            {
                SelectModelSettingsTab(tab);
                SetFocus(g_modelSettingsTabs[tab]);
                Layout();
            }
            return 0;
        }
        if (id == 278 && HIWORD(w) == BN_CLICKED && g_modelSettingsOpen && g_modelSettingsAdvanced)
        {
            if (g_busy != J_NONE) return 0;
            const auto current = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
            if (current.is_discarded() || !current.is_object())
            {
                SetText(g_status, L"Settings must be valid JSON with an object at the top level before editing environment variables.");
                return 0;
            }
            LoadModelSettingsEnvironmentRows(current);
            g_modelSettingsEnvironmentOpen = true;
            UpdateModelSettingsTabs();
            SetPageControls();
            Layout();
            SetFocus(g_modelSettingsEnvList);
            return 0;
        }
        if (id == 279 && HIWORD(w) == LBN_SELCHANGE && g_modelSettingsEnvironmentOpen)
        {
            SelectModelSettingsEnvironmentRow();
            return 0;
        }
        if (id == 286 && HIWORD(w) == BN_CLICKED && g_modelSettingsEnvironmentOpen)
        {
            if (g_busy != J_NONE) return 0;
            const auto key = TrimModelEnvironmentKey(W(ControlText(g_modelSettingsEnvName)));
            if (key.empty())
            {
                SetText(g_status, L"Enter an environment variable name.");
                SetFocus(g_modelSettingsEnvName);
                return 0;
            }
            auto rows = g_modelSettingsEnvironmentRows;
            const auto value = W(ControlText(g_modelSettingsEnvValue));
            const int selected = (int)SendMessageW(g_modelSettingsEnvList, LB_GETCURSEL, 0, 0);
            auto duplicate = std::find_if(rows.begin(), rows.end(), [&](const auto &row) { return row.first == key; });
            size_t target = 0;
            if (duplicate != rows.end())
            {
                const size_t duplicateIndex = (size_t)std::distance(rows.begin(), duplicate);
                duplicate->second = value;
                if (selected >= 0 && selected < (int)rows.size() && (size_t)selected != duplicateIndex)
                {
                    rows.erase(rows.begin() + selected);
                    target = duplicateIndex > (size_t)selected ? duplicateIndex - 1 : duplicateIndex;
                }
                else target = duplicateIndex;
            }
            else if (selected >= 0 && selected < (int)rows.size())
            {
                rows[selected] = {key, value};
                target = (size_t)selected;
            }
            else
            {
                rows.emplace_back(key, value);
                target = rows.size() - 1;
            }
            if (!CommitModelSettingsEnvironmentRows(rows)) return 0;
            g_modelSettingsEnvironmentRows = std::move(rows);
            RefreshModelSettingsEnvironmentList((int)target);
            return 0;
        }
        if (id == 287 && HIWORD(w) == BN_CLICKED && g_modelSettingsEnvironmentOpen)
        {
            if (g_busy != J_NONE) return 0;
            const int selected = (int)SendMessageW(g_modelSettingsEnvList, LB_GETCURSEL, 0, 0);
            if (selected < 0 || selected >= (int)g_modelSettingsEnvironmentRows.size()) return 0;
            auto rows = g_modelSettingsEnvironmentRows;
            rows.erase(rows.begin() + selected);
            if (!CommitModelSettingsEnvironmentRows(rows)) return 0;
            const int nextSelected = rows.empty() ? -1 : (int)std::min((size_t)selected, rows.size() - 1);
            g_modelSettingsEnvironmentRows = std::move(rows);
            RefreshModelSettingsEnvironmentList(nextSelected);
            return 0;
        }
        if (id == 203 && g_modelSettingsOpen)
        {
            if (g_modelSettingsEnvironmentOpen)
            {
                g_modelSettingsEnvironmentOpen = false;
                UpdateModelSettingsTabs();
                SetPageControls();
                Layout();
                return 0;
            }
            if (g_busy != J_NONE) return 0;
            if (!g_modelSettingsAdvanced)
            {
                if (!SyncModelSettingsFields())
                {
                    SetText(g_status, L"Correct the highlighted setting before opening Advanced JSON.");
                    return 0;
                }
                g_modelSettingsAdvanced = true;
                SetText(g_modelSettingsModeButton, L"Show native fields");
                SetText(g_modelSettingsHint, L"Advanced JSON includes arrays and maps. Preview with Maya before applying.");
            }
            else
            {
                const auto current = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
                if (current.is_discarded() || !current.is_object())
                {
                    SetText(g_status, L"Settings must be valid JSON with an object at the top level before returning to fields.");
                    return 0;
                }
                PopulateModelContextChoices(g_modelSettingsChoices, current, g_modelSettingsContextRange);
                BuildModelSettingsFields(current, g_modelSettingsChoices, g_modelData.value("gpus", Json::array()));
                LoadModelSettingsEnvironmentRows(current);
                g_modelSettingsAdvanced = false;
                SelectModelSettingsTab(g_modelSettingsTab);
                SetText(g_modelSettingsModeButton, L"Show Advanced JSON");
                UpdateModelSettingsContextHint(current);
            }
            SetPageControls();
            Layout();
            return 0;
        }
        if (id >= 210 && id <= 259 && g_modelSettingsOpen && !g_modelSettingsAdvanced)
        {
            KillTimer(h, 11);
            if (!SyncModelSettingsFields())
            {
                KillTimer(h, 11);
                g_modelSettingsPreviewValid = false;
                EnableWindow(g_modelSettingsPreviewButton, FALSE);
                EnableWindow(g_modelSettingsSave, FALSE);
                SetText(g_modelSettingsHint, L"Enter a valid number or choose a value. Preview is disabled until fields are valid.");
                UpdateModelSettingsTabs();
            }
            else
            {
                const auto draft = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
                if (!draft.is_discarded() && draft.is_object() && CurrentModelSettingsChanged(draft))
                    SetTimer(h, 11, 300, nullptr);
            }
            if (HIWORD(w) == BN_CLICKED && !NativeModelAblationAvailable(g_modelSettingsChoices))
                for (const auto &field : g_modelSettingFields)
                    if (field.id == id && field.key == "ablate" &&
                        SendMessageW(field.control, BM_GETCHECK, 0, 0) != BST_CHECKED)
                        EnableWindow(field.control, FALSE);
            return 0;
        }
        if (id == 164 && HIWORD(w) == EN_CHANGE)
        {
            KillTimer(h, 11);
            g_modelSettingsPreviewValid = false;
            EnableWindow(g_modelSettingsSave, FALSE);
            const auto draft = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
            if (g_modelSettingsOpen && !draft.is_discarded() && draft.is_object())
            {
                UpdateModelSettingsContextHint(draft);
                if (CurrentModelSettingsChanged(draft)) SetTimer(h, 11, 300, nullptr);
            }
            if (g_modelSettingsOpen) UpdateModelSettingsTabs();
            if (g_modelSettingsOpen) SetText(g_status, L"Settings changed; checking the launch command with Maya…");
            return 0;
        }
        if (id == 166)
        {
            RequestModelSettingsPreview();
            return 0;
        }
        if (id == 167)
        {
            KillTimer(h, 11);
            const auto draft = Json::parse(ControlText(g_modelSettingsEdit), nullptr, false);
            if (!g_modelSettingsOpen || draft.is_discarded() || !draft.is_object() ||
                !NativeModelSettingsApplyEnabled(CurrentModelSettingsChanged(draft), g_modelSettingsPreviewValid,
                                                 ControlText(g_modelSettingsEdit) == g_modelSettingsPreviewText) ||
                (g_busy != J_NONE && !NativeModelSettingsRequestMayOverlapChat(
                    g_busy == J_CHAT, g_modelSettingsRequestInFlight != J_NONE)))
                return 0;
            bool running = false;
            for (const auto &model : g_modelData.value("models", Json::array()))
                if (model.value("file", std::string()) == g_modelSettingsFile)
                    running = model.value("running", false);
            const bool restarts = running && g_modelData.value("restarts", false);
            std::wstring prompt = L"Save these validated settings to " + W(g_modelSettingsFile) +
                L" and restart the running model? Maya usually needs a few minutes to reload it. Your chats stay saved; "
                L"the next answer reads this chat again. API clients may need to retry while Maya restarts.";
            if (g_busy == J_CHAT)
                prompt += L" The current answer finishes first.";
            if (restarts && MessageBoxW(h, prompt.c_str(), L"Apply engine settings",
                                        MB_YESNO | MB_ICONWARNING) != IDYES)
                return 0;
            g_modelSettingsRequest["dry_run"] = false;
            EnableModelSettingsInputs(false);
            EnableWindow(g_modelSettingsModeButton, FALSE);
            EnableWindow(g_modelSettingsPreviewButton, FALSE);
            EnableWindow(g_modelSettingsCancel, FALSE);
            EnableWindow(g_modelSettingsSave, FALSE);
            BeginJob(J_MODEL_SETTINGS_SAVE);
            return 0;
        }
        if (id == 168)
        {
            KillTimer(h, 11);
            g_modelSettingsOpen = false;
            g_modelSettingsEnvironmentOpen = false;
            g_modelSettingsEnvironmentRows.clear();
            g_modelSettingsPreviewValid = false;
            g_modelSettingsFile.clear();
            g_modelSettingsSetup = Json();
            SetPageControls();
            DestroyModelSettingsFields();
            Layout();
            return 0;
        }
        if (id == 143)
        {
            if (g_catalogSel >= 0 && g_catalogData.contains("models") && g_catalogData["models"].is_array() &&
                g_catalogSel < (int)g_catalogData["models"].size() &&
                CatalogCanStartPull(g_catalogData["models"][g_catalogSel], g_pullActive,
                                    g_pullRequestInFlight, g_catalogUnavailable))
            {
                auto ms = g_catalogData["models"];
                if (g_catalogSel < (int)ms.size())
                {
                    auto model = ms[g_catalogSel].value("id", std::string());
                    auto profile = g_profile;
                    g_pullRequestInFlight = true;
                    UpdateCatalogDownloadButton();
                    SetText(g_status, L"Starting model download…");
                    SetPullProgress(Json{{"state", "starting"}});
                    std::thread([model, profile] {
                        try
                        {
                            MayaHttp a(Configuration(g_root, profile));
                            auto j = a.Post(L"/api/pull", {{"model", model}, {"detach", true}});
                            PostDone(J_PULL, j);
                        }
                        catch (const std::exception &e)
                        {
                            PostDone(J_PULL, {}, e.what());
                        }
                    }).detach();
                }
            }
            return 0;
        }
        if (id == 145)
        {
            if (g_busy != J_NONE || !g_pullActive)
                return 0;
            if (MessageBoxW(h, L"What is downloaded stays; the next download continues from it.",
                            L"Cancel the download?", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                return 0;
            BeginJob(J_PULL_CANCEL);
            SetText(g_status, L"Cancelling model download…");
            return 0;
        }
        if (id == 144)
        {
            BeginJob(J_MODELS);
            if (g_busy == J_MODELS && ModelCatalogVisible(g_modelsAddOpen, g_pullActive))
                g_catalogFetchPending = true;
            return 0;
        }
        if (id == 281 && HIWORD(w) == EN_CHANGE)
        {
            UpdateBudgetSave();
            return 0;
        }
        if (id == 280 && HIWORD(w) == BN_CLICKED)
        {
            if (g_busy != J_NONE || !g_budgetSupported || !g_contextData.contains("budget")) return 0;
            try
            {
                const int budget = NativeSettingsInteger(W(ControlText(g_budgetEdit)));
                if (!NativeThinkingBudgetValid(budget))
                    throw std::runtime_error("Use 0 (no limit) or 256 to 262144 tokens.");
                g_budgetRequest = budget;
                BeginJob(J_BUDGET_SAVE);
                SetText(g_settingsStatus, L"Saving thinking budget…");
            }
            catch (const std::exception &e)
            {
                SetText(g_settingsStatus, W(e.what()));
            }
            return 0;
        }
        if (id == 151)
        {
            if (g_busy != J_NONE || !g_contextData.contains("context") || !g_contextData.contains("port") ||
                !g_contextData.contains("budget"))
            {
                SetText(g_settingsStatus, L"Load current Maya settings first, or wait for the current action.");
                return 0;
            }
            try
            {
                auto readInt = [](HWND edit) {
                    const int n = GetWindowTextLengthW(edit);
                    if (n <= 0 || n > 16)
                        throw std::runtime_error("Enter a whole number in each settings field.");
                    std::wstring text(n + 1, L'\0');
                    GetWindowTextW(edit, text.data(), n + 1);
                    text.resize(n);
                    return NativeSettingsInteger(text);
                };
                const int contextIndex = (int)SendMessageW(g_contextEdit, CB_GETCURSEL, 0, 0);
                if (contextIndex == CB_ERR)
                    throw std::runtime_error("Choose one of Maya's supported context sizes.");
                const LRESULT contextItem = SendMessageW(g_contextEdit, CB_GETITEMDATA, contextIndex, 0);
                if (contextItem == CB_ERR)
                    throw std::runtime_error("The selected context size is invalid; reload the settings list.");
                const int context = (int)contextItem;
                const int port = readInt(g_portEdit);
                const int budget = readInt(g_budgetEdit);
                const auto contextInfo = g_contextData["context"];
                const auto portInfo = g_contextData["port"];
                const auto budgetInfo = g_contextData["budget"];
                const bool changeContext = context != contextInfo.value("context", 0);
                const bool changePort = port != portInfo.value("port", 0);
                if (budget != budgetInfo.value("budget", 0) && !budgetInfo.value("supported", false))
                    throw std::runtime_error("This model does not support a thinking budget.");
                if (changeContext || changePort)
                {
                    std::wstring warning = changeContext
                        ? NativeContextReloadWarning(contextInfo, context,
                            g_contextTokensKnown ? g_contextTokens : 0,
                            g_contextTokensApproximate)
                        : std::wstring();
                    if (changePort)
                    {
                        if (!warning.empty()) warning += L"\n\n";
                        warning += L"Changing the port restarts Maya and updates its saved configuration. "
                            L"API clients will need to use the new address.";
                    }
                    if (MessageBoxW(h, warning.c_str(), L"Apply Maya settings",
                                    MB_YESNO | MB_ICONWARNING) != IDYES)
                        return 0;
                }

                auto profile = g_profile;
                g_busy = J_SAVE_SETTINGS;
                UpdateChatActionButtons();
                EnableWindow(g_settingsSave, FALSE);
                SetText(g_settingsStatus, L"Validating settings and applying changes in order…");
                std::thread([context, port, budget, profile] {
                    try
                    {
                        MayaHttp api(Configuration(g_root, profile));
                        PostDone(J_SAVE_SETTINGS, ApplySettings(api, context, port, budget));
                    }
                    catch (const std::exception &e)
                    {
                        PostDone(J_SAVE_SETTINGS, {}, e.what());
                    }
                }).detach();
            }
            catch (const std::exception &e)
            {
                SetText(g_settingsStatus, W(e.what()));
            }
            return 0;
        }
        if (id == NativeMcpToolsControlId)
        {
            const bool previous = g_useTools;
            g_useTools = SendMessageW(g_toolsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
            {
                g_useTools = previous;
                SendMessageW(g_toolsCheck, BM_SETCHECK, g_useTools ? BST_CHECKED : BST_UNCHECKED, 0);
                return 0;
            }
            SetText(g_settingsStatus, g_useTools ? L"MCP tools enabled for new requests." : L"MCP tools disabled for new requests.");
            ScheduleContextMeter();
            return 0;
        }
        if ((id == 193 || id == 194) && HIWORD(w) == CBN_SELCHANGE)
        {
            const auto oldTheme = g_themeChoice;
            const auto oldScheme = g_schemeChoice;
            if (id == 193)
            {
                const int choice = (int)SendMessageW(g_themeBox, CB_GETCURSEL, 0, 0);
                g_themeChoice = choice == 0 ? "system" : choice == 2 ? "light" : "dark";
            }
            else
                g_schemeChoice = SendMessageW(g_schemeBox, CB_GETCURSEL, 0, 0) == 1 ? "violet" : "default";
            ApplyAppearance();
            if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
            {
                g_themeChoice = oldTheme;
                g_schemeChoice = oldScheme;
                SendMessageW(g_themeBox, CB_SETCURSEL, ChatThemeComboIndex(g_themeChoice), 0);
                SendMessageW(g_schemeBox, CB_SETCURSEL, ChatSchemeComboIndex(g_schemeChoice), 0);
                ApplyAppearance();
            }
            else SetText(g_settingsStatus, L"Appearance saved for Maya Native Manager.");
            return 0;
        }
        if (id == 174 && HIWORD(w) == EN_CHANGE &&
            NativeInstructionAutosaveNeeded(GetFocus() == g_defaultInstructions,
                                            ControlText(g_defaultInstructions) != g_defaultSystem))
        {
            g_defaultSystem = ControlText(g_defaultInstructions);
            const bool currentChatUsesDefault = g_chat >= 0 && g_chat < (int)g_chats.size() &&
                                                g_chats[g_chat].messages.empty();
            if (currentChatUsesDefault)
                g_chats[g_chat].system = g_defaultSystem;
            if (!SaveChatPreferences(g_defaultSystem, g_showThinking))
                SetText(g_settingsStatus, L"Instructions changed for this session, but could not be saved to disk.");
            if (currentChatUsesDefault) ScheduleContextMeter();
            return 0;
        }
        if (id == 175)
        {
            const int n = GetWindowTextLengthW(g_defaultInstructions);
            std::wstring value((size_t)n + 1, L'\0');
            GetWindowTextW(g_defaultInstructions, value.data(), n + 1);
            value.resize((size_t)n);
            const auto system = U(value);
            if (!SaveChatPreferences(system, g_showThinking)) return 0;
            g_defaultSystem = system;
            if (g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].messages.empty())
            {
                g_chats[g_chat].system = system;
                SaveChatMetadata(g_chats[g_chat]);
            }
            SetText(g_settingsStatus, L"New-chat instructions saved.");
            return 0;
        }
        if (id == 192)
        {
            if (g_busy != J_NONE)
            {
                SetText(g_status, L"Wait for Maya's current action to finish before clearing local chats.");
                return 0;
            }
            if (!IsNativeChatDataPath(g_userData, g_chatDir))
            {
                SetText(g_status, L"Refusing to clear data outside the Maya Manager chat folder.");
                return 0;
            }
            if (MessageBoxW(h,
                (L"Move native chats, folders, chat preferences, and attached copies to the Recycle Bin?\r\n\r\n" +
                 g_chatDir + L"\r\n\r\nAppearance choices and Maya's model/engine settings are kept. The local API-key override is cleared; profile API keys remain.").c_str(),
                L"Clear native chats and settings", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES)
                return 0;
            const auto clearResult = NativeClearLocalData(
                [] {
                    if (!ClearMayaApiKeyOverride()) return false;
                    g_api.reset();
                    return true;
                },
                [] { return RecyclePathToBin(g_chatDir); });
            if (clearResult == NativeClearDataResult::API_KEY_FAILED)
            {
                SetText(g_status, L"Windows could not clear the API-key override; no chat data was moved.");
                return 0;
            }
            if (clearResult == NativeClearDataResult::CHAT_DATA_FAILED)
            {
                SetText(g_status, L"Windows could not move all chat data to the Recycle Bin. The API-key override was cleared; check the chat folder and Recycle Bin before retrying.");
                return 0;
            }
            KillTimer(h, 8);
            g_pendingChatDeletes.clear();
            Layout();
            try
            {
                fs::create_directories(g_chatDir);
                g_chats.clear();
                g_folders.clear();
                g_samplingPrefs = Json::object();
                g_turboSnapshot = nullptr;
                g_useTools = true;
                g_monitorGpuOpen = -1;
                g_monitorGpuSelected = -1;
                g_monitorGpuOpenKey.clear();
                g_sidebarOpen = true;
                g_defaultSystem.clear();
                g_showThinking = true;
                g_chat = -1;
                g_savedCurrentChat.clear();
                g_savedModelId.clear();
                g_editMessage = -1;
                g_attachments.clear();
                g_attachmentSnapshots.clear();
                g_expandedThoughts.clear();
                g_collapsedThoughts.clear();
                SaveFolders();
                const bool preferencesSaved = SaveChatPreferences(g_defaultSystem, g_showThinking);
                SendMessageW(g_themeBox, CB_SETCURSEL, ChatThemeComboIndex(g_themeChoice), 0);
                SendMessageW(g_schemeBox, CB_SETCURSEL, ChatSchemeComboIndex(g_schemeChoice), 0);
                ApplyAppearance();
                SetText(g_defaultInstructions, L"");
                SendMessageW(g_showThinkingCheck, BM_SETCHECK, BST_CHECKED, 0);
                SendMessageW(g_toolsCheck, BM_SETCHECK, BST_CHECKED, 0);
                LoadChatDefaultsControls();
                NewChat();
                RefreshChatList();
                RenderChat();
                Layout();
                InvalidateRect(h, nullptr, TRUE);
                SetText(g_status, preferencesSaved
                    ? L"Native chats and local chat settings were moved to the Recycle Bin."
                    : L"Native chats were moved to the Recycle Bin, but default preferences could not be recreated.");
            }
            catch (const std::exception &e)
            {
                SetText(g_status, W(e.what()));
            }
            return 0;
        }
        if (id == 176)
        {
            if (g_busy == J_CHAT)
            {
                SendMessageW(g_showThinkingCheck, BM_SETCHECK, g_showThinking ? BST_CHECKED : BST_UNCHECKED, 0);
                SetText(g_settingsStatus, L"Change reasoning visibility after the current answer finishes.");
                return 0;
            }
            const bool show = SendMessageW(g_showThinkingCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (!SaveChatPreferences(g_defaultSystem, show))
            {
                SendMessageW(g_showThinkingCheck, BM_SETCHECK, g_showThinking ? BST_CHECKED : BST_UNCHECKED, 0);
                return 0;
            }
            g_showThinking = show;
            RenderChat();
            SetText(g_settingsStatus, show ? L"Thinking will be shown in chat." : L"Thinking is hidden; it remains saved with the chat.");
            return 0;
        }
        if (id >= 181 && id <= 189 && HIWORD(w) == EN_CHANGE && !g_loadingChatDefaults)
        {
            if (id == 181) UpdateSamplingFilterControls();
            SendMessageW(g_samplingPreset, CB_SETCURSEL, 3, 0);
            SaveSamplingDefaultsFromControls(false);
            return 0;
        }
        if (id == 190 && HIWORD(w) == BN_CLICKED)
        {
            if (g_busy != J_NONE)
            {
                SendMessageW(g_sharedDefaultsCheck, BM_SETCHECK,
                             g_sharedDefaultsOn ? BST_CHECKED : BST_UNCHECKED, 0);
                SetText(g_samplingStatus, L"Wait for Maya's current action to finish.");
                return 0;
            }
            const bool enable = SendMessageW(g_sharedDefaultsCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            try
            {
                g_sharedRequest = enable ? SharedChatDefaults(ReadChatDefaultsControls()) : Json(nullptr);
                EnableWindow(g_sharedDefaultsCheck, FALSE);
                SetText(g_samplingStatus, L"Saving API defaults…");
                BeginJob(J_SHARED_SAVE);
            }
            catch (const std::exception &e)
            {
                SendMessageW(g_sharedDefaultsCheck, BM_SETCHECK,
                             g_sharedDefaultsOn ? BST_CHECKED : BST_UNCHECKED, 0);
                SetText(g_samplingStatus, W(e.what()));
            }
            return 0;
        }
        if (id == 179 && HIWORD(w) == CBN_SELCHANGE && !g_loadingChatDefaults)
        {
            const int preset = (int)SendMessageW(g_samplingPreset, CB_GETCURSEL, 0, 0);
            if (preset >= 0 && preset < 3)
            {
                g_loadingChatDefaults = true;
                for (int i = 0; i < 4; ++i)
                    if (const auto value = ChatSamplingPresetValue(preset, i)) SetText(g_samplingFields[i], value);
                g_loadingChatDefaults = false;
                SaveSamplingDefaultsFromControls(false);
            }
            return 0;
        }
        if (id == 180 && HIWORD(w) == CBN_SELCHANGE && !g_loadingChatDefaults)
        {
            SaveSamplingDefaultsFromControls(true);
            return 0;
        }
        if (id == 177)
        {
            SaveSamplingDefaultsFromControls(true);
            return 0;
        }
        if (id == 178)
        {
            const auto family = ChatSamplingFamily(g_health.value("model", std::string()));
            const auto previousSampling = g_samplingPrefs;
            if (g_samplingPrefs.is_object()) g_samplingPrefs.erase(family);
            if (SaveChatPreferences(g_defaultSystem, g_showThinking))
            {
                LoadChatDefaultsControls();
                const auto values = ResolveChatSampling(g_samplingPrefs, g_health.value("model", std::string()),
                                                        g_health.value("defaults", Json::object()));
                if (g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].messages.empty())
                {
                    g_chats[g_chat].thinking = values.value("reasoning_effort", std::string("high"));
                    g_chats[g_chat].thinkingFor = g_health.value("model", std::string());
                    SaveChatMetadata(g_chats[g_chat]);
                    ApplyChatThinking();
                }
                SetText(g_samplingStatus, L"Back to this model's recommended defaults.");
                if (g_sharedDefaultsOn)
                {
                    g_sharedRequest = SharedChatDefaults(values);
                    SetText(g_samplingStatus, L"Local defaults reset; updating API defaults…");
                    BeginJob(J_SHARED_SAVE);
                }
            }
            else g_samplingPrefs = previousSampling;
            return 0;
        }
        if (id == 120 && HIWORD(w) == LBN_SELCHANGE)
        {
            const int visible = (int)SendMessageW(g_chatList, LB_GETCURSEL, 0, 0);
            if (visible < 0 || visible >= (int)g_visibleChats.size())
                return 0;
            const auto row = g_visibleChats[visible];
            if (row.chat < 0)
            {
                if (row.folder >= 0)
                {
                    g_folders[row.folder].collapsed = !g_folders[row.folder].collapsed;
                    SaveFolders();
                    RefreshChatList();
                }
                return 0;
            }
            if (g_editMessage >= 0)
            {
                if (MessageBoxW(h, L"Discard the unsent edit and switch chats?", L"Switch chat",
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                    return 0;
                CancelChatEdit();
            }
            g_chat = row.chat;
            if (NativeChatSearchClearOnSelect(W(ControlText(g_chatSearch))))
            {
                SetWindowTextW(g_chatSearch, L"");
                RefreshChatList();
            }
            SaveCurrentChatSelection();
            ApplyChatThinking();
            RenderChat();
            ScheduleContextMeter();
            ResumePendingChat();
            return 0;
        }
    }
    if (m == WM_DONE)
    {
        std::unique_ptr<Done> d((Done *)l);
        if (d->job == J_MCP)
        {
            g_mcpPollInFlight = false;
            if (g_busy == J_MCP) g_busy = J_NONE;
            UpdateChatActionButtons();
            if (d->error.empty())
            {
                g_mcpData = d->value;
                SetText(g_mcpDetails, W(McpStatusReport(g_mcpData)));
            }
            else
                SetText(g_mcpDetails, L"MCP status could not be loaded. Maya will retry automatically.");
            return 0;
        }
        if (d->job == J_MODEL_SETTINGS_PREVIEW || d->job == J_MODEL_SETTINGS_SAVE)
        {
            g_modelSettingsRequestInFlight = J_NONE;
            if (g_busy == d->job)
                g_busy = J_NONE;
        }
        else if (d->job == J_CHAT && g_modelSettingsRequestInFlight != J_NONE)
            g_busy = g_modelSettingsRequestInFlight;
        else if (d->job != J_TOKENS && d->job != J_SWITCH &&
                 !(d->backgroundPoll && (d->job == J_UPDATE_CHECK || d->job == J_TURBO_INFO)))
            g_busy = J_NONE;
        UpdateChatActionButtons();
        if (d->job == J_UPDATE_CHECK) g_updateCheckInFlight = false;
        if (d->job == J_TURBO_INFO) g_turboInfoInFlight = false;
        UpdateModelSwitchButton();
        if (d->job == J_RESTART)
        {
            KillTimer(h, NativeRestartTimerId);
            g_restartStartedAt = 0;
        }
        if (d->job == J_MODEL_DELETE_PLAN && !d->error.empty() && !g_modelSheetPlanPreview)
        {
            d->value = ModelDeletePlanFailure(d->error);
            d->error.clear();
        }
        if (!d->error.empty())
        {
            if (d->job == J_MODEL_DELETE_PLAN && g_modelSheetPlanPreview)
            {
                g_modelSheetPlanPreview = false;
                g_modelSheetPlanFile = g_deleteFile;
                g_modelSheetPlan = Json::object();
                g_modelSheetPlanError = "Could not inspect this model's files: " + d->error;
                UpdateModelSheetView();
                SetText(g_status, W(g_modelSheetPlanError));
            }
            if (d->job == J_BUDGET_SAVE)
            {
                SetText(g_settingsStatus, L"Thinking budget did not change: " + W(d->error));
                UpdateBudgetSave();
            }
            if (d->job == J_PULL)
            {
                g_pullRequestInFlight = false;
                UpdateCatalogDownloadButton();
                SetText(g_status, L"Model download did not start: " + W(d->error));
                BeginJob(J_CATALOG);
            }
            if (d->job == J_PULL_CANCEL)
            {
                SetText(g_status, W(PullCancelErrorText(d->error)));
                BeginJob(J_CATALOG);
            }
            if (d->job == J_LOAD || d->job == J_UNLOAD)
            {
                UpdateModelLoadButton();
                g_modelActionError = ModelOperationErrorText(d->job == J_LOAD, W(d->error));
                BeginJob(J_MODELS);
            }
            if (d->job == J_UPDATE_CHECK) SetUpdateAvailable(false);
            if (d->job == J_UPDATE_CHECK)
            {
                g_updateStartAfterCheck = false;
                g_updateAutoStartAfterCheck = false;
                const bool recoveringUpdateFailure = g_updateRecovering;
                g_updateRecovering = false;
                g_updateChecked = true;
                g_updateCheckResult = Json::object();
                g_updateCanInstall = false;
                g_updateInstallVisible = false;
                g_updateSummaryText = recoveringUpdateFailure
                    ? "Maya returned on the previous version, but Win-Manager could not retrieve the updater's failure reason: " + d->error
                    : "Couldn't check for Maya updates: " + d->error;
                SetText(g_updateSummary, W(g_updateSummaryText));
                ShowWindow(g_updateInstall, SW_HIDE);
                ShowWindow(g_updateNotes, g_updateNotesUrl.empty() ? SW_HIDE : SW_SHOW);
                SetText(g_releaseNotes, W(g_latestReleaseNotesText));
                if (g_page == ABOUT) Layout();
            }
            if (d->job == J_UPDATE_START)
            {
                g_updateWatching = false;
                g_updateDown = false;
                g_updateCanInstall = false;
                g_updateInstallVisible = false;
                g_updateSummaryText = "The update did not start: " + d->error;
                SetText(g_updateSummary, W(g_updateSummaryText));
                ShowWindow(g_updateInstall, SW_HIDE);
                SetLifecycleButtons(g_online);
                if (g_page == ABOUT) Layout();
                if (!g_online) BeginJob(J_STATUS);
            }
            if (d->job == J_REFRESH)
            {
                g_metricsInFlight = false;
                const auto previousConnection = g_apiConnection;
                const auto status = NativeMetricsHttpStatusOrZero(d->value);
                g_apiConnection = NativeApiConnectionAfterMetricsPoll(g_apiConnection, g_metricPollFailures, status, false);
                UpdateMetricsPollTimer(IsIconic(h) != FALSE, GetForegroundWindow() == h);
                if (previousConnection != g_apiConnection)
                {
                    SetText(g_status, StatusText());
                    UpdateGreeting();
                }
            }
            if (d->job == J_REFRESH && g_page == MONITOR && UpdateMonitorVramHitExpiryTimer())
                InvalidateMonitorThroughput(h);
            if (d->job == J_MODEL_SETTINGS_PREVIEW || d->job == J_MODEL_SETTINGS_SAVE)
                SetText(g_modelSettingsPreview, W(d->error));
            else if (NativePollShowsError(d->backgroundPoll,
                                          d->job == J_STATUS || d->job == J_REFRESH || d->job == J_PULL_STATUS ||
                                              d->job == J_MODELS))
                SetText(g_status, W(d->error));
            if (d->job == J_MODELS && !g_modelActionError.empty())
            {
                SetText(g_status, ModelOperationRefreshErrorText(g_modelActionError));
                g_modelActionError.clear();
            }
            if (d->job == J_MODELS && g_modelDeleteOutcomePending)
                SetText(g_status, L"Maya returned, but the model list could not be refreshed. Refresh Models to verify the deletion.");
            if (d->job == J_CATALOG)
            {
                g_catalogUnavailable = true;
                SetText(g_status, W(CatalogUnavailableStatus(d->error)));
                g_catalogSel = -1;
                SendMessageW(g_catalog, LB_RESETCONTENT, 0, 0);
                SendMessageW(g_catalog, LB_ADDSTRING, 0, (LPARAM)L"Catalog unavailable · see status and retry Refresh models");
                if (!ModelCatalogHasSnapshot(g_catalogData))
                    g_catalogData["models"] = Json::array();
                UpdateCatalogDownloadButton();
            }
            if (d->job == J_MODEL_SETTINGS_PREVIEW || d->job == J_MODEL_SETTINGS_SAVE)
            {
                EnableModelSettingsInputs(true);
                EnableWindow(g_modelSettingsModeButton, TRUE);
                EnableWindow(g_modelSettingsPreviewButton, TRUE);
                EnableWindow(g_modelSettingsCancel, TRUE);
                EnableWindow(g_modelSettingsSave, d->job == J_MODEL_SETTINGS_SAVE && g_modelSettingsPreviewValid &&
                    ControlText(g_modelSettingsEdit) == g_modelSettingsPreviewText);
            }
            if (d->job == J_MODEL_LEFTOVER_DELETE)
            {
                SetText(g_status, L"Unused model files were not deleted: " + W(d->error));
                UpdateLeftoverDetails();
                BeginJob(J_MODELS);
            }
            if (d->job == J_MODEL_DELETE)
                SetText(g_status, W(ModelDeleteOutcomeText(Json{{"file", g_deleteFile}, {"error", d->error}})));
            if (d->job == J_REPORT && g_aboutReportRequest)
            {
                if (g_page == ABOUT)
                {
                    EnableWindow(GetDlgItem(h, 260), TRUE);
                    EnableWindow(GetDlgItem(h, 210), TRUE);
                    SetWindowTextW(GetDlgItem(h, 260), g_aboutLogVisible ? L"Hide report" : L"Show report");
                }
                SetText(g_status, W(d->error));
                g_aboutCopyRequested = false;
                g_aboutReportRequest = false;
            }
            if (d->job == J_SAVE_SETTINGS)
            {
                EnableWindow(g_settingsSave, TRUE);
                SetText(g_settingsStatus, W(d->error));
                BeginJob(J_CONTEXT);
            }
            if (d->job == J_SHARED_SAVE)
            {
                EnableWindow(g_sharedDefaultsCheck, TRUE);
                SendMessageW(g_sharedDefaultsCheck, BM_SETCHECK, g_sharedDefaultsOn ? BST_CHECKED : BST_UNCHECKED, 0);
                SetText(g_samplingStatus, W(d->error));
            }
            if (d->job == J_SWITCH)
            {
                g_modelSwitchRequestInFlight = false;
                SetLifecycleButtons(g_online);
                SetText(g_status, L"The model did not change: " + W(d->error));
            }
            if (d->job == J_STATUS)
            {
                SetUpdateAvailable(false);
                g_online = false;
                UpdateGreeting();
                UpdateConnectPage();
                SetLifecycleButtons(false);
                if (g_modelSettingsRestartWatching)
                {
                    g_modelSettingsRestartDown = true;
                    if (NativeRestartTimeoutElapsed(g_modelSettingsRestartStartedAt, GetTickCount64()))
                    {
                        g_modelSettingsRestartWatching = false;
                        g_modelSettingsRestartDown = false;
                        KillTimer(h, 9);
                        SetText(g_status, L"Maya did not return after applying the engine settings within 15 minutes.");
                    }
                    else
                        SetText(g_status, L"Maya is restarting to apply engine settings…");
                }
                if (g_updateWatching)
                {
                    g_updateDown = true;
                    SetText(g_status, L"Maya is updating to v" + W(g_updateTarget) + L"; waiting for it to return…");
                }
                if (g_turboRestartWatching)
                {
                    if (NativeRestartTimeoutElapsed(g_turboRestartStartedAt, GetTickCount64()))
                    {
                        g_turboRestartWatching = false;
                        g_turboRestartDown = false;
                        SetText(g_samplingStatus, L"Maya did not return after applying Turbo within 15 minutes.");
                    }
                    else
                    {
                        g_turboRestartDown = true;
                        SetText(g_samplingStatus, L"Maya is restarting to apply Turbo…");
                    }
                }
                if (g_switchRestartWatching)
                {
                    g_switchRestartDown = true;
                    if (NativeRestartTimeoutElapsed(g_switchStartedAt, GetTickCount64()))
                    {
                        g_switchRestartWatching = false;
                        g_switchRestartDown = false;
                        const bool deleteTimedOut = g_modelDeleteRestartWatching;
                        g_modelDeleteRestartWatching = false;
                        SetLifecycleButtons(false);
                        SetText(g_status, deleteTimedOut
                            ? L"Maya did not return after switching models within 15 minutes; the deferred deletion could not be verified."
                            : L"Maya did not return after the model switch within 15 minutes.");
                    }
                }
                if (!g_modelSettingsRestartWatching && !g_autoStartMayaAttempted &&
                    MayaWindowsStartup::ShouldStartMaya(g_windowsStartupLaunch, [&] {
                        try { return NativeAutoStartMaya(); } catch (...) { return false; }
                    }()))
                {
                    g_autoStartMayaAttempted = true;
                    BeginJob(J_START);
                }
            }
            if (d->job == J_UPDATE_CHECK && g_updateRecovering)
            {
                g_updateRecovering = false;
                SetText(g_status, L"Could not resume update tracking: " + W(d->error));
            }
            if (d->job == J_TURBO_INFO)
            {
                g_turboData = {{"unavailable", true}};
                UpdateTurboButton();
                SetText(g_samplingStatus, L"This Maya server does not provide Turbo.");
            }
            if (d->job == J_TURBO_SET)
            {
                const bool restored = !g_turboRequestOn || RestoreNativeTurboPrefs();
                g_turboData["on"] = !g_turboRequestOn;
                UpdateTurboButton();
                SetText(g_samplingStatus, !restored
                    ? L"Turbo failed and the prior settings could not be saved; they are restored in memory only. " + W(d->error)
                    : W(d->error));
            }
            if (d->job == J_START || d->job == J_STOP || d->job == J_RESTART)
            {
                SetLifecycleButtons(g_online);
                if (d->job == J_RESTART) SetText(g_status, L"Restart failed: " + W(d->error));
                LoadStatusData();
            }
            if (d->job == J_CHAT)
            {
                EnableBusy(false);
                KillTimer(h, 3);
                FlushStream();
                FinalizeMcpEvents(g_answer.tools);
                g_answer.error = g_resumingPendingJob
                    ? "The saved answer could not be resumed: " + d->error
                    : d->error;
                g_answer.stopped = g_cancelled.load();
                g_answer.meta = AnswerMetadata(g_answer.usage, g_answer.timings,
                    std::chrono::duration<double>(Clock::now() - g_actionAt).count(),
                    g_answer.stopped, g_answer.tools, g_answer.toolLimit);
                auto chat = std::find_if(g_chats.begin(), g_chats.end(),
                    [](const Chat &c) { return c.id == g_jobChatId; });
                if (chat != g_chats.end())
                {
                    if (g_resumingPendingJob)
                        g_answer.jobId = chat->pendingJob;
                    const bool keepPendingJob = NativeChatKeepPendingJobAfterFailure(
                        !g_answer.jobId.empty(), g_cancelled.load(), d->error == NativeChatResumeMissingMessage);
                    if (keepPendingJob)
                    {
                        chat->pendingJob = g_answer.jobId;
                        g_answer.error += " Maya still has this answer; the Manager will retry automatically.";
                        g_pendingChatRetryAt = GetTickCount64() + 30000;
                        SetText(g_status, L"Connection lost; the saved answer will retry automatically.");
                        SaveChatMetadata(*chat);
                    }
                    else if (!chat->pendingJob.empty() && chat->pendingJob == g_answer.jobId)
                    {
                        chat->pendingJob.clear();
                        g_pendingChatRetryAt = 0;
                        SaveChatMetadata(*chat);
                    }
                    chat->messages.push_back(g_answer);
                    if (!AppendLog(*chat, g_answer))
                        SetText(g_status, L"The failed answer could not be saved to chat history.");
                    RefreshChatList();
                }
                g_resumingPendingJob = false;
                RenderChat();
                ScheduleContextMeter();
                ContinueAfterChat();
                ResumePendingChat();
            }
            if (d->job == J_LOAD || d->job == J_UNLOAD)
                BeginJob(J_MODELS, nullptr, true);
            UpdateSupportLinkVisibility();
            return 0;
        }
        switch (d->job)
        {
        case J_STATUS: {
            const Json previousHealth = g_health;
            const auto previousMaxContext = (uint64_t)std::max<int64_t>(0, MonitorInteger(g_health, "max_context"));
            g_health = d->value;
            const bool healthChanged = previousHealth != g_health;
            const bool serverRecovered = !g_online;
            bool statusUpdateAvailable = d->value.value("update_available", false);
            bool checkedUpdateAvailable = false;
            if (g_updateChecked && g_updateInstallVisible && !g_updateFreshInstall && !g_updateWatching)
            {
                const auto checked = g_updateCheckResult.value("update", Json::object());
                const auto latest = checked.value("latest", Json::object());
                const auto latestVersion = latest.value("version", std::string());
                const auto currentVersion = d->value.value("version", std::string());
                if (!latestVersion.empty() && currentVersion == latestVersion)
                {
                    g_updateInstallVisible = false;
                    g_updateCanInstall = false;
                    statusUpdateAvailable = false;
                    g_updateSummaryText = "Maya v" + currentVersion + " is up to date.";
                    SetText(g_updateSummary, W(g_updateSummaryText));
                    ShowWindow(g_updateInstall, SW_HIDE);
                    if (g_page == ABOUT) Layout();
                }
                else
                    checkedUpdateAvailable = true;
            }
            SetUpdateAvailable(MayaUpdateBadgeVisible(statusUpdateAvailable, g_updateWatching) || checkedUpdateAvailable);
            const auto modelId = ChatModelIdentity(g_health);
            const bool modelChanged = ChatModelChanged(g_savedModelId, modelId);
            const bool statusUiNeedsRefresh = NativeStatusUiNeedsRefresh(healthChanged, serverRecovered, modelChanged);
            if (g_page == ABOUT && healthChanged) UpdateAboutFacts();
            if (statusUiNeedsRefresh) UpdateContextMeterVisibility();
            if (ChatModelListNeedsRefreshAfterStatus(modelChanged, g_switchRestartWatching))
                BeginJob(J_MODELS, nullptr, true);
            const bool modelIdentityNeedsSave = !modelId.empty() && g_savedModelId != modelId;
            if (modelIdentityNeedsSave)
            {
                g_savedModelId = modelId;
                if (modelChanged)
                {
                    g_samplingPrefs = ResetChatSamplingForModelChange(g_samplingPrefs);
                    if (g_chat >= 0 && g_chat < (int)g_chats.size() && g_chats[g_chat].messages.empty())
                    {
                        auto &chat = g_chats[g_chat];
                        const auto defaults = ResolveChatSampling(g_samplingPrefs, g_health.value("model", std::string()),
                                                                  g_health.value("defaults", Json::object()));
                        chat.thinking = defaults.value("reasoning_effort", std::string("high"));
                        chat.thinkingFor = g_health.value("model", std::string());
                        SaveChatMetadata(chat);
                    }
                }
                if (!SaveChatPreferences(g_defaultSystem, g_showThinking) && modelChanged)
                    SetText(g_samplingStatus, L"The model changed, but its sampling defaults could not be saved locally.");
                else if (modelChanged)
                    SetText(g_samplingStatus, L"Model changed. Custom sampling choices were cleared; this model's defaults are active.");
            }
            if (statusUiNeedsRefresh) UpdateConnectPage();
            const bool refreshModelUi = modelChanged || serverRecovered;
            g_online = true;
            if (refreshModelUi)
            {
                if (g_page == CHAT_DEFAULTS) LoadChatDefaultsControls();
                ApplyChatThinking();
            }
            if (g_page == MONITOR && healthChanged)
                UpdateMonitorView(&g_metrics, &previousHealth);
            g_images = g_health.value("images", false);
            ApplyChatAblation();
            if (statusUiNeedsRefresh)
            {
                UpdateGreeting();
                SetLifecycleButtons(true);
                SetText(g_status, modelChanged ? L"Model changed; its default sampling settings are active."
                    : W("Maya v" + g_health.value("version", std::string("?")) + " · " +
                        g_health.value("model_label", g_health.value("model", std::string("model")))));
            }
            if (g_updateWatching)
            {
                const auto version = g_health.value("version", std::string());
                const auto state = g_health.value("status", std::string());
                const auto outcome = MayaUpdateWatchStep(true, version, g_updateTarget, g_updateDown, state);
                if (outcome == MayaUpdateWatchState::COMPLETE)
                {
                    g_updateWatching = false;
                    g_updateDown = false;
                    SetText(g_status, W("Maya updated to v" + version + "."));
                    SetLifecycleButtons(true);
                }
                else if (outcome == MayaUpdateWatchState::FAILED)
                {
                    g_updateWatching = false;
                    g_updateDown = false;
                    g_updateRecovering = true;
                    SetText(g_status, L"Maya returned on the previous version; checking the updater's failure reason…");
                    SetLifecycleButtons(true);
                    BeginJob(J_UPDATE_CHECK, L"update-check", true);
                }
                else
                    SetText(g_status, L"Maya is updating to v" + W(g_updateTarget) + L"…");
            }
            SetText(g_detail, L"Open Maya · " + HeaderApiAddress() +
                                  (g_images ? L" · Vision ON" : L" · Vision OFF") + L" · " +
                                  std::to_wstring(g_health.value("max_context", 0) / 1000) + L"K context");
            SetText(g_hwnd, NativeManagerTitle(W(g_health.value("version", std::string())), g_images,
                                               MonitorInteger(g_health, "max_context")));
            if (g_switchRestartWatching)
            {
                const auto step = ModelSwitchWatchStep(g_switchRestartDown, true,
                    g_health.value("status", std::string()) == "ok",
                    NativeRestartTimeoutElapsed(g_switchStartedAt, GetTickCount64()));
                if (step == ModelSwitchWatchState::COMPLETE)
                {
                    g_switchRestartWatching = false;
                    g_switchRestartDown = false;
                    const bool checkingDelete = g_modelDeleteRestartWatching;
                    g_modelDeleteRestartWatching = false;
                    SetText(g_status, checkingDelete
                        ? L"Maya returned after switching models; checking the deferred deletion result…"
                        : W("Maya returned after switching to " + g_switchTarget + "."));
                    SetLifecycleButtons(true);
                    // Refresh the cached running-profile marker even when Chat is open.
                    BeginJob(J_MODELS, nullptr, true);
                }
                else if (step == ModelSwitchWatchState::TIMED_OUT)
                {
                    g_switchRestartWatching = false;
                    g_switchRestartDown = false;
                    const bool deleteTimedOut = g_modelDeleteRestartWatching;
                    g_modelDeleteRestartWatching = false;
                    SetText(g_status, deleteTimedOut
                        ? L"Maya did not return after switching models within 15 minutes; the deferred deletion could not be verified."
                        : L"Maya did not return after the model switch within 15 minutes.");
                    SetLifecycleButtons(true);
                }
            }
            if (g_modelSettingsRestartWatching)
            {
                const auto step = ModelSwitchWatchStep(g_modelSettingsRestartDown, true,
                    g_health.value("status", std::string()) == "ok",
                    NativeRestartTimeoutElapsed(g_modelSettingsRestartStartedAt, GetTickCount64()));
                if (step == ModelSwitchWatchState::COMPLETE)
                {
                    g_modelSettingsRestartWatching = false;
                    g_modelSettingsRestartDown = false;
                    KillTimer(h, 9);
                    g_refreshModelsWhenIdle = g_page == MODELS;
                    SetText(g_status, L"Maya returned; the engine settings are applied.");
                    SetLifecycleButtons(true);
                }
                else if (step == ModelSwitchWatchState::TIMED_OUT)
                {
                    g_modelSettingsRestartWatching = false;
                    g_modelSettingsRestartDown = false;
                    KillTimer(h, 9);
                    SetText(g_status, L"Maya did not return after applying the engine settings within 15 minutes.");
                }
                else
                    SetText(g_status, L"Waiting for Maya to reload the engine settings…");
            }
            if (g_turboRestartWatching && g_turboRestartDown)
            {
                g_turboRestartWatching = false;
                g_turboRestartDown = false;
                SetText(g_samplingStatus, L"Maya returned after applying Turbo.");
                BeginJob(J_TURBO_INFO);
            }
            else if (g_page == CHAT_DEFAULTS && !g_turboData.contains("on") &&
                     !g_turboData.value("unavailable", false))
                BeginJob(J_TURBO_INFO);
            const auto currentMaxContext = (uint64_t)std::max<int64_t>(0, MonitorInteger(g_health, "max_context"));
            if (ContextMeterNeedsRefresh(modelChanged, previousMaxContext, currentMaxContext, serverRecovered))
                ScheduleContextMeter();
            StartQueuedModelRefreshWhenIdle();
            ResumePendingChat();
            TrySendStartupQuestion();
            if (NativeTurboInfoEligible(g_turboData.contains("on") || g_turboData.value("unavailable", false),
                                        g_turboInfoInFlight))
                BeginJob(J_TURBO_INFO, nullptr, true);
            const bool updating = g_health.contains("status") && g_health["status"].is_string() &&
                                  g_health["status"].get<std::string>() == "updating";
            if (!updating) g_updateRecoveryAttempted = false;
            else if (!g_updateWatching && !g_updateRecovering && !g_updateRecoveryAttempted && g_busy == J_NONE)
            {
                g_updateRecovering = true;
                g_updateRecoveryAttempted = true;
                BeginJob(J_UPDATE_CHECK, L"update-check");
            }
            CheckForUpdatesIfNeeded();
            if (!g_mcpLoadAttempted && g_busy == J_NONE)
            {
                g_mcpLoadAttempted = true;
                BeginJob(J_MCP, nullptr, true);
            }
            break;
        }
        case J_START:
        case J_STOP:
        case J_RESTART:
            SetLifecycleButtons(g_online);
            SetText(g_status, JsonText(d->value, "status", "Lifecycle action finished."));
            LoadStatusData();
            break;
        case J_REFRESH: {
            g_metricsInFlight = false;
            if (!NativeMetricsPayloadIsObject(d->value))
            {
                const auto previousConnection = g_apiConnection;
                g_apiConnection = NativeApiConnectionAfterMetricsPoll(g_apiConnection, g_metricPollFailures, 0, true);
                UpdateMetricsPollTimer(IsIconic(h) != FALSE, GetForegroundWindow() == h);
                if (previousConnection != g_apiConnection)
                {
                    SetText(g_status, StatusText());
                    UpdateGreeting();
                }
                break;
            }
            const auto previousRows = MonitorGpuRows(g_metrics).size();
            const bool previousExpanded = g_monitorGpuOpen >= 0;
            Json previousMetrics = std::move(g_metrics);
            {
                const auto previousConnection = g_apiConnection;
                g_apiConnection = NativeApiConnectionAfterMetricsPoll(g_apiConnection, g_metricPollFailures, 0, true);
                UpdateMetricsPollTimer(IsIconic(h) != FALSE, GetForegroundWindow() == h);
                g_metrics = std::move(d->value);
                UpdateSavingsSidebar();
                ReconcileMonitorGpuRows(previousRows, previousExpanded);
                if (g_page == ABOUT) UpdateAboutFacts();
                if (previousConnection != g_apiConnection)
                {
                    SetText(g_status, StatusText());
                    UpdateGreeting();
                }
            }
            if (g_page == MONITOR) UpdateMonitorView(&previousMetrics);
            if (MonitorAllRequestsIncomplete(g_metrics, g_allRequests))
                BeginJob(J_REFRESH);
            break;
        }
        case J_REPORT:
            if (d->value.is_string() && g_aboutReportRequest)
            {
                SetText(g_aboutReport, W(d->value.get<std::string>()));
                if (g_page == ABOUT)
                {
                    EnableWindow(GetDlgItem(h, 260), TRUE);
                    EnableWindow(GetDlgItem(h, 210), TRUE);
                    SetWindowTextW(GetDlgItem(h, 260), g_aboutLogVisible ? L"Hide report" : L"Show report");
                    if (g_aboutLogVisible) ShowWindow(g_aboutReport, SW_SHOW);
                }
                if (g_aboutCopyRequested && CopyUnicodeText(h, W(d->value.get<std::string>())))
                    SetText(g_status, L"Troubleshooting report copied. Review it before sharing.");
                else if (g_aboutCopyRequested)
                    SetText(g_status, L"Could not copy the troubleshooting report.");
                g_aboutCopyRequested = false;
                g_aboutReportRequest = false;
            }
            else if (d->value.is_string() && CopyUnicodeText(h, W(d->value.get<std::string>())))
                SetText(g_status, L"Diagnostics report copied to clipboard.");
            else
                SetText(g_status, L"Could not copy the diagnostics report to the clipboard.");
            break;
        case J_MODEL_SETTINGS_PREVIEW: {
            EnableModelSettingsInputs(true);
            EnableWindow(g_modelSettingsModeButton, TRUE);
            EnableWindow(g_modelSettingsPreviewButton, TRUE);
            EnableWindow(g_modelSettingsCancel, TRUE);
            const auto command = d->value.value("command", Json());
            SetText(g_modelSettingsPreview, command.is_null()
                ? L"Maya accepted the settings but did not return a command preview."
                : W(ModelSettingsCommandPreviewText(command, CurrentModelSavedCommand()) +
                    "\nValidated by Maya. Nothing has been saved yet."));
            g_modelSettingsPreviewValid = ControlText(g_modelSettingsEdit) == g_modelSettingsPreviewText;
            const auto previewDraft = Json::parse(g_modelSettingsPreviewText, nullptr, false);
            const bool changed = !previewDraft.is_discarded() && previewDraft.is_object() &&
                                 CurrentModelSettingsChanged(previewDraft);
            EnableWindow(g_modelSettingsSave, NativeModelSettingsApplyEnabled(
                changed, g_modelSettingsPreviewValid,
                ControlText(g_modelSettingsEdit) == g_modelSettingsPreviewText));
            if (g_busy != J_CHAT)
                SetText(g_status, !g_modelSettingsPreviewValid ? L"The settings changed during preview. Preview them again."
                    : changed ? L"Settings preview passed; Apply is ready."
                              : L"No settings changed; Apply is disabled.");
            break;
        }
        case J_MODEL_SETTINGS_SAVE: {
            const bool restarts = d->value.value("restarts", false);
            const bool changed = d->value.value("changed", false);
            g_modelSettingsOpen = false;
            g_modelSettingsEnvironmentOpen = false;
            g_modelSettingsEnvironmentRows.clear();
            g_modelSettingsPreviewValid = false;
            DestroyModelSettingsFields();
            SetPageControls();
            Layout();
            if (restarts)
            {
                g_modelSettingsRestartWatching = true;
                UpdateLeftoverDetails();
                g_modelSettingsRestartDown = false;
                g_modelSettingsRestartStartedAt = GetTickCount64();
                SetTimer(h, 9, 500, nullptr);
                if (g_busy == J_CHAT)
                {
                    g_modelSettingsRestartAfterChat = true;
                    SetText(g_status, L"Settings saved. Maya will reload them after the current answer finishes…");
                }
                else
                {
                    SetText(g_status, L"Settings saved. Maya is restarting with the new engine configuration…");
                    BeginJob(J_STATUS);
                }
            }
            else
            {
                SetText(g_status, changed ? L"Settings saved; they apply when Maya next loads this model."
                                          : L"Settings saved; no engine values changed.");
                if (g_busy == J_CHAT)
                    g_refreshModelsAfterChat = true;
                else
                    BeginJob(J_MODELS);
            }
            break;
        }
        case J_MODEL_LEFTOVER_DELETE:
            SetText(g_status, L"Unused path removed; freed " + ByteSize(d->value.value("freed", uint64_t(0))) + L".");
            BeginJob(J_MODELS);
            break;
        case J_SAVINGS:
        case J_SAVINGS_RESET:
        case J_SAVINGS_SAVE_PRICE:
        case J_SAVINGS_DEFAULT_PRICE:
            if (d->job == J_SAVINGS_DEFAULT_PRICE)
                g_priceDrafts.erase(g_priceRequest.value("model", std::string()));
            g_savingsData = d->value;
            SetText(g_savings, W(SavingsReportText(g_savingsData)));
            SendMessageW(g_priceModels, CB_RESETCONTENT, 0, 0);
            if (g_savingsData.contains("price_list") && g_savingsData["price_list"].is_array())
                for (const auto &p : g_savingsData["price_list"])
                {
                    auto name = SavingsString(p, "model");
                    if (name.empty()) name = "Unknown model";
                    if (p.value("running", false)) name += "  [RUNNING]";
                    SendMessageW(g_priceModels, CB_ADDSTRING, 0, (LPARAM)W(name).c_str());
                }
            {
                const auto list = g_savingsData.value("price_list", Json::array());
                int selected = 0;
                for (int i = 0; list.is_array() && i < (int)list.size(); ++i)
                    if (SavingsString(list[i], "model") == g_savingsModel) { selected = i; break; }
                if (list.is_array() && !list.empty()) SendMessageW(g_priceModels, CB_SETCURSEL, selected, 0);
            }
            LoadPriceFields();
            SetText(g_status, d->job == J_SAVINGS_RESET ? L"Savings counts reset; configured prices were retained."
                     : d->job == J_SAVINGS_SAVE_PRICE ? L"Model prices saved; all totals were recalculated."
                     : d->job == J_SAVINGS_DEFAULT_PRICE ? L"Built-in prices restored; totals were recalculated."
                     : L"Savings loaded.");
            break;
        case J_MODELS:
        {
            g_modelData = d->value;
            if (g_chatModelMenuOpen && g_modelData.contains("models") && g_modelData["models"].is_array())
            {
                g_chatModelMenuReopen = true;
                EndMenu();
            }
            g_modelLiveContext = g_modelData.value("live_context", Json::object());
            const auto deleted = g_modelData.value("deleted", Json(nullptr));
            const auto deletedKey = ModelDeleteOutcomeKey(deleted);
            if (!deletedKey.empty() && deletedKey != g_seenModelDeleteOutcome)
            {
                g_seenModelDeleteOutcome = deletedKey;
                auto message = W(ModelDeleteOutcomeText(deleted));
                if (ModelDeleteOutcomeIncludesFiles(deleted))
                    message += L" " + ByteSize(ModelDeleteOutcomeFreed(deleted)) + L" freed on disk.";
                if (!message.empty()) SetText(g_status, message);
                g_modelDeleteOutcomePending = false;
            }
            else if (g_modelDeleteOutcomePending)
            {
                g_modelDeleteOutcomePending = false;
                SetText(g_status, L"Maya returned, but did not report the deferred model deletion result. Review the Models list.");
            }
            const bool wasPullActive = g_pullActive;
            g_pullActive = PullStatusActive(g_modelData.value("pull", Json(nullptr)));
            std::vector<std::wstring> modelNames;
            const bool loaded = g_modelData.value("loaded", false);
            if (g_modelData.contains("models"))
                for (auto &m : g_modelData["models"])
                    modelNames.push_back(W(ModelListLabel(m, loaded)));
            const int previousModelCount = (int)SendMessageW(g_models, LB_GETCOUNT, 0, 0);
            SetListContentsIfChanged(g_models, modelNames);
            g_modelSel = (int)SendMessageW(g_models, LB_GETCURSEL, 0, 0);
            UpdateModelLoadButton();
            UpdateModelSwitchButton();
            UpdateModelDeleteButton();
            UpdateModelSettingsButton();
            if (g_leftoverOpen) PopulateLeftovers();
            if (!g_modelActionError.empty())
            {
                SetText(g_status, g_modelActionError);
                g_modelActionError.clear();
            }
            if (g_page == MODELS)
            {
                if (wasPullActive != g_pullActive) SetPageControls();
                if (wasPullActive != g_pullActive || previousModelCount != (int)modelNames.size())
                {
                    NativeLayoutTransition transition;
                    Layout();
                }
                if (ModelCatalogVisible(g_modelsAddOpen, g_pullActive))
                {
                    g_catalogFetchPending = false;
                    BeginJob(J_CATALOG);
                }
            }
            break;
        }
        case J_CATALOG: {
            g_catalogData = d->value;
            g_catalogUnavailable = false;
            if (CatalogStatusIsUnavailable(ControlText(g_status)))
                SetText(g_status, L"Model catalog loaded.");
            const auto pull = g_catalogData.value("pull", Json::object());
            std::vector<std::wstring> catalogNames;
            g_catalogRowToModel.clear();
            if (g_catalogData.contains("models") && g_catalogData["models"].is_array())
            {
                const auto &models = g_catalogData["models"];
                g_catalogRowToModel = CatalogListRowModelIndices(models);
                for (int modelIndex : g_catalogRowToModel)
                {
                    if (modelIndex < 0)
                    {
                        catalogNames.push_back(L"Other quants — files by other makers that the setup also downloads and runs: the same download, checks, and pack");
                        continue;
                    }
                    const auto &m = models[modelIndex];
                    auto name = m.value("name", m.value("id", std::string("Model")));
                    name += "  " + CatalogDetailsLabel(m);
                    if (CatalogInstalled(m))
                        name += "  [INSTALLED]";
                    if (CatalogDownloadFailed(m, pull))
                        name += "  [DOWNLOAD FAILED]";
                    catalogNames.push_back(W(name));
                }
            }
            if (catalogNames.empty())
                catalogNames.push_back(L"No supported model setups were returned");
            const bool wasPullActive = g_pullActive;
            g_pullActive = PullStatusActive(pull);
            SetListContentsIfChanged(g_catalog, catalogNames);
            SetCatalogHorizontalExtent(catalogNames);
            g_catalogSel = CatalogModelIndexForListRow(g_catalogRowToModel,
                                                       (int)SendMessageW(g_catalog, LB_GETCURSEL, 0, 0));
            SetPullProgress(pull);
            if (pull.is_object() && !pull.value("state", std::string()).empty())
                SetText(g_status, W(PullStatusText(pull)));
            if (g_pullActive)
                SetTimer(h, 4, 1500, nullptr);
            else
                KillTimer(h, 4);
            UpdateCatalogDownloadButton();
            g_catalogFetchPending = false;
            if (g_page == MODELS)
            {
                if (wasPullActive != g_pullActive) SetPageControls();
                NativeLayoutTransition transition;
                Layout();
            }
            if (PullStatusNeedsModelRefresh(wasPullActive, pull)) BeginJob(J_MODELS);
            break;
        }
        case J_PULL:
            g_pullRequestInFlight = false;
            g_pullActive = true;
            SetPullProgress(Json{{"state", "starting"}});
            SetText(g_status, L"Model download started…");
            SetTimer(h, 4, 1500, nullptr);
            if (g_page == MODELS) { SetPageControls(); Layout(); }
            BeginJob(J_CATALOG);
            break;
        case J_PULL_CANCEL:
            if (g_pullActive)
            {
                g_pullActive = false;
                if (g_page == MODELS) { SetPageControls(); Layout(); }
            }
            KillTimer(h, 4);
            SetText(g_status, d->value.value("cancelled", false) ? L"Download cancelled." : L"No download was active.");
            BeginJob(J_CATALOG);
            UpdateCatalogDownloadButton();
            break;
        case J_MODEL_DELETE_PLAN: {
            if (g_modelSheetPlanPreview)
            {
                g_modelSheetPlanPreview = false;
                g_modelSheetPlanFile = g_deleteFile;
                g_modelSheetPlan = d->value;
                g_modelSheetPlanError.clear();
                UpdateModelSheetView();
                break;
            }
            const auto plan = d->value;
            const bool running = plan.value("running", false);
            std::vector<std::string> targets;
            std::vector<std::pair<std::wstring, std::wstring>> choices;
            if (running)
            {
                targets = ModelDeleteTargets(g_modelData.value("models", Json::array()));
                for (const auto &target : targets)
                {
                    const auto it = std::find_if(g_modelData["models"].begin(), g_modelData["models"].end(),
                        [&](const Json &m) { return m.is_object() && m.contains("file") && m["file"].is_string() &&
                                                    m["file"].get<std::string>() == target; });
                    if (it == g_modelData["models"].end()) continue;
                    const auto name = it->value("model", std::string("Model")) + " " + it->value("quant", std::string());
                    choices.emplace_back(W(name + "  [" + target + "]"), W(target));
                }
                if (choices.empty())
                {
                    SetText(g_status, targets.empty()
                        ? L"Cannot delete the running model: no other loadable model is installed."
                        : L"Cannot delete the running model: no replacement model is available.");
                    break;
                }
            }
            const bool canDeleteFiles = plan.value("ok", false) && plan.contains("items") &&
                                        plan["items"].is_array() && !plan["items"].empty();
            std::wstring prompt = L"Maya will remove this model's config: " + W(g_deleteFile);
            const auto configuredModels = g_modelData.value("models", Json::array());
            const auto deletedModel = std::find_if(configuredModels.begin(), configuredModels.end(), [&](const Json &m) {
                    return m.is_object() && m.contains("file") && m["file"].is_string() &&
                           m["file"].get<std::string>() == g_deleteFile;
                });
            if (deletedModel != configuredModels.end())
            {
                const auto script = deletedModel->value("script", std::string());
                if (!script.empty()) prompt += L" and its start script: " + W(script);
            }
            prompt += L". Your chats stay.";
            if (!plan.value("ok", false))
                prompt += L"\n\nDownloaded files will stay: " + W(plan.value("why", std::string("Maya could not verify them.")));
            else if (!canDeleteFiles)
                prompt += L"\n\nNo downloaded model files were found to remove.";
            else
            {
                prompt += L"\n\nDownloaded files Maya can remove (" + ByteSize(plan.value("bytes", uint64_t(0))) + L"):";
                for (const auto &item : plan["items"])
                    prompt += L"\n\n• " + W(ModelDeleteItemLabel(item)) + L"  ·  " +
                              ByteSize(item.value("bytes", uint64_t(0))) + L"\n  " +
                              W(item.value("path", std::string()));
            }
            if (plan.contains("kept") && plan["kept"].is_array())
                for (const auto &kept : plan["kept"])
                    if (kept.is_string()) prompt += L"\n\nKept: " + W(kept.get<std::string>());
            if (canDeleteFiles && plan.contains("drafter_users") && plan["drafter_users"].is_array() &&
                !plan["drafter_users"].empty())
            {
                prompt += L"\n\nThese models share the DFlash2 drafter: ";
                for (size_t i = 0; i < plan["drafter_users"].size(); ++i)
                {
                    const auto &file = plan["drafter_users"][i];
                    if (!file.is_string()) continue;
                    const auto found = std::find_if(g_modelData["models"].begin(), g_modelData["models"].end(),
                        [&](const Json &m) { return m.is_object() && m.contains("file") && m["file"].is_string() &&
                                                    m["file"].get<std::string>() == file.get<std::string>(); });
                    if (i) prompt += L", ";
                    prompt += found == g_modelData["models"].end()
                        ? W(file.get<std::string>())
                        : W(found->value("model", std::string("Model")) + " " + found->value("quant", std::string()));
                }
                prompt += L". They will use MTP until setup downloads the drafter again.";
            }
            if (running)
                prompt += L"\n\nIt is running. Maya switches to the selected model first.";
            const auto selection = ReviewModelDelete(h, L"Review model deletion",
                L"Review what Maya will remove. This action cannot be undone when files are selected.", prompt,
                L"Also delete the downloaded files (" + ByteSize(plan.value("bytes", uint64_t(0))) + L")",
                std::move(choices), std::move(targets), running, canDeleteFiles);
            if (!selection.accepted)
            {
                SetText(g_status, L"Model deletion cancelled; nothing was changed.");
                break;
            }
            g_deleteThen = selection.then;
            g_deleteFiles = selection.deleteFiles;
            BeginJob(J_MODEL_DELETE);
            break;
        }
        case J_MODEL_DELETE:
            if (!d->value.value("deleting", std::string()).empty())
            {
                g_modelDeleteRestartWatching = true;
                g_modelDeleteOutcomePending = true;
                g_switchRestartWatching = true;
                UpdateLeftoverDetails();
                g_switchRestartDown = false;
                g_switchStartedAt = GetTickCount64();
                g_switchTarget = d->value.value("to", std::string("selected model"));
                SetLifecycleButtons(g_online, true);
                SetText(g_status, W("Maya is switching to " + g_switchTarget + "; waiting to verify the deletion…"));
            }
            else
            {
                auto message = W(ModelDeleteOutcomeText(d->value));
                if (ModelDeleteOutcomeIncludesFiles(d->value))
                    message += L" " + ByteSize(ModelDeleteOutcomeFreed(d->value)) + L" freed on disk.";
                SetText(g_status, message.empty() ? L"Model deletion finished." : message);
                BeginJob(J_MODELS);
            }
            break;
        case J_PULL_STATUS:
            if (d->value.contains("pull") && d->value["pull"].is_object())
            {
                auto p = d->value["pull"];
                g_catalogData["pull"] = p;
                SetPullProgress(p);
                SetText(g_status, W(PullStatusText(p)));
                if (!PullStatusActive(p))
                {
                    KillTimer(h, 4);
                    BeginJob(J_MODELS);
                }
            }
            else
            {
                g_catalogData["pull"] = nullptr;
                const bool wasPullActive = g_pullActive;
                g_pullActive = false;
                KillTimer(h, 4);
                SetPullProgress(Json(nullptr));
                SetText(g_status, L"No model download is active.");
                UpdateCatalogDownloadButton();
                if (wasPullActive && g_page == MODELS) { SetPageControls(); Layout(); }
            }
            break;
        case J_TOKENS:
            if (d->value.value("sequence", uint64_t(0)) == g_contextSeq.load() &&
                !(g_busy == J_CHAT && g_chat >= 0 && g_chats[g_chat].id == g_jobChatId))
                ShowContextCount(d->value.value("tokens", uint64_t(0)),
                                 d->value.value("max_context", (uint64_t)g_health.value("max_context", 0)),
                                 d->value.value("approximate", false));
            break;
        case J_CONTEXT: {
            g_contextData = d->value;
            const auto shared = d->value.value("shared", Json::object());
            g_sharedDefaultsOn = shared.value("shared", false);
            SetText(g_sharedDefaultsSummary, W(ChatSharedDefaultsSummary(shared)));
            SendMessageW(g_sharedDefaultsCheck, BM_SETCHECK,
                         g_sharedDefaultsOn ? BST_CHECKED : BST_UNCHECKED, 0);
            const auto context = d->value.value("context", Json::object());
            const auto port = d->value.value("port", Json::object());
            const auto budget = d->value.value("budget", Json::object());
            SendMessageW(g_contextEdit, CB_RESETCONTENT, 0, 0);
            const int currentContext = context.value("context", 0);
            const auto contextStops = NativeContextStops(context.value("min", 0), context.value("max", 0), {currentContext});
            for (const int stop : contextStops)
            {
                const auto label = std::to_wstring(stop) + L" tokens";
                const LRESULT index = SendMessageW(g_contextEdit, CB_ADDSTRING, 0, (LPARAM)label.c_str());
                if (index != CB_ERR && index != CB_ERRSPACE)
                {
                    SendMessageW(g_contextEdit, CB_SETITEMDATA, index, stop);
                    if (stop == currentContext) SendMessageW(g_contextEdit, CB_SETCURSEL, index, 0);
                }
            }
            EnableWindow(g_contextEdit, !contextStops.empty());
            SetText(g_portEdit, std::to_wstring(port.value("port", 0)).c_str());
            SetText(g_budgetEdit, std::to_wstring(budget.value("budget", 0)).c_str());
            g_budgetSupported = NativeBudgetSupported(budget);
            EnableWindow(g_budgetEdit, g_budgetSupported);
            if (g_page == SETTINGS)
            {
                ShowWindow(g_budgetLabel, g_budgetSupported ? SW_SHOW : SW_HIDE);
                ShowWindow(g_budgetEdit, g_budgetSupported ? SW_SHOW : SW_HIDE);
            }
            UpdateBudgetSave();
            const auto mcp = d->value.value("mcp", Json::object());
            g_mcpData = mcp;
            g_mcpLoadAttempted = true;
            const size_t toolCount = mcp.contains("tools") && mcp["tools"].is_array() ? mcp["tools"].size() : 0;
            SetText(g_mcpDetails, W(McpStatusReport(mcp)));
            if (g_page == CHAT_DEFAULTS)
                SetText(g_settingsStatus, L"Context range: " + std::to_wstring(context.value("min", 0)) + L"–" +
                                              std::to_wstring(context.value("max", 0)) + L" tokens. Context or port changes reload Maya.");
            else
                SetText(g_settingsStatus, L"Context range: " + std::to_wstring(context.value("min", 0)) + L"–" +
                                              std::to_wstring(context.value("max", 0)) + L" tokens. MCP tools available: " +
                                              std::to_wstring(toolCount) +
                                              (budget.value("supported", false) ? L". Thinking budget supported."
                                                                                : L". Thinking budget unsupported."));
            break;
        }
        case J_SHARED_SAVE:
            EnableWindow(g_sharedDefaultsCheck, TRUE);
            g_sharedDefaultsOn = d->value.value("shared", false);
            SetText(g_sharedDefaultsSummary, W(ChatSharedDefaultsSummary(d->value)));
            SendMessageW(g_sharedDefaultsCheck, BM_SETCHECK,
                         g_sharedDefaultsOn ? BST_CHECKED : BST_UNCHECKED, 0);
            SetText(g_samplingStatus, g_sharedDefaultsOn
                ? L"Other API clients will use these defaults when their request leaves a value unset."
                : L"Other API clients now use their own defaults.");
            break;
        case J_TURBO_INFO: {
            g_turboData = d->value;
            const bool hadSnapshot = g_turboSnapshot.is_object();
            const bool restored = !hadSnapshot || g_turboData.value("on", false) || RestoreNativeTurboPrefs();
            UpdateTurboButton();
            if (g_turboData.value("on", false))
                SetText(g_samplingStatus, L"Turbo is on. It trades answer quality for speed; turn it off to restore this app's saved settings.");
            else if (!restored)
                SetText(g_samplingStatus, L"Turbo is off, but the restored local settings could not be saved to disk.");
            else if (!g_turboSnapshot.is_object())
                SetText(g_samplingStatus, L"Turbo is off. Enabling it can reduce answer quality and accuracy.");
            break;
        }
        case J_TURBO_SET: {
            g_turboData = d->value;
            const bool prefsRestored = g_turboRequestOn || RestoreNativeTurboPrefs();
            UpdateTurboButton();
            if (d->value.value("restarting_now", false))
            {
                g_turboRestartWatching = true;
                g_turboRestartDown = false;
                g_turboRestartStartedAt = GetTickCount64();
                SetText(g_samplingStatus, L"Maya is restarting to apply Turbo…");
            }
            else SetText(g_samplingStatus, !prefsRestored
                ? L"Turbo is off, but the restored local settings could not be saved to disk."
                : g_turboRequestOn ? L"Turbo enabled. Answers may be less accurate."
                                  : L"Turbo disabled; your saved settings are restored.");
            break;
        }
        case J_SETTINGS:
            g_settingsData = d->value;
            break;
        case J_CHAT: {
            FlushStream();
            EnableBusy(false);
            KillTimer(h, 3);
            g_answer.error = d->error;
            if (!d->error.empty()) SetText(g_status, L"Request failed: " + W(d->error));
            FinalizeMcpEvents(g_answer.tools);
            g_answer.stopped = NativeChatStopped(g_cancelled.load(), d->value.value("stopped", false));
            if (g_answer.jobId.empty())
            {
                std::lock_guard<std::mutex> lock(g_jobIdMutex);
                g_answer.jobId = g_jobId;
            }
            g_answer.meta = AnswerMetadata(g_answer.usage, g_answer.timings,
                                           std::chrono::duration<double>(Clock::now() - g_actionAt).count(),
                                           g_answer.stopped, g_answer.tools, g_answer.toolLimit);
            if (NativeChatAnswerShouldPersist(!g_answer.text.empty(), !g_answer.reasoning.empty(), !g_answer.tools.empty(),
                                               g_answer.toolLimit, !g_answer.jobId.empty(), !g_answer.error.empty(),
                                               g_answer.stopped))
            {
                const auto chat = std::find_if(g_chats.begin(), g_chats.end(),
                                               [](const Chat &c) { return c.id == g_jobChatId; });
                if (chat != g_chats.end())
                {
                    chat->messages.push_back(g_answer);
                    if (!AppendLog(*chat, g_answer))
                    {
                        if (!chat->pendingJob.empty() && chat->pendingJob == g_answer.jobId)
                        {
                            chat->messages.pop_back();
                            g_pendingChatRetryAt = GetTickCount64() + 30000;
                            SetText(g_status, L"Could not save the answer; Maya still holds it and the Manager will retry.");
                        }
                        else SetText(g_status, L"Answer finished, but its transcript could not be saved.");
                    }
                    else if (!chat->pendingJob.empty() && chat->pendingJob == g_answer.jobId)
                    {
                        chat->pendingJob.clear();
                        g_pendingChatRetryAt = 0;
                        SaveChatMetadata(*chat);
                    }
                }
            }
            g_resumingPendingJob = false;
            RenderChat();
            ScheduleContextMeter();
            ContinueAfterChat();
            TrySendStartupQuestion();
            break;
        }
        case J_SWITCH: {
            g_modelSwitchRequestInFlight = false;
            const bool profileSelected = SelectManagerProfileForModelSwitch(d->value);
            if (ModelSwitchRestarts(d->value))
            {
                g_switchRestartWatching = true;
                g_switchRestartDown = false;
                g_switchStartedAt = GetTickCount64();
                g_switchTarget = d->value.value("model", d->value.value("to", std::string("selected model")));
                SetLifecycleButtons(g_online);
                SetText(g_status, profileSelected
                    ? W("Switching to " + g_switchTarget + "; waiting for Maya to return…")
                    : L"Maya is switching models, but Win-Manager could not save the matching profile selection.");
            }
            else
            {
                SetLifecycleButtons(g_online);
                if (!profileSelected)
                {
                    SetText(g_status, L"Maya saved the model choice, but Win-Manager could not save its profile. Maya was left running.");
                    BeginJob(J_MODELS, nullptr);
                }
                else if (g_busy == J_CHAT)
                {
                    g_modelSwitchRestartAfterChat = true;
                    SetText(g_status, L"Model selection saved. Maya will restart when the current answer finishes.");
                }
                else
                {
                    SetText(g_status, L"Model selection saved. Win-Manager is restarting Maya to load it…");
                    BeginJob(J_RESTART, L"restart");
                }
            }
            break;
        }
        case J_LOAD:
        case J_UNLOAD:
            SetText(g_status, d->job == J_LOAD ? L"Model load request finished." : L"Model unload request finished.");
            BeginJob(J_MODELS);
            break;
        case J_SAVE_SETTINGS:
            EnableWindow(g_settingsSave, TRUE);
            SetText(g_settingsStatus, d->value.value("port_restarts", false)
                                          ? L"Settings saved. Maya is restarting on the new port."
                                          : L"Settings applied successfully.");
            BeginJob(J_CONTEXT);
            break;
        case J_BUDGET_SAVE:
            if (!d->value.is_object() || !d->value.contains("budget") || !d->value["budget"].is_number_integer())
            {
                SetText(g_settingsStatus, L"Maya returned an invalid thinking budget; refreshing settings.");
                BeginJob(J_CONTEXT);
                break;
            }
            g_contextData["budget"] = d->value;
            SetText(g_budgetEdit, std::to_wstring(d->value["budget"].get<int>()).c_str());
            SetText(g_settingsStatus, d->value["budget"].get<int>() == 0
                ? L"Thinking budget saved. Thinking has no limit, from the next answer."
                : L"Thinking budget saved. It applies from the next answer.");
            UpdateBudgetSave();
            break;
        case J_UPDATE_CHECK: {
            if (g_updateRecovering)
            {
                g_updateRecovering = false;
                const auto target = MayaUpdateRecoveryTarget(d->value);
                const auto failure = MayaUpdateRecoveryError(d->value);
                if (!target.empty())
                {
                    SetUpdateAvailable(false);
                    g_updateTarget = target;
                    g_updateWatching = true;
                    g_updateDown = false;
                    SetLifecycleButtons(true);
                    SetText(g_status, L"Resumed update tracking for Maya v" + W(target) + L"…");
                }
                else
                    SetText(g_status, failure.empty()
                        ? L"Maya returned on the previous version, but its updater did not report a reason; inspect the engine log."
                        : L"Maya's previous update failed: " + W(failure));
                break;
            }
            if (d->value.is_object() && d->value.value("mode", std::string()) == "fresh")
            {
                g_updateStartAfterCheck = false;
                g_updateAutoStartAfterCheck = false;
                g_updateCheckResult = d->value;
                g_updateChecked = true;
                g_updateFreshInstall = true;
                g_updateCanInstall = MayaUpdateCanStart(d->value);
                g_updateInstallVisible = true;
                g_updateSummaryText = MayaUpdateCardText(d->value);
                const auto notesUrl = MayaUpdateNotesUrl(d->value);
                if (!notesUrl.empty()) g_updateNotesUrl = W(notesUrl);
                SetUpdateAvailable(false);
                SetText(g_updateSummary, W(g_updateSummaryText));
                SetWindowTextW(g_updateInstall, g_updateCanInstall ? L"Install Maya" : L"Setup details");
                EnableWindow(g_updateInstall, TRUE);
                if (g_page == ABOUT) Layout();
                if (!d->backgroundPoll)
                    SetText(g_status, g_updateCanInstall ? L"Maya setup checks passed; review About and updates to install."
                                                        : L"Maya setup is blocked; review About and updates for details.");
                break;
            }
            auto u = d->value.value("update", Json::object());
            g_updateCheckResult = d->value;
            g_updateChecked = true;
            g_updateFreshInstall = false;
            g_updateCanInstall = MayaUpdateCanStart(d->value);
            g_updateInstallVisible = u.value("newer", false);
            SetUpdateAvailable(MayaUpdateBadgeVisible(u.value("newer", false), g_updateWatching));
            g_updateSummaryText = MayaUpdateCardText(d->value);
            const auto notesUrl = MayaUpdateNotesUrl(d->value);
            if (!notesUrl.empty()) g_updateNotesUrl = W(notesUrl);
            const auto latest = u.value("latest", Json::object());
            const auto latestVersion = latest.value("version", std::string());
            std::string releaseBody;
            for (const char *key : {"changelog", "body", "notes", "summary"})
                if (latest.contains(key) && latest[key].is_string() && !latest[key].get<std::string>().empty())
                {
                    releaseBody = latest[key].get<std::string>();
                    break;
                }
            if (latestVersion == "1.1.73")
            {
                g_latestReleaseNotesText =
                    "Latest release · v1.1.73\r\n"
                    "• Inkling 2 Mini now supports 8 GB NVIDIA cards, V100 and RTX 20 GPUs, plus AMD on Linux.\r\n"
                    "• Optional drafting measures whether the official drafter is faster on this PC; accepted answers stay identical.\r\n"
                    "• The looped-layer gate step is faster.\r\n"
                    "• DeepSeek now preserves the configured VRAM reserve for the whole run.\r\n"
                    "• Hy4-Flash uses safe host-memory transfers when two-GPU direct copies are unsafe.";
            }
            else if (!latestVersion.empty() && !releaseBody.empty())
            {
                if (releaseBody.size() > 2800) releaseBody.resize(2800);
                g_latestReleaseNotesText = "Latest release · v" + latestVersion + "\r\n" + releaseBody +
                    "\r\n\r\nUse Full release notes for the complete official changelog.";
            }
            SetText(g_updateSummary, W(g_updateSummaryText));
            SetText(g_releaseNotes, W(g_latestReleaseNotesText));
            SetWindowTextW(g_updateInstall, g_updateCanInstall
                ? W("Install v" + latest.value("version", std::string())).c_str()
                : L"Preflight details");
            EnableWindow(g_updateInstall, g_updateInstallVisible);
            if (g_page == ABOUT)
            {
                ShowWindow(g_updateSummary, SW_SHOW);
                ShowWindow(g_updateNotes, g_updateNotesUrl.empty() ? SW_HIDE : SW_SHOW);
                ShowWindow(g_updateInstall, g_updateInstallVisible ? SW_SHOW : SW_HIDE);
                Layout();
            }
            if (!d->backgroundPoll) SetText(g_status, W(MayaUpdateStatusText(d->value)));
            if (g_updateStartAfterCheck)
            {
                g_updateStartAfterCheck = false;
                const bool autoStart = g_updateAutoStartAfterCheck;
                g_updateAutoStartAfterCheck = false;
                if (g_updateAvailable && g_updateInstallVisible)
                    StartMayaUpdateFromPreflight(h, false, autoStart);
                else
                    SetText(g_status, L"Maya is already up to date.");
            }
            break;
        }
        case J_UPDATE_START:
            SetText(g_status, JsonText(d->value, "status").empty() ? L"Update started." : JsonText(d->value, "status"));
            g_updateCanInstall = false;
            g_updateInstallVisible = false;
            g_updateSummaryText = JsonText(d->value, "status").empty() ? "Update started." : U(JsonText(d->value, "status"));
            SetText(g_updateSummary, W(g_updateSummaryText));
            if (g_page == ABOUT) Layout();
            if (d->value.value("managed_complete", false))
            {
                g_updateWatching = false;
                g_updateDown = false;
                g_online = true;
                SetLifecycleButtons(true);
                SetText(g_status, JsonText(d->value, "status"));
                BeginJob(J_STATUS);
            }
            else if (d->value.value("online", false) && !g_updateTarget.empty())
            {
                g_updateWatching = true;
                g_updateDown = false;
                SetUpdateAvailable(false);
                SetLifecycleButtons(g_online);
                SetText(g_status, L"Maya is updating to v" + W(g_updateTarget) + L"…");
            }
            break;
        default:
            break;
        }
        if (g_catalogFetchPending && g_busy == J_NONE)
        {
            g_catalogFetchPending = false;
            if (g_page == MODELS && g_modelsAddOpen) BeginJob(J_CATALOG);
        }
        if (d->job != J_STATUS && d->job != J_REFRESH &&
            NativePollInvalidatesWindow(d->backgroundPoll, d->job == J_MODELS || d->job == J_CATALOG))
            InvalidateRect(h, nullptr, FALSE);
        UpdateSupportLinkVisibility();
        return 0;
    }
    if (m == WM_CLOSE)
    {
        FinishPendingChatDelete();
        if (g_busy == J_CHAT)
        {
            std::string job;
            {
                std::lock_guard<std::mutex> lock(g_jobIdMutex);
                job = g_jobId;
            }
            auto chat = std::find_if(g_chats.begin(), g_chats.end(),
                                     [](const Chat &c) { return c.id == g_jobChatId; });
            if (chat != g_chats.end() && NativeChatShouldPersistJobOnClose(g_cancelled, job))
            {
                chat->pendingJob = std::move(job);
                SaveChatMetadata(*chat);
            }
        }
        if (g_streamHandle.load())
            Api().Cancel(g_streamHandle);
        DestroyWindow(h);
        return 0;
    }
    if (m == WM_DESTROY)
    {
        KillTimer(h, 1);
        KillTimer(h, 2);
        KillTimer(h, 3);
        KillTimer(h, 4);
        KillTimer(h, 7);
        KillTimer(h, 8);
        KillTimer(h, 9);
        KillTimer(h, 11);
        KillTimer(h, NativeRestartTimerId);
        if (g_trayIconAdded)
        {
            NOTIFYICONDATAW icon{};
            icon.cbSize = sizeof(icon);
            icon.hWnd = h;
            icon.uID = 1;
            Shell_NotifyIconW(NIM_DELETE, &icon);
            g_trayIconAdded = false;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int show)
{
    // Keep the legacy 96-DPI layout while letting Windows scale its GDI output more cleanly.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED);
    g_inst = inst;
    g_taskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
    int argc = 0;
    if (auto argv = CommandLineToArgvW(GetCommandLineW(), &argc))
    {
        std::vector<std::wstring> args;
        for (int i = 1; i < argc; ++i)
        {
            args.emplace_back(argv[i]);
            if (args.back() == L"--windows-startup") g_windowsStartupLaunch = true;
        }
        g_startupQuestion = NativeStartupQuestion(args);
        LocalFree(argv);
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    wchar_t exe[32768]{};
    GetModuleFileNameW(nullptr, exe, 32768);
    auto dir = fs::path(exe).parent_path();
    g_root = FindMayaRoot(dir);
    if (g_root.empty()) {
        MessageBoxW(nullptr, L"Could not find the Maya-XS folder. Keep WinManager under the folder containing maya.py and serve\\web.",
                    L"Maya Win-Manager", MB_OK | MB_ICONERROR);
        return 1;
    }
    g_worker = dir / L"MayaManagerWorker.exe";
    g_mayaIconLarge = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR);
    g_mayaIconSmall = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    g_deepseekIcon = (HICON)LoadImageW(inst, MAKEINTRESOURCEW(102), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    wchar_t local[32768]{};
    DWORD localLen = GetEnvironmentVariableW(L"LOCALAPPDATA", local, 32768);
    g_userData = localLen && localLen < 32768 ? local : L".";
    g_chatDir = g_userData + L"\\MayaXS\\WinManager\\chats";
    fs::create_directories(g_chatDir);
    INITCOMMONCONTROLSEX cc{sizeof(cc), ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
    InitCommonControlsEx(&cc);
    LoadLibraryW(L"Msftedit.dll");
    g_bgBrush = CreateSolidBrush(C_BG);
    g_sideBrush = CreateSolidBrush(C_SIDE);
    g_cardBrush = CreateSolidBrush(C_CARD);
    g_fieldBrush = CreateSolidBrush(C_CARD2);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = Proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"MayaNativeManager";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBrush;
    wc.hIcon = g_mayaIconLarge;
    wc.hIconSm = g_mayaIconSmall;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        if (g_mayaIconLarge) DestroyIcon(g_mayaIconLarge);
        if (g_mayaIconSmall) DestroyIcon(g_mayaIconSmall);
        if (g_deepseekIcon) DestroyIcon(g_deepseekIcon);
        return 1;
    }
    g_sidebarIconFont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_DONTCARE, L"Segoe MDL2 Assets");
    auto h = CreateWindowExW(0, wc.lpszClassName, L"Maya | Local AI", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             1440, 960, nullptr, nullptr, inst, nullptr);
    if (!h)
    {
        if (g_mayaIconLarge) DestroyIcon(g_mayaIconLarge);
        if (g_mayaIconSmall) DestroyIcon(g_mayaIconSmall);
        if (g_deepseekIcon) DestroyIcon(g_deepseekIcon);
        if (g_sidebarIconFont) DeleteObject(g_sidebarIconFont);
        return 1;
    }
    SetWindowTheme(h, L"DarkMode_Explorer", nullptr);
    ShowWindow(h, SW_SHOWMAXIMIZED);
    UpdateWindow(h);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (HandleShortcut(msg)) continue;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_TAB)
        {
            HWND focused = GetFocus();
            if (!focused || focused == g_hwnd || GetAncestor(focused, GA_ROOT) == g_hwnd)
            {
                while (focused && focused != g_hwnd && GetParent(focused) && GetParent(focused) != g_hwnd)
                    focused = GetParent(focused);
                const bool backwards = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                const HWND next = MayaNextTabStop(g_hwnd, focused == g_hwnd ? nullptr : focused, backwards);
                if (next && next != focused) SetFocus(next);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_navTooltip && IsWindow(g_navTooltip)) DestroyWindow(g_navTooltip);
    if (g_gdiplusToken) GdiplusShutdown(g_gdiplusToken);
    if (g_mayaIconLarge) DestroyIcon(g_mayaIconLarge);
    if (g_mayaIconSmall) DestroyIcon(g_mayaIconSmall);
    if (g_deepseekIcon) DestroyIcon(g_deepseekIcon);
    if (g_sidebarIconFont) DeleteObject(g_sidebarIconFont);
    CoUninitialize();
    return 0;
}

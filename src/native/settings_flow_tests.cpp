#include "settings_flow.h"
#include "settings_layout.h"
#include "windows_startup_policy.h"
#include "list_refresh.h"
#include "status_card.h"
#include "support_link.h"
#include "../config.h"
#include "chat_metadata.h"
#include "chat_reasoning_control.h"
#include "chat_menu_policy.h"
#include "chat_delete_policy.h"
#include "manager_title.h"
#include "chat_search_preview.h"
#include "chat_message_ranges.h"
#include "chat_preferences.h"
#include "chat_sampling.h"
#include "turbo_native.h"
#include "chat_export.h"
#include "chat_history.h"
#include "chat_day_group.h"
#include "composer_layout.h"
#include "chat_ideas.h"
#include "chat_context.h"
#include "chat_answer.h"
#include "chat_tools.h"
#include "connect_info.h"
#include "local_data.h"
#include "monitor_report.h"
#include "pull_status.h"
#include "model_delete.h"
#include "savings_report.h"
#include "savings_price.h"
#include "model_settings.h"
#include "model_settings_tabs.h"
#include "model_leftover.h"
#include "model_details.h"
#include "model_catalog.h"
#include "model_switch_result.h"
#include "model_switch_watch.h"
#include "thought_display.h"
#include "about_report.h"
#include "keyboard_shortcuts.h"
#include "startup_question.h"
#include "chat_scroll.h"
#include "chat_markdown.h"
#include "chat_code_actions.h"
#include "code_viewer_policy.h"
#include "chat_attachments.h"
#include "video_player_policy.h"
#include "status_poll.h"
#include "context_stops.h"
#include "context_warning.h"
#include "chat_stream_policy.h"
#include "theme_policy.h"
#include "update_preflight.h"
#include "update_preserve.h"
#include "update_watch.h"
#include "update_endpoint.h"
#include "github_repo.h"
#include <algorithm>
#include <cassert>
#include <ctime>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct FixtureApi
{
    Json ctx{{"context", 32768}, {"min", 1024}, {"max", 272000}, {"reload", nullptr}};
    Json port{{"port", 8082}, {"saved", true}, {"restarts", true}};
    Json budget{{"budget", 1024}, {"supported", true}};
    Json sharedSettings{{"shared", false}, {"defaults", Json::object()}};
    std::vector<std::string> calls;
    std::vector<std::wstring> postPaths;
    std::vector<Json> postBodies;
    bool failReload = false;

    Json Get(const wchar_t *path)
    {
        const std::wstring p(path);
        if (p == L"/api/context")
        {
            calls.push_back("GET context");
            return ctx;
        }
        if (p == L"/api/port")
        {
            calls.push_back("GET port");
            return port;
        }
        if (p == L"/api/thinking-budget")
        {
            calls.push_back("GET budget");
            return budget;
        }
        if (p == L"/settings")
        {
            calls.push_back("GET shared");
            return sharedSettings;
        }
        throw std::runtime_error("Unexpected fixture endpoint");
    }

    Json Post(const wchar_t *path, const Json &body)
    {
        const std::wstring p(path);
        postPaths.push_back(p);
        postBodies.push_back(body);
        if (p == L"/api/context")
        {
            calls.push_back("POST context");
            ctx["context"] = body["max_context"];
            ctx["reload"] = {{"state", failReload ? "failed" : "done"},
                             {"error", failReload ? "fixture reload failure" : ""}};
            return ctx["reload"];
        }
        if (p == L"/api/thinking-budget")
        {
            calls.push_back("POST budget");
            budget["budget"] = body["budget"];
            return budget;
        }
        if (p == L"/api/port")
        {
            calls.push_back("POST port");
            port["port"] = body["port"];
            return port;
        }
        if (p == L"/settings")
        {
            calls.push_back("POST shared");
            const auto defaults = body.value("defaults", Json::object());
            sharedSettings = {{"shared", defaults.is_object() && !defaults.empty()},
                              {"defaults", defaults.is_object() ? defaults : Json::object()}};
            return sharedSettings;
        }
        throw std::runtime_error("Unexpected fixture endpoint");
    }
};

template <class Fn> void MustThrow(Fn fn, const char *part)
{
    try
    {
        fn();
        assert(false && "expected exception");
    }
    catch (const std::exception &e)
    {
        assert(std::string(e.what()).find(part) != std::string::npos);
    }
}

int main()
{
    const auto settingsRows = NativeSettingsTopRowsAt(200);
    assert(settingsRows.headingY + settingsRows.headingHeight <= settingsRows.themeBoxY);
    assert(settingsRows.themeBoxY + 34 <= settingsRows.schemeBoxY);
    assert(settingsRows.schemeBoxY + 34 <= settingsRows.contextY);
    assert(settingsRows.themeLabelY == settingsRows.themeBoxY + 4);
    assert(settingsRows.schemeLabelY == settingsRows.schemeBoxY + 4);

    const std::array<std::array<bool, 2>, 4> supportStates{{{false, false}, {true, false}, {false, true}, {true, true}}};
    for (const auto &state : supportStates)
    {
        const auto placement = NativeSupportLinkPlacementFor(state[0], state[1]);
        assert(placement.about && !placement.update);
    }
    assert(std::wstring(NativeSupportUrl) == L"https://github.com/sponsors/mw00");
    const std::array<bool, 4> settingTabs{true, false, true, true};
    assert(ModelSettingsGroupForKey("context") == NativeModelSettingsGroup::MODEL);
    assert(ModelSettingsGroupForKey("gpus") == NativeModelSettingsGroup::GPUS_MEMORY);
    assert(ModelSettingsGroupForKey("ram_gb") == NativeModelSettingsGroup::GPUS_MEMORY &&
           ModelSettingsNumericKey("ram_gb") && ModelSettingsWholeKey("ram_gb"));
    assert(ModelSettingsGroupForKey("kv_streaming") == NativeModelSettingsGroup::PROMPTS_CACHE);
    assert(ModelSettingsGroupForKey("cpu_threads") == NativeModelSettingsGroup::SPEED &&
           ModelSettingsGroupForKey("fast_prefill") == NativeModelSettingsGroup::SPEED);
    assert(NextModelSettingsTab(0, 1, settingTabs) == 2 && NextModelSettingsTab(2, 1, settingTabs) == 3 &&
           NextModelSettingsTab(3, 1, settingTabs) == 0 && NextModelSettingsTab(0, -1, settingTabs) == 3);
    assert(NextModelSettingsTab(0, 0, settingTabs) == -1 &&
           NextModelSettingsTab(0, 1, std::array<bool, 4>{}) == -1);
    const Json savedTabs{{"context", 32768}, {"gpus", Json::array({0})}, {"prefill", 4096}, {"cpu_threads", 8}};
    assert(!ModelSettingsGroupChanged(savedTabs, savedTabs, NativeModelSettingsGroup::MODEL));
    assert(!ModelSettingsFieldChanged(savedTabs, savedTabs, "context"));
    assert(ModelSettingsFieldChanged(savedTabs, Json{{"context", 65536}}, "context"));
    assert(ModelSettingsFieldChanged(savedTabs, Json{{"gpus", Json::array({0})}}, "context"));
    assert(ModelSettingsFieldChanged(savedTabs, Json{{"context", 32768}, {"gpus", Json::array({0})},
                                                      {"prefill", 4096}, {"cpu_threads", 8}, {"new", true}}, "new"));
    std::array<std::vector<std::wstring>, 4> modelWarnings{};
    AppendModelSettingsWarning(modelWarnings, (int)NativeModelSettingsGroup::MODEL,
                               L"Context size (tokens)", L"Choose a supported value.");
    assert(modelWarnings[0].size() == 1 &&
           modelWarnings[0][0] == L"Context size (tokens): Choose a supported value.");
    Json invalidModelSetting = savedTabs;
    assert(!ModelSettingsSetScalar(invalidModelSetting, "context", "not-a-number"));
    AppendModelSettingsWarning(modelWarnings, (int)NativeModelSettingsGroup::MODEL,
                               L"Context size (tokens)", ModelSettingsInvalidWarning("context"));
    assert(modelWarnings[0].back() == L"Context size (tokens): Enter a valid value.");
    assert(ModelSettingsTabMarker(false, false) == NativeModelSettingsTabMarker::NONE &&
           ModelSettingsTabMarker(true, false) == NativeModelSettingsTabMarker::CHANGED &&
           ModelSettingsTabMarker(true, true) == NativeModelSettingsTabMarker::WARNING);
    assert(!ModelSettingsGroupChanged(Json{{"context", 32768}, {"env", {{"CACHE", "old"}}}},
                                      Json{{"context", 32768}, {"env", {{"CACHE", "new"}}}},
                                      NativeModelSettingsGroup::MODEL));
    assert(ModelSettingsGroupChanged(savedTabs, Json{{"context", 65536}, {"gpus", Json::array({0})},
                                                    {"prefill", 4096}, {"cpu_threads", 8}},
                                     NativeModelSettingsGroup::MODEL));
    assert(!ModelSettingsGroupChanged(savedTabs, Json{{"context", 32768}, {"gpus", Json::array({1})},
                                                      {"prefill", 4096}, {"cpu_threads", 8}},
                                      NativeModelSettingsGroup::MODEL));
    assert(!ModelSettingsChanged(savedTabs, savedTabs));
    assert(ModelSettingsChanged(savedTabs, Json{{"context", 65536}, {"gpus", Json::array({0})},
                                                {"prefill", 4096}, {"cpu_threads", 8}}));
    assert(!ModelSettingsChanged(savedTabs, Json{{"context", 32768}, {"gpus", Json::array({0})},
                                                 {"prefill", 4096}, {"cpu_threads", 8}, {"unknown", true}}));
    const Json ablateChoices{{"ablate", {{"available", true}}}};
    const Json ablateUnavailable{{"ablate", {{"available", false}}}};
    assert(NativeModelAblationAvailable(ablateChoices) && !NativeModelAblationAvailable(ablateUnavailable));
    assert(NativeModelAblationVisible(Json::object(), ablateChoices));
    assert(NativeModelAblationVisible(Json{{"ablate", true}}, ablateUnavailable));
    assert(!NativeModelAblationVisible(Json{{"ablate", false}}, ablateUnavailable));
    const Json settingsWithAblate{{"context", 32768}, {"gpus", Json::array({0})},
                                  {"prefill", 4096}, {"cpu_threads", 8}, {"ablate", true}};
    assert(ModelSettingsChanged(savedTabs, settingsWithAblate));
    assert(ModelSettingsGroupChanged(savedTabs, settingsWithAblate, NativeModelSettingsGroup::MODEL));
    assert(ModelSettingsFieldChanged(savedTabs, settingsWithAblate, "ablate"));
    assert(NativeChatAblationControlId == 6000 && NativeChatAblationControlId != NativeChatReasoningControlId);
    assert(NativeModelSettingsApplyEnabled(true, true, true));
    assert(!NativeModelSettingsApplyEnabled(false, true, true));
    assert(!NativeModelSettingsApplyEnabled(true, false, true));
    assert(!NativeModelSettingsApplyEnabled(true, true, false));
    assert(!ModelSettingsEnvironmentChanged(Json{{"env", {{"CACHE", "1"}}}},
                                             Json{{"env", {{"CACHE", "1"}}}}));
    assert(ModelSettingsEnvironmentChanged(Json{{"env", {{"CACHE", "1"}}}},
                                            Json{{"env", {{"CACHE", "2"}}}}));
    assert(ModelSettingsEnvironmentChanged(Json::object(), Json{{"env", Json::object()}}));
    assert(!ModelSettingsEnvironmentChanged(Json{{"env", Json::object()}}, Json::object()));
    assert(ModelSettingsEnvironmentChanged(Json{{"env", {{"CACHE", "1"}}}}, Json::object()));
    const auto environmentRows = ModelSettingsEnvironmentRows(
        Json{{"env", {{"EMPTY", ""}, {"PATH", "C:\\tools=a"}}}});
    assert(environmentRows.size() == 2 && environmentRows[0] == std::make_pair(std::string("EMPTY"), std::string()) &&
           environmentRows[1] == std::make_pair(std::string("PATH"), std::string("C:\\tools=a")));
    const Json editedEnvironment{{"CACHE", "16"}};
    assert(ModelSettingsEnvironmentObject({{"CACHE", "8"}, {"", "ignored"}, {"CACHE", "16"}}) == editedEnvironment);
    const std::string whitespaceEnvKey = " \t\r\n" "\xE2\x80\x83";
    assert(ModelSettingsEnvironmentObject({{whitespaceEnvKey, "ignored"}, {" CACHE ", "kept"}}) ==
           (Json{{" CACHE ", "kept"}}));
    assert(ModelSettingsCommandPreviewText(
        {{"env", {{"KEEP", "1"}, {"CHANGE", "new"}, {"ADD", "3"}}}, {"exe", "maya"},
         {"args", Json::array({"--context", "65536", "--quiet", "--new", "yes"})}},
        {{"env", {{"KEEP", "1"}, {"CHANGE", "old"}}}, {"exe", "maya"},
         {"args", Json::array({"--context", "65536", "--quiet", "--new", "no"})}}) ==
        "Environment:\n  KEEP=1\n+ CHANGE=new\n+ ADD=3\nExecutable: maya\nArguments:\n  --context 65536\n  --quiet\n+ --new yes\n");
    const auto stableWarnings = ModelSettingsConditionalWarnings(
        {{"context", 65536}, {"parallel", 2}, {"gpus", Json::array({0, 1})}, {"tp", 2}, {"draft", "none"}},
        {{"context", 65536}, {"parallel", 2}, {"gpus", Json::array({0, 1})}, {"tp", 2}, {"draft", "none"}},
        {{"tested", 262144}}, "deepseek");
    assert(std::none_of(stableWarnings.begin(), stableWarnings.end(), [](bool warning) { return warning; }));
    const auto deepseekWarnings = ModelSettingsConditionalWarnings(
        {{"context", 262144}},
        {{"context", 600000}, {"parallel", 2}, {"gpus", Json::array({0})}, {"tp", 2},
         {"draft", "dspark"}, {"dspark_conf", 1.1}},
        {{"tested", 262144}}, "deepseek");
    assert(deepseekWarnings[(int)NativeModelSettingsGroup::MODEL] &&
           deepseekWarnings[(int)NativeModelSettingsGroup::GPUS_MEMORY] &&
           deepseekWarnings[(int)NativeModelSettingsGroup::SPEED]);
    const auto deepseekWarningText = ModelSettingsConditionalWarningMessages(
        {{"context", 262144}},
        {{"context", 600000}, {"parallel", 2}, {"gpus", Json::array({0})}, {"tp", 2},
         {"draft", "dspark"}, {"dspark_conf", 1.1}},
        {{"tested", 262144}}, "deepseek");
    const auto containsWarning = [](const auto &warnings, int group, const std::wstring &part) {
        return std::any_of(warnings[group].begin(), warnings[group].end(),
                           [&](const auto &warning) { return warning.find(part) != std::wstring::npos; });
    };
    Json fastPrefillSetting{{"fast_prefill", false}};
    assert(ModelSettingsSetScalar(fastPrefillSetting, "fast_prefill", "true") &&
           fastPrefillSetting["fast_prefill"].is_boolean() && fastPrefillSetting["fast_prefill"].get<bool>());
    const auto fastPrefillWarnings = ModelSettingsConditionalWarningMessages(
        Json{{"fast_prefill", false}}, fastPrefillSetting, Json::object(), "deepseek");
    const auto fastPrefillOffWarnings = ModelSettingsConditionalWarningMessages(
        Json{{"fast_prefill", false}}, Json{{"fast_prefill", false}}, Json::object(), "deepseek");
    assert(containsWarning(fastPrefillWarnings, (int)NativeModelSettingsGroup::SPEED, L"may shift answers slightly") &&
           fastPrefillOffWarnings[(int)NativeModelSettingsGroup::SPEED].empty());
    assert(containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::MODEL, L"untested") &&
           containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::MODEL, L"Beyond 256K") &&
           containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::MODEL, L"conversations of") &&
           containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::MODEL, L"DSpark drafts") &&
           containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::GPUS_MEMORY, L"With one GPU") &&
           containsWarning(deepseekWarningText, (int)NativeModelSettingsGroup::SPEED, L"0 to 1"));
    const auto fractionalContextWarning = ModelSettingsConditionalWarningMessages(
        {{"context", 262144}}, {{"context", 272000}, {"parallel", 4}}, {{"tested", 270000}}, "deepseek");
    assert(containsWarning(fractionalContextWarning, (int)NativeModelSettingsGroup::MODEL, L"Beyond 270k") &&
           containsWarning(fractionalContextWarning, (int)NativeModelSettingsGroup::MODEL, L"of 272k each"));
    assert(ModelSettingsConditionalWarnings({{"context", 262144}},
        {{"context", 262144}, {"parallel", 2}, {"draft", "dflash2"}}, Json::object(), "glm")[(int)NativeModelSettingsGroup::MODEL]);
    const auto glmWarningText = ModelSettingsConditionalWarningMessages({{"context", 262144}},
        {{"context", 262144}, {"parallel", 2}, {"draft", "dflash2"}}, Json::object(), "glm");
    assert(containsWarning(glmWarningText, (int)NativeModelSettingsGroup::MODEL, L"engine drafts with MTP"));
    assert(ModelSettingsConditionalWarnings({{"context", 262144}},
        {{"context", 262144}, {"parallel", 2}}, Json::object(), "minimax")[(int)NativeModelSettingsGroup::MODEL]);
    const auto mmWarningText = ModelSettingsConditionalWarningMessages({{"context", 262144}},
        {{"context", 600000}, {"parallel", 2}}, Json::object(), "minimax");
    assert(containsWarning(mmWarningText, (int)NativeModelSettingsGroup::MODEL, L"whole cache") &&
           containsWarning(mmWarningText, (int)NativeModelSettingsGroup::MODEL, L"conversations of"));
    assert(std::wstring(ModelSettingsInvalidWarning("dspark_conf")) == L"A number from 0 to 1, or empty." &&
           std::wstring(ModelSettingsInvalidWarning("prefill_chunk")) == L"A whole number, or empty.");
    assert(NativeHttpApiErrorText(R"({"error":{"message":"engine error"}})", 400) == "engine error");
    assert(NativeHttpApiErrorText(R"({"error":"model error"})", 403) == "model error");
    assert(NativeHttpApiErrorText(R"({"error":"model error"})", 403, true) == "HTTP 403");
    assert(NativeHttpApiErrorText(R"({"error":{"message":""}})", 409) == "HTTP 409");
    assert(NativeHttpApiErrorText("not json", 502) == "HTTP 502");
    SYSTEMTIME localNow{};
    GetLocalTime(&localNow);
    SYSTEMTIME midnight = localNow, utcMidnight{};
    midnight.wHour = midnight.wMinute = midnight.wSecond = midnight.wMilliseconds = 0;
    FILETIME startFile{};
    assert(TzSpecificLocalTimeToSystemTime(nullptr, &midnight, &utcMidnight));
    assert(SystemTimeToFileTime(&utcMidnight, &startFile));
    ULARGE_INTEGER startTicks{};
    startTicks.HighPart = startFile.dwHighDateTime; startTicks.LowPart = startFile.dwLowDateTime;
    constexpr ULONGLONG dayTicks = 10'000'000ULL * 86400ULL;
    auto chatTime = [&](ULONGLONG age) {
        const ULONGLONG ticks = startTicks.QuadPart - age;
        ULARGE_INTEGER value{}; value.QuadPart = ticks;
        return FILETIME{value.LowPart, value.HighPart};
    };
    assert(NativeChatDayGroup(startFile, localNow) == L"Today");
    wchar_t chatTodayText[80]{};
    const int chatTodayLength = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &midnight, nullptr,
                                               chatTodayText, _countof(chatTodayText));
    assert(chatTodayLength > 1 && NativeChatRelativeTime(startFile, localNow) ==
           std::wstring(chatTodayText, chatTodayLength - 1));
    assert(NativeChatDayGroup(chatTime(dayTicks - 1), localNow) == L"Yesterday");
    assert(NativeChatRelativeTime(chatTime(dayTicks), localNow) == L"Yesterday");
    assert(NativeChatDayGroup(chatTime(dayTicks), localNow) == L"Yesterday");
    assert(NativeChatDayGroup(chatTime(dayTicks + 1), localNow) == L"Previous 7 days");
    assert(NativeChatDayGroup(chatTime(7 * dayTicks), localNow) == L"Previous 7 days");
    assert(NativeChatDayGroup(chatTime(7 * dayTicks + 1), localNow) == L"Previous 30 days");
    assert(NativeChatDayGroup(chatTime(30 * dayTicks), localNow) == L"Previous 30 days");
    const auto older = NativeChatDayGroup(chatTime(30 * dayTicks + 1), localNow);
    assert(older != L"Today" && older != L"Yesterday" && older != L"Previous 7 days" && older != L"Previous 30 days");
    const auto oldRelative = NativeChatRelativeTime(chatTime(31 * dayTicks), localNow);
    assert(!oldRelative.empty() && oldRelative != L"Yesterday");
    SYSTEMTIME previousYearChat = localNow;
    --previousYearChat.wYear;
    previousYearChat.wMonth = 1; previousYearChat.wDay = 5;
    previousYearChat.wHour = 12; previousYearChat.wMinute = previousYearChat.wSecond = previousYearChat.wMilliseconds = 0;
    SYSTEMTIME previousYearChatUtc{};
    assert(TzSpecificLocalTimeToSystemTime(nullptr, &previousYearChat, &previousYearChatUtc));
    FILETIME previousYearChatFile{};
    assert(SystemTimeToFileTime(&previousYearChatUtc, &previousYearChatFile));
    wchar_t previousYearChatText[80]{};
    const int previousYearChatLength = GetDateFormatW(LOCALE_USER_DEFAULT, 0, &previousYearChat, L"MMM d",
                                                       previousYearChatText, _countof(previousYearChatText));
    assert(previousYearChatLength > 1 && NativeChatRelativeTime(previousYearChatFile, localNow) ==
           std::wstring(previousYearChatText, previousYearChatLength - 1));
    assert(NativeChatMenuFor(0) == NativeChatMenuMode::INSTRUCTIONS_ONLY);
    assert(NativeChatMenuFor(1) == NativeChatMenuMode::FULL);
    assert(NativeChatMenuVisibleRow(true, 3, 0, 5) == 3 && NativeChatMenuVisibleRow(false, -1, 4, 5) == 4 &&
           NativeChatMenuVisibleRow(true, -1, 2, 5) == -1 && NativeChatMenuVisibleRow(false, -1, 5, 5) == -1);
    const auto emptyChatActions = NativeChatToolbarActionsFor(true, false, false, false);
    assert(!emptyChatActions.editLast && !emptyChatActions.regenerate && !emptyChatActions.cancelEdit &&
           !emptyChatActions.answerOptions);
    const auto answeredChatActions = NativeChatToolbarActionsFor(true, true, true, false);
    assert(answeredChatActions.editLast && answeredChatActions.regenerate && !answeredChatActions.cancelEdit &&
           answeredChatActions.answerOptions);
    const auto editingChatActions = NativeChatToolbarActionsFor(true, true, true, true);
    assert(!editingChatActions.editLast && !editingChatActions.regenerate && editingChatActions.cancelEdit &&
           !editingChatActions.answerOptions);
    const auto backgroundChatActions = NativeChatToolbarActionsFor(false, true, true, false);
    assert(!backgroundChatActions.editLast && !backgroundChatActions.regenerate &&
           !backgroundChatActions.cancelEdit && !backgroundChatActions.answerOptions);
    assert(NativeChatToolbarActionsEnabled(false));
    assert(!NativeChatToolbarActionsEnabled(true));
    assert(NativeChatFolderDestination(false, L"new-folder") == L"new-folder");
    assert(NativeChatFolderDestination(false, L"existing-folder") == L"existing-folder");
    assert(NativeChatFolderDestination(true, L"same-folder").empty());
    assert(NativeFolderCollapsedAfterChatMove(false, true) == false);
    assert(NativeFolderCollapsedAfterChatMove(false, false) == false);
    assert(NativeFolderCollapsedAfterChatMove(true, true) == true);
    assert(NativeFolderCollapsedAfterChatMove(true, false) == false);
    assert(NativeFolderDeleteCanRemove(0, 0) && NativeFolderDeleteCanRemove(2, 2));
    assert(!NativeFolderDeleteCanRemove(1, 2));
    assert(!NativeComposerShowsTools(0));
    assert(NativeComposerShowsTools(1));
    assert(NativeComposerHeight(1, false, false, 18, 900) == 42);
    assert(NativeComposerHeight(2, true, false, 18, 900) == 82);
    assert(NativeComposerHeight(6, false, false, 18, 900) == 128);
    assert(NativeComposerHeight(1, false, true, 18, 900) == 82);
    assert(NativeComposerHeight(6, true, false, 18, 100) == 42);
    const auto wideChatColumn = NativeChatColumnFor(264, 1636, 220, 768);
    assert(wideChatColumn.left == 808 && wideChatColumn.width == 768);
    const auto narrowChatColumn = NativeChatColumnFor(24, 650, 220, 768);
    assert(narrowChatColumn.left == 244 && narrowChatColumn.width == 430);
    const auto collapsedChatColumn = NativeChatColumnFor(24, 1200, 0, 768);
    assert(collapsedChatColumn.left == 240 && collapsedChatColumn.width == 768);
    assert(NativeChatColumnFor(24, 180, 220, 768).width == 0);
    assert(NativeChatModelMenuActionFor(1) == NativeChatModelMenuAction::CONTEXT);
    assert(NativeChatModelMenuActionFor(2) == NativeChatModelMenuAction::SAMPLING);
    assert(NativeChatModelMenuActionFor(3) == NativeChatModelMenuAction::MODELS);
    assert(NativeChatModelMenuActionFor(0) == NativeChatModelMenuAction::NONE);
    assert(NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, true, true));
    assert(NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, true, false));
    assert(NativeChatGreetingVisible(true, false, false, false));
    assert(NativeChatGreetingVisible(true, true, false, false));
    assert(!NativeChatGreetingVisible(true, true, true, false));
    assert(!NativeChatGreetingVisible(true, true, false, true));
    assert(!NativeChatGreetingVisible(false, false, false, false));
    assert(!NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, true, false));
    assert(!NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, true, true));
    assert(!NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, false, true, true));
    assert(NativeChatModelButtonShouldOpen(NativeGreetingModelButtonId, true, false, true));
    assert(!NativeChatModelButtonShouldOpen(NativeChatModelButtonId, true, false, true));
    assert(NativeChatModelButtonName("deepseek-v4.1-flash-abliterated") == "DeepSeek V4.1 Flash Abliterated");
    assert(NativeChatModelButtonName("cyber-frost-3.8-ps") == "Qwen-3.8-Flash-Next");
    assert(NativeChatModelButtonName("CYBER-FROST-3.8-PS") == "Qwen-3.8-Flash-Next" &&
           NativeChatModelButtonName("cyber-frost-3.8beta-ps") == "Qwen-3.8-Flash-Next");
    const auto &normalIdeas = NativeChatIdeas(false);
    const std::array<std::wstring, 4> normalIdeaLabels = {
        L"A three.js scene in one HTML file", L"Explain and improve some code",
        L"Plan and write a small tool", L"Explain a topic clearly"};
    const std::array<std::wstring, 4> normalIdeaPrompts = {
        L"Write a single-file HTML page with a three.js scene: a slowly rotating, softly lit crystal on a dark background, with orbit controls.",
        L"Explain step by step what this code does, then suggest two improvements:\n\n```\n\n```",
        L"Plan a small Python command-line tool that renames photos by the date they were taken. List the steps, then write it.",
        L"Summarize the key ideas of mixture-of-experts language models for a curious engineer, in five short paragraphs."};
    const auto &turboIdeas = NativeChatIdeas(true);
    const std::array<std::wstring, 4> turboIdeaLabels = {
        L"Write a Python function", normalIdeaLabels[0], normalIdeaLabels[1], L"Fix a bug"};
    for (size_t i = 0; i < normalIdeas.size(); ++i)
        assert(normalIdeas[i].label == normalIdeaLabels[i] && normalIdeas[i].prompt == normalIdeaPrompts[i]);
    assert(turboIdeas[0].label == turboIdeaLabels[0] &&
           turboIdeas[0].prompt == L"Write a Python function that merges overlapping intervals in a list of (start, end) pairs, with a few tests.");
    assert(turboIdeas[1].label == turboIdeaLabels[1] && turboIdeas[1].prompt == normalIdeas[0].prompt);
    assert(turboIdeas[2].label == turboIdeaLabels[2] && turboIdeas[2].prompt == normalIdeas[1].prompt);
    assert(turboIdeas[3].label == turboIdeaLabels[3] &&
           turboIdeas[3].prompt == L"This function should return the median of a list but is wrong for even lengths. Find the bug and fix it:\n\n```python\n\n```");
    assert(ChatSamplingPresetName(Json{{"temperature", 0}}) == "Precise");
    assert(ChatSamplingPresetName(Json{{"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0}}) == "Balanced");
    assert(ChatSamplingPresetName(Json{{"temperature", 1.0}, {"top_p", 0.95}, {"top_k", 0}, {"min_p", 0.05}}) == "Creative");
    assert(ChatSamplingPresetName(Json{{"temperature", 0.4}}) == "Custom");
    assert(std::wstring(ChatSamplingPresetValue(0, 0)) == L"0" &&
           ChatSamplingPresetValue(0, 1) == nullptr && ChatSamplingPresetValue(0, 2) == nullptr &&
           ChatSamplingPresetValue(0, 3) == nullptr && std::wstring(ChatSamplingPresetValue(1, 1)) == L"0.95" &&
           ChatSamplingPresetValue(3, 0) == nullptr);
    assert(NativeSamplingAutosaveNeeded(false, true, true));
    assert(!NativeSamplingAutosaveNeeded(true, true, true));
    assert(!NativeSamplingAutosaveNeeded(false, false, true));
    assert(!NativeSamplingAutosaveNeeded(false, true, false));
    assert(NativeSharedDefaultsSyncReady(true, true, false));
    assert(!NativeSharedDefaultsSyncReady(false, true, false));
    assert(!NativeSharedDefaultsSyncReady(true, false, false));
    assert(!NativeSharedDefaultsSyncReady(true, true, true));
    assert(NativeChatSelectionAfterDelete(3, 1, 3) == 2);
    assert(NativeChatSelectionAfterDelete(1, 3, 3) == 1);
    assert(NativeChatSelectionAfterDelete(2, 2, 3) == 0);
    assert(NativeChatSelectionAfterDelete(0, 0, 0) == -1);
    assert(NativeCanReuseBlankChat(true, true, true));
    assert(!NativeCanReuseBlankChat(false, true, true));
    assert(!NativeCanReuseBlankChat(true, false, true));
    assert(!NativeCanReuseBlankChat(true, true, false));
    assert(!NativeChatUndoAvailable(0) && NativeChatUndoAvailable(3));
    std::vector<int64_t> restoredChatTimes{3, 2}; // C, D remain after deleting B and then A.
    restoredChatTimes.push_back(1); // Undo A appends it; the web app re-sorts by update time.
    assert((NativeChatRecentOrder(restoredChatTimes) == std::vector<int>{0, 1, 2}));
    restoredChatTimes.push_back(4); // Undo B restores the original recent-chat order.
    assert((NativeChatRecentOrder(restoredChatTimes) == std::vector<int>{3, 0, 1, 2}));
    assert(NativeManagerTitle(L"1.1.50", true, 272000) == L"Strata Engine | Maya 1.1.50 | Vision ON | 272K Context");
    assert(NativeManagerTitle(L"1.1.50", false, 0) == L"Strata Engine | Maya 1.1.50 | Vision OFF | Context unavailable");
    assert((NativeChatRecentOrder({10, 30, 20, 30}) == std::vector<int>{1, 3, 2, 0}));
    assert((NativeChatRecentOrder({}) == std::vector<int>{}));
    const DWORD wrappedCodeStyle = MayaCodeViewerEditStyle(true);
    const DWORD unwrappedCodeStyle = MayaCodeViewerEditStyle(false);
    assert((wrappedCodeStyle & (ES_AUTOHSCROLL | WS_HSCROLL)) == 0);
    assert((unwrappedCodeStyle & (ES_AUTOHSCROLL | WS_HSCROLL)) == (ES_AUTOHSCROLL | WS_HSCROLL));
    assert(MayaGitHubRepo("https://github.com/mw00/project-maya.git\r\n") == "mw00/project-maya");
    assert(MayaGitHubRepo("  https://github.com/mw00/project-maya/  \n") == "mw00/project-maya");
    assert(MayaGitHubRepo("https://example.com/mw00/project-maya.git\n").empty());

    assert(!NativeTextContentsChanged(L"Maya v1.1.50 · DeepSeek", L"Maya v1.1.50 · DeepSeek"));
    assert(NativeTextContentsChanged(L"Maya v1.1.49 · DeepSeek", L"Maya v1.1.50 · DeepSeek"));
    assert(!NativeListContentsChanged(std::vector<std::wstring>{L"model A", L"model B"},
                                      std::vector<std::wstring>{L"model A", L"model B"}));
    assert(NativeListContentsChanged(std::vector<std::wstring>{L"model A"},
                                      std::vector<std::wstring>{L"model A", L"model B"}));
    assert(std::wstring(MayaWindowsStartup::ManagerStartupRunValue) == L"MayaXSWinManager");
    assert(std::wstring(MayaWindowsStartup::ManagerStartupRunValue) != L"MayaXSWinControl");
    assert(!MayaWindowsStartup::AutoStartRequiresManager(false, false));
    assert(!MayaWindowsStartup::AutoStartRequiresManager(false, true));
    assert(!MayaWindowsStartup::AutoStartRequiresManager(true, false));
    assert(MayaWindowsStartup::AutoStartRequiresManager(true, true));
    assert(!MayaWindowsStartup::ShouldStartMaya(false, true));
    assert(!MayaWindowsStartup::ShouldStartMaya(true, false));
    assert(MayaWindowsStartup::ShouldStartMaya(true, true));
    assert(NativeStartupQuestion({L"--ask", L"what is 2 + 2?"}) == L"what is 2 + 2?");
    assert(NativeStartupQuestion({L"--profile", L"maya", L"--ask=hello there"}) == L"hello there");
    assert(NativeStartupQuestion({L"--ask"}).empty());
    assert(NativeStartupQuestion({L"--unknown", L"hello"}).empty());
    assert(NativeStartupQuestionCanSend(L"hello", true, true, true, true));
    assert(!NativeStartupQuestionCanSend(L"", true, true, true, true));
    assert(!NativeStartupQuestionCanSend(L"hello", false, true, true, true));
    assert(!NativeStartupQuestionCanSend(L"hello", true, false, true, true));
    assert(!NativeStartupQuestionCanSend(L"hello", true, true, false, true));
    assert(!NativeStartupQuestionCanSend(L"hello", true, true, true, false));
    const std::vector<NativeMessageRange> messageRanges = {{0, 5, 1}, {10, 20, 4}};
    assert(NativeMessageAt(messageRanges, 0) == 1 && NativeMessageAt(messageRanges, 4) == 1);
    assert(NativeMessageAt(messageRanges, 5) == -1 && NativeMessageAt(messageRanges, 9) == -1);
    assert(NativeMessageAt(messageRanges, 10) == 4 && NativeMessageAt(messageRanges, 19) == 4);
    assert(NativeMessageAt(messageRanges, 20) == -1);
    const std::vector<NativeMessageRange> answerRanges = {{30, 45, 2}, {50, 65, 5}};
    assert(NativeMessageAt(answerRanges, 30) == 2 && NativeMessageAt(answerRanges, 44) == 2);
    assert(NativeMessageAt(answerRanges, 45) == -1 && NativeMessageAt(answerRanges, 50) == 5);
    assert(NativeStatusPollIntervalMs(false) == 2000u);
    assert(NativeStatusPollIntervalMs(true) == 10000u);
    assert(NativeStatusUiNeedsRefresh(true, false, false));
    assert(NativeStatusUiNeedsRefresh(false, true, false));
    assert(NativeStatusUiNeedsRefresh(false, false, true));
    assert(!NativeStatusUiNeedsRefresh(false, false, false));
    assert(NativeMetricsPollIntervalMs(false, true, false) == 1000u);
    assert(NativeMetricsPollIntervalMs(false, true, false, false) == 10000u);
    assert(NativeMetricsPollIntervalMs(false, false, false) == 10000u);
    assert(NativeMetricsPollIntervalMs(true, true, false) == 5000u);
    assert(NativeMetricsPollIntervalMs(false, true, true) == 10000u);
    assert(NativeMetricsPollIntervalMs(true, false, true) == 30000u);
    assert(NativeBudgetSupported(Json{{"supported", true}}));
    assert(!NativeBudgetSupported(Json{{"supported", false}}));
    assert(!NativeBudgetSupported(Json::object()) && !NativeBudgetSupported(Json{{"supported", "true"}}));
    assert(!NativeJobShowsProgress(true)); // health and metrics polling stays invisible
    assert(NativeJobShowsProgress(false));
    assert(!NativeJobShowsProgress(false, true)); // routine model/catalog refresh stays invisible
    assert(!NativePollShowsError(true));
    assert(NativePollShowsError(false));
    assert(!NativePollShowsError(false, true)); // transient routine poll errors stay quiet while retrying
    assert(!NativePollShowsError(true, false));
    assert(!NativePollInvalidatesWindow(true));
    assert(NativePollInvalidatesWindow(false));
    assert(!NativePollInvalidatesWindow(false, true)); // routine refresh redraws only changed controls
    assert(NativeApiConnectionAfterFailure(NativeApiConnection::CONNECTING, 1, false) == NativeApiConnection::CONNECTING);
    assert(NativeApiConnectionAfterFailure(NativeApiConnection::CONNECTING, 3, false) == NativeApiConnection::UNREACHABLE);
    assert(NativeApiConnectionAfterFailure(NativeApiConnection::READY, 1, false) == NativeApiConnection::READY);
    assert(NativeApiConnectionAfterFailure(NativeApiConnection::READY, 1, true) == NativeApiConnection::API_KEY);
    assert(NativeApiConnectionAfterFailure(NativeApiConnection::API_KEY, 3, false) == NativeApiConnection::UNREACHABLE);
    assert(NativeApiConnectionAfterSuccess() == NativeApiConnection::READY);
    assert(NativeMetricsHttpStatusOrZero(Json()) == 0u);
    assert(NativeMetricsHttpStatusOrZero(Json::object()) == 0u);
    assert(NativeMetricsHttpStatusOrZero(Json{{"http_status", 401u}}) == 401u);
    assert(NativeMetricsHttpStatusOrZero(Json{{"http_status", 401}}) == 401u);
    assert(NativeMetricsHttpStatusOrZero(Json{{"http_status", nullptr}}) == 0u);
    assert(NativeMetricsHttpStatusOrZero(Json{{"http_status", "401"}}) == 0u);
    assert(NativeMetricsPayloadIsObject(Json::object()));
    assert(!NativeMetricsPayloadIsObject(Json()));
    assert(!NativeMetricsPayloadIsObject(Json::array()));
    unsigned metricFailures = 0;
    auto metricConnection = NativeApiConnection::CONNECTING;
    metricConnection = NativeApiConnectionAfterMetricsPoll(metricConnection, metricFailures, 0, false);
    assert(metricConnection == NativeApiConnection::CONNECTING && metricFailures == 1);
    metricConnection = NativeApiConnectionAfterMetricsPoll(metricConnection, metricFailures, 0, false);
    assert(metricConnection == NativeApiConnection::CONNECTING && metricFailures == 2);
    metricConnection = NativeApiConnectionAfterMetricsPoll(metricConnection, metricFailures, 0, false);
    assert(metricConnection == NativeApiConnection::UNREACHABLE && metricFailures == 3);
    metricConnection = NativeApiConnectionAfterMetricsPoll(metricConnection, metricFailures, 0, true);
    assert(metricConnection == NativeApiConnection::READY && metricFailures == 0);
    metricConnection = NativeApiConnectionAfterMetricsPoll(metricConnection, metricFailures, 401, false);
    assert(metricConnection == NativeApiConnection::API_KEY);
    assert(std::wstring(NativeApiConnectionText(NativeApiConnection::API_KEY)) == L"API key needed");
    assert(std::wstring(NativeApiConnectionText(NativeApiConnection::UNREACHABLE)) == L"Server not reachable");
    assert(NativeStatusCardClicked(NativeStatusCardId, 0));
    static_assert(NativeChatOptionsId != NativeStatusCardId);
    assert(!NativeStatusCardClicked(NativeStatusCardId, 1));
    assert(!NativeStatusCardClicked(NativeStatusCardId + 1, 0));
    assert(ModelOperationErrorText(true, L"API timeout") == L"Model load failed: API timeout");
    assert(ModelOperationErrorText(false, L"API timeout") == L"Model unload failed: API timeout");
    assert(ModelOperationRefreshErrorText(ModelOperationErrorText(true, L"API timeout")) ==
           L"Model load failed: API timeout Current model state could not be refreshed.");
    assert(NativeModelSettingsRequestMayOverlapChat(true, false));
    assert(!NativeModelSettingsRequestMayOverlapChat(false, false));
    assert(!NativeModelSettingsRequestMayOverlapChat(true, true));
    assert(NativeModelSettingsAutoPreviewReady(true, true, true, false));
    assert(!NativeModelSettingsAutoPreviewReady(false, true, true, false));
    assert(!NativeModelSettingsAutoPreviewReady(true, false, true, false));
    assert(!NativeModelSettingsAutoPreviewReady(true, true, false, false));
    assert(!NativeModelSettingsAutoPreviewReady(true, true, true, true));
    assert(NativeModelSettingsResetEnabled(true, false));
    assert(!NativeModelSettingsResetEnabled(false, false));
    assert(!NativeModelSettingsResetEnabled(true, true));
    assert(NativeModelImageToggleEnabled(true, false));
    assert(NativeModelImageToggleEnabled(false, true));
    assert(!NativeModelImageToggleEnabled(false, false));
    assert(ModelSwitchWatchStep(false, true, true, false) == ModelSwitchWatchState::WAITING);
    assert(ModelSwitchWatchStep(true, false, false, false) == ModelSwitchWatchState::WAITING);
    assert(ModelSwitchWatchStep(true, true, true, false) == ModelSwitchWatchState::COMPLETE);
    assert(ModelSwitchWatchStep(false, true, true, true) == ModelSwitchWatchState::TIMED_OUT);
    assert(!NativeRestartTimeoutElapsed(1000, 900999));
    assert(NativeRestartTimeoutElapsed(1000, 901000));
    assert(!NativeRestartTimeoutElapsed(901000, 1000));
    assert((NativeContextStops(4096, 262144, {131072, 272000}) ==
            std::vector<int>{4096, 8192, 16384, 32768, 49152, 65536, 98304, 131072, 196608, 262144, 272000}));
    assert((NativeContextStops(65536, 131072, {32768, 196608}) == std::vector<int>{32768, 65536, 98304, 131072, 196608}));
    assert((NativeContextStops(0, 131072, {65536}) ==
            std::vector<int>{4096, 8192, 16384, 32768, 49152, 65536, 98304, 131072}));
    assert(NativeContextStops(131072, 4096, {65536}).empty());
    const auto gpuContextWarning = NativeContextReloadWarning(
        {{"context", 131072}, {"tested", 131072}, {"load_s", 90}, {"kv_gb", 2.0},
         {"kv_ctx", 131072}, {"vram_gb", 16.0}, {"vram_slots", 64}, {"kv_resident", 0}},
        262144, 300000, true);
    assert(gpuContextWarning.find(L"2 minutes") != std::wstring::npos &&
           gpuContextWarning.find(L"4.0 GB of GPU memory") != std::wstring::npos &&
           gpuContextWarning.find(L"beyond the largest size tested") != std::wstring::npos &&
           gpuContextWarning.find(L"open chat is estimated to exceed") != std::wstring::npos);
    const auto ramContextWarning = NativeContextReloadWarning(
        {{"context", 131072}, {"kv_resident", 49152}, {"kv_ram_gb", 4.0}, {"ram_free_gb", 10.0}},
        262144, 0, false);
    assert(ramContextWarning.find(L"8.0 GB pinned in RAM") != std::wstring::npos &&
           ramContextWarning.find(L"leaves little free RAM") != std::wstring::npos);
    assert(NativeChatResumeShouldStop(404));
    assert(NativePendingChatResumeEligible(true, true, false));
    assert(!NativePendingChatResumeEligible(false, true, false) &&
           !NativePendingChatResumeEligible(true, false, false) &&
           !NativePendingChatResumeEligible(true, true, true));
     assert(NativeChatNeedsApiKeyHint(401, true));
     assert(!NativeChatNeedsApiKeyHint(401, false));
     assert(!NativeChatNeedsApiKeyHint(403, true));
     assert(std::string(NativeChatApiKeyRequiredMessage) ==
            "This server needs an API key: add it in Settings › Connect your tools.");
    assert(!NativeChatCancelReady(false, false, true));
    assert(!NativeChatCancelReady(true, false, false));
    assert(!NativeChatCancelReady(true, true, true));
    assert(NativeChatCancelReady(true, false, true));
    assert(NativeChatStopped(true, false));
    assert(NativeChatFailureText(true, "Maya chat stream disconnected.").empty());
    assert(NativeChatFailureText(false, "Maya chat stream disconnected.") == "Maya chat stream disconnected.");
    assert(NativeGreetingModelCard(L"DeepSeek V4.1 Flash", 272000, true, true) ==
           L"DeepSeek V4.1 Flash  ·  Ready  ·  272K context  ·  Images on  ·  Switch model");
    assert(NativeGreetingModelCard(L"", 0, false, false) ==
           L"No model loaded  ·  Unavailable  ·  Images off  ·  Switch model");
    assert(NativeChatStopped(false, true));
    assert(!NativeChatStopped(false, false));
    assert(NativeChatAnswerShouldPersist(false, false, false, 0, false, true, false));
    assert(NativeChatAnswerShouldPersist(false, false, false, 0, false, false, true));
    assert(NativeChatAnswerShouldPersist(true, false, false, 0, false, false, false));
    assert(!NativeChatAnswerShouldPersist(false, false, false, 0, false, false, false));
    assert(NativeChatShouldPersistJobOnClose(false, "job-123"));
    assert(!NativeChatShouldPersistJobOnClose(true, "job-123"));
    assert(!NativeChatShouldPersistJobOnClose(false, ""));
    assert(std::string(NativeChatResumeMissingMessage) ==
           "This answer was cut off when the page was reloaded, and the server no longer has it.");
    assert(!NativeChatResumeShouldStop(401));
    assert(!NativeChatResumeShouldStop(408));
    assert(!NativeChatResumeShouldStop(429));
    assert(!NativeChatResumeShouldStop(500));
    assert(NativeSystemThemeChanged(false, true));
    assert(NativeSystemThemeChanged(true, false));
    assert(!NativeSystemThemeChanged(true, true));
    assert(NativeTitlebarUsesDarkMode(false));
    assert(!NativeTitlebarUsesDarkMode(true));
    const auto darkDefault = NativeThemeColorsFor(false, false);
    assert(darkDefault.background == 0x19191d && darkDefault.sidebar == 0x111114 && darkDefault.card == 0x222227 &&
           darkDefault.fill == 0x2a2a30 && darkDefault.surface3 == 0x34343b && darkDefault.line == 0x2e2e35 &&
           darkDefault.text == 0xececf1 && darkDefault.softText == 0xc9c9d3 && darkDefault.mutedText == 0x9d9daa &&
           darkDefault.open == 0x242427 &&
           darkDefault.warningText == 0xfcd34d && darkDefault.accent == 0x8b7cff && darkDefault.accentText == 0xb4aaff &&
           darkDefault.info == 0x38bdf8 && darkDefault.infoText == 0x7dd3fc && darkDefault.ok == 0x34d399 &&
           darkDefault.okText == 0x6ee7b7 && darkDefault.danger == 0xfb7185 && darkDefault.dangerText == 0xfda4af);
    const auto lightDefault = NativeThemeColorsFor(true, false);
    assert(lightDefault.background == 0xffffff && lightDefault.sidebar == 0xf7f7f9 && lightDefault.card == 0xffffff &&
           lightDefault.fill == 0xf2f2f5 && lightDefault.surface3 == 0xe8e8ed && lightDefault.line == 0xe6e6eb &&
           lightDefault.text == 0x18181d && lightDefault.softText == 0x3a3a44 && lightDefault.mutedText == 0x676773 &&
           lightDefault.open == 0xeaeaec &&
           lightDefault.warningText == 0x92400e && lightDefault.accent == 0x6d5cf6 && lightDefault.accentText == 0x5143d9 &&
           lightDefault.info == 0x0284c7 && lightDefault.infoText == 0x0369a1 && lightDefault.ok == 0x10b981 &&
           lightDefault.okText == 0x047857 && lightDefault.danger == 0xe11d48 && lightDefault.dangerText == 0xbe123c);
    const auto darkViolet = NativeThemeColorsFor(false, true);
    assert(darkViolet.background == 0x090a14 && darkViolet.sidebar == 0x0c0d20 && darkViolet.card == 0x11132a &&
           darkViolet.fill == 0x171a34 && darkViolet.surface3 == 0x1e2240 && darkViolet.line == 0x252a4a &&
           darkViolet.text == 0xeceefe && darkViolet.softText == 0xc3c7e6 && darkViolet.mutedText == 0x8a8fb4 &&
           darkViolet.open == 0x1f2032 &&
           darkViolet.warningText == 0xfcd34d && darkViolet.accent == 0x8b7cff && darkViolet.accentText == 0xb4aaff &&
           darkViolet.info == 0x38bdf8 && darkViolet.infoText == 0x7dd3fc && darkViolet.ok == 0x34d399 &&
           darkViolet.okText == 0x6ee7b7 && darkViolet.danger == 0xfb7185 && darkViolet.dangerText == 0xfda4af);
    const auto lightViolet = NativeThemeColorsFor(true, true);
    assert(lightViolet.background == 0xf4f4fa && lightViolet.sidebar == 0xeeeef8 && lightViolet.card == 0xffffff &&
           lightViolet.fill == 0xf0f0f8 && lightViolet.surface3 == 0xe8e8f4 && lightViolet.line == 0xe0e1ef &&
           lightViolet.text == 0x14152b && lightViolet.softText == 0x33365a && lightViolet.mutedText == 0x62668a &&
           lightViolet.open == 0xe1e1ec &&
           lightViolet.warningText == 0x92400e && lightViolet.accent == 0x6d5cf6 && lightViolet.accentText == 0x5143d9 &&
           lightViolet.info == 0x0284c7 && lightViolet.infoText == 0x0369a1 && lightViolet.ok == 0x10b981 &&
           lightViolet.okText == 0x047857 && lightViolet.danger == 0xe11d48 && lightViolet.dangerText == 0xbe123c);
    assert(NativeSettingsInteger(L"0") == 0);
    assert(NativeSettingsInteger(L"  262144  ") == 262144);
    for (const auto *bad : {L"", L" ", L"+256", L"-0", L"1 0", L"256x", L"2147483648"})
    {
        bool rejected = false;
        try { NativeSettingsInteger(bad); }
        catch (const std::runtime_error &) { rejected = true; }
        assert(rejected);
    }
    const std::string utf8Release = "abc\xE2\x82\xAC";
    assert(MayaBoundedUtf8(utf8Release, 4) == "abc...");
    assert(MayaBoundedUtf8(utf8Release, 6) == utf8Release);
    const Json updateCard{{"ready", true}, {"dependency_ok", true}, {"online", true}, {"update", {
        {"current", "1.1.50"},
        {"newer", true}, {"can_update", true}, {"latest", {{"version", "1.1.51"}, {"tag", "v1.1.51"}, {"summary", "Engine and catalog updates."},
                                       {"url", "https://github.com/mw00/project-maya/releases/tag/v1.1.51"}}}}}};
    assert(MayaUpdateCheckVerified(updateCard["update"]));
    assert(MayaUpdateCanStart(updateCard));
    assert(MayaUpdateStatusText(updateCard) == "Update available; choose Install in About and updates.");
    assert(MayaUpdateSummaryText(updateCard).find("Maya v1.1.51 is available.") != std::string::npos);
    assert(MayaUpdateCardText(updateCard).find("Maya v1.1.51 is available.") != std::string::npos);
    assert(MayaUpdatePreflightText(updateCard).find("Engine and catalog updates.") != std::string::npos);
    auto dependencyBlockedUpdate = updateCard;
    dependencyBlockedUpdate["dependency_ok"] = false;
    dependencyBlockedUpdate["dependency_report"] = "CUDA runtime check failed.";
    assert(!MayaUpdateCanStart(dependencyBlockedUpdate));
    assert(MayaUpdatePreflightText(dependencyBlockedUpdate).find("Preflight is blocked.") != std::string::npos &&
           MayaUpdatePreflightText(dependencyBlockedUpdate).find("CUDA runtime check failed.") != std::string::npos);
    auto nullUpdateFields = updateCard;
    nullUpdateFields["online"] = nullptr;
    nullUpdateFields["ready"] = nullptr;
    nullUpdateFields["update"]["can_update"] = false;
    nullUpdateFields["update"]["blocker"] = nullptr;
    nullUpdateFields["update"]["preserve_tracked_changes"] = nullptr;
    nullUpdateFields["update"]["preserved_paths"] = nullptr;
    assert(!MayaUpdateCanStart(nullUpdateFields));
    assert(MayaUpdatePreflightText(nullUpdateFields).find("Preflight is blocked.") != std::string::npos);
    assert(MayaUpdateCardText(nullUpdateFields).find("Blocked:") == std::string::npos);
    const Json latestUpdate{{"update", {{"current", "1.1.51"}, {"newer", false}, {"latest", {{"version", "1.1.51"}, {"tag", "v1.1.51"}}}}}};
    assert(MayaUpdateCheckVerified(latestUpdate["update"]));
    assert(MayaUpdateCardText(latestUpdate).find("Up to date. Project Maya v1.1.51") != std::string::npos);
    assert(MayaUpdateStatusText(latestUpdate) == "Maya is up to date.");
    auto missingReleaseTag = latestUpdate["update"];
    missingReleaseTag["latest"].erase("tag");
    assert(!MayaUpdateCheckVerified(missingReleaseTag));
    const Json failedUpdate{{"update", {{"current", "1.1.50"}, {"state", {{"state", "failed"}, {"error", "compile failed"}}}}}};
    assert(MayaUpdateCardText(failedUpdate).find("The update did not happen: compile failed") != std::string::npos);
    const Json errorUpdate{{"ready", true}, {"dependency_ok", true}, {"update", {{"current", "1.1.50"}, {"newer", true}, {"can_update", true},
        {"latest", {{"version", "1.1.51"}, {"tag", "v1.1.51"}}}, {"error", "GitHub unavailable"}}}};
    assert(!MayaUpdateCheckVerified(errorUpdate["update"]));
    assert(!MayaUpdateCanStart(errorUpdate));
    assert(MayaUpdateCardText(errorUpdate).find("Couldn't check for updates. Project Maya v1.1.50") != std::string::npos);
    assert(MayaUpdateStatusText(errorUpdate) == "Couldn't verify the latest Maya release.");
    const Json inconsistentRelease{{"current", "1.1.50"}, {"newer", false}, {"latest", {{"version", "1.1.51"}, {"tag", "v1.1.51"}}}};
    assert(!MayaUpdateCheckVerified(inconsistentRelease));
    const Json disabledUpdates{{"update", {{"enabled", false}}}};
    assert(MayaUpdateCardText(disabledUpdates) == "Maya update checks are disabled (MAYA_UPDATE_CHECK=0).");
    auto blockedDirtyUpdate = updateCard;
    blockedDirtyUpdate["ready"] = false;
    blockedDirtyUpdate["update"]["can_update"] = false;
    blockedDirtyUpdate["update"]["blocker"] = "files in this folder were changed by hand (git status lists them)";
    blockedDirtyUpdate["update"]["by_hand"] = "git pull, then start Maya again (START-MAYA.bat)";
    const auto blockedDirtyText = MayaUpdatePreflightText(blockedDirtyUpdate);
    assert(blockedDirtyText.find("Update by hand: git pull, then start Maya again (START-MAYA.bat)") != std::string::npos &&
           blockedDirtyText.find("files in this folder were changed by hand") != std::string::npos);
    assert(!MayaUpdateCanStart(blockedDirtyUpdate) && MayaUpdateCardText(blockedDirtyUpdate).find("Blocked:") != std::string::npos);
    assert(MayaUpdatePreflightText(Json(nullptr)).find("Preflight is blocked.") != std::string::npos);
    auto offlineUpdate = updateCard;
    offlineUpdate["online"] = false;
    offlineUpdate["ready"] = false;
    offlineUpdate["update"]["blocker"] = "Maya is running or starting. Stop Maya before an offline upgrade.";
    const auto offlineText = MayaUpdatePreflightText(offlineUpdate);
    assert(offlineText.find("Maya API offline") != std::string::npos &&
           offlineText.find("Preflight is blocked.") != std::string::npos &&
           offlineText.find("Stop Maya before an offline upgrade.") != std::string::npos);
    auto offlineReady = updateCard;
    offlineReady["online"] = false;
    const auto offlineReadyText = MayaUpdatePreflightText(offlineReady);
    assert(MayaUpdateCanStart(offlineReady) &&
           offlineReadyText.find("Maya API offline") != std::string::npos &&
           offlineReadyText.find("Preflight passed.") != std::string::npos);
    const Json checkedRelease{{"latest", Json{{"tag", "v1.1.55"}}}};
    assert(MayaUpdateRecheckedTagMatches(checkedRelease, "v1.1.55"));
    assert(!MayaUpdateRecheckedTagMatches(checkedRelease, "v1.1.54"));
    assert(!MayaUpdateRecheckedTagMatches(Json{{"latest", Json::object()}}, "v1.1.55"));
    assert(!MayaUpdateRecheckedTagMatches(Json(nullptr), "v1.1.55"));
    assert(MayaUpdateCanFastForward("abc123\r\n", "abc123\r\n"));
    assert(!MayaUpdateCanFastForward("abc123\n", "def456\n"));
    assert(!MayaUpdateCanFastForward("", ""));
    assert(MayaPythonSelectorArgument(L"-3") == L" -3" && MayaPythonSelectorArgument(L"").empty());
    assert(MayaInstalledModelConfigName(L"maya-ds4.json"));
    assert(MayaInstalledModelConfigName(L"maya-glm-maya-s.json"));
    assert(MayaInstalledModelConfigName(L"MAYA-MM3.JSON"));
    assert(!MayaInstalledModelConfigName(L"maya-ds4.shared-settings.json"));
    assert(!MayaInstalledModelConfigName(L"maya-ds4.json.prev"));
    assert(!MayaInstalledModelConfigName(L"maya.json"));
    assert(!MayaInstalledModelConfigName(L"other-model.json"));
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", "deepseek"}}}}) == L"DeepSeek-V4.1-Flash-1017");
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", "minimax"}}}}) == L"MiniMax-M3.1-Flash");
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", "qwen"}}}}) == L"CYBER-FROST-3.8-PS");
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", "glm"}}}}).empty());
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", nullptr}}}, {"exe", "C:/Maya/maya-ds4.exe"}}) == L"DeepSeek-V4.1-Flash-1017");
    assert(MayaDependencyCheckModel(Json{{"installer", {{"family", 7}}}}).empty());
    assert(MayaDependencyCheckModel(Json{{"exe", "C:/Maya/maya-ds4.exe"}}) == L"DeepSeek-V4.1-Flash-1017");
    assert(MayaDependencyCheckModel(Json{{"args", {"--ple-gguf", "path/to/pack"}}}) == L"CYBER-FROST-3.8-PS");
    auto preservedUpdate = updateCard;
    preservedUpdate["update"]["preserve_tracked_changes"] = true;
    preservedUpdate["update"]["preserved_paths"] = 2;
    const auto preservedText = MayaUpdatePreflightText(preservedUpdate);
    assert(preservedText.find("2 locally changed files will be backed up and reapplied") != std::string::npos);
    const char updatePathFixture[] = "serve/web/models.js\0serve/web/turbo.js\0";
    assert(MayaUpdatePathList(std::string(updatePathFixture, sizeof(updatePathFixture) - 1)).size() == 2);
    assert(MayaUpdatePathConflict({"serve/web/models.js"}, {}, {"serve/web/chats.js"}).empty());
    assert(!MayaUpdatePathConflict({"serve/web/models.js"}, {}, {"SERVE\\WEB\\models.js"}).empty());
    assert(MayaUpdatePathConflict({}, {"win-manager/native/MayaNative.cpp"}, {"serve/web/app.js"}).empty());
    assert(!MayaUpdatePathConflict({}, {"win-manager/native/MayaNative.cpp"}, {"win-manager/native"}).empty());
    assert(MayaUpdatePathsCollide(u8"serve/web/Ä.md", u8"serve\\web\\ä.MD"));
    assert(MayaUpdatePathsCollide(u8"Ä", u8"ä\\child.txt"));
    assert(!MayaUpdatePathsCollide(u8"Ä", u8"äcar"));
    assert(MayaUpdatePathsCollide(std::string(1, static_cast<char>(0xFF)), "safe"));
    assert(!MayaUpdatePathConflict({}, {}, {std::string(1, static_cast<char>(0xFF))}).empty());
    assert(!MayaUpdatePathConflict({}, {"user-data"}, {"user-data\\new.txt"}).empty());
    const auto updatePathRoot = std::filesystem::temp_directory_path() /
        (L"MayaUpdatePathFixture-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(GetCurrentProcessId()));
    assert(!std::filesystem::exists(updatePathRoot));
    std::filesystem::create_directories(updatePathRoot);
    { std::ofstream occupied(updatePathRoot / L"occupied.txt"); occupied << "preserve"; }
    std::filesystem::create_directories(updatePathRoot / L"occupied-dir");
    assert(!MayaUpdateExistingPathConflict(updatePathRoot, {"occupied.txt"}).empty());
    assert(!MayaUpdateExistingPathConflict(updatePathRoot, {"occupied-dir"}).empty());
    assert(MayaUpdateExistingPathConflict(updatePathRoot, {"new-file.txt"}).empty());
    const Json freshReady{{"mode", "fresh"}, {"installed", false}, {"ready", true}, {"dependency_ok", true},
                          {"dependency_report", "Python and build prerequisites passed."}};
    const auto freshText = MayaFreshInstallPreflightText(freshReady);
    assert(MayaFreshInstallCanStart(freshReady) && freshText.find("preflight passed") != std::string::npos &&
           freshText.find("Python and build prerequisites passed.") != std::string::npos);
    assert(MayaUpdateCanStart(freshReady) && MayaUpdateCardText(freshReady).find("ready to install") != std::string::npos);
    auto freshBlocked = freshReady;
    freshBlocked["ready"] = false;
    assert(!MayaFreshInstallCanStart(freshBlocked) &&
           MayaFreshInstallPreflightText(freshBlocked).find("preflight is blocked") != std::string::npos);
    assert(!MayaUpdateCanStart(freshBlocked) && MayaUpdateCardText(freshBlocked).find("Setup is blocked") != std::string::npos);
    auto freshInstalled = freshReady;
    freshInstalled["installed"] = true;
    assert(!MayaFreshInstallCanStart(freshInstalled));
    auto malformedFresh = freshReady;
    malformedFresh["ready"] = "true";
    assert(!MayaFreshInstallCanStart(malformedFresh));
    assert(MayaUpdateWatchStep(true, "1.1.51", "1.1.51", false, "ready") == MayaUpdateWatchState::COMPLETE);
    assert(MayaUpdateWatchStep(false, "", "1.1.51", true, "") == MayaUpdateWatchState::WAITING);
    assert(MayaUpdateWatchStep(true, "1.1.50", "1.1.51", false, "updating") == MayaUpdateWatchState::WAITING);
    assert(MayaUpdateWatchStep(true, "1.1.50", "1.1.51", true, "ready") == MayaUpdateWatchState::FAILED);
    assert(MayaUpdateWatchStep(true, "1.1.50", "1.1.51", true, "updating") == MayaUpdateWatchState::WAITING);
    assert(MayaUpdateBadgeVisible(true, false));
    assert(!MayaUpdateBadgeVisible(false, false) && !MayaUpdateBadgeVisible(true, true));
    assert(MayaUpdateCheckEligible(false, false, false, false));
    assert(!MayaUpdateCheckEligible(true, false, false, false) &&
           !MayaUpdateCheckEligible(false, true, false, false) &&
           !MayaUpdateCheckEligible(false, false, true, false) &&
           !MayaUpdateCheckEligible(false, false, false, true));
    assert(std::wstring(MayaUpdateEndpointPath(false, false)) == L"/api/update");
    assert(std::wstring(MayaUpdateEndpointPath(false, true)) == L"/api/update?force=1");
    assert(std::wstring(MayaUpdateEndpointPath(true, true)) == L"/api/update");
    assert(MayaUpdateRecoveryTarget(Json{{"update", {{"state", {{"state", "updating"}, {"to", "1.1.51"}}}}}}) == "1.1.51");
    assert(MayaUpdateRecoveryTarget(Json{{"update", {{"state", {{"state", "failed"}, {"to", "1.1.51"}}}}}}).empty());
    assert(MayaUpdateRecoveryError(Json{{"update", {{"state", {{"state", "failed"}, {"error", "compile failed"}}}}}}) == "compile failed");
    assert(MayaUpdateNotesUrl(updateCard) == "https://github.com/mw00/project-maya/releases/tag/v1.1.51");
    const Json unsafeUpdate{{"update", {{"latest", {{"url", "https://github.com.example.invalid/releases"}}}}}};
    assert(MayaUpdateNotesUrl(unsafeUpdate).empty());
    assert(ModelSwitchRestarts(Json{{"to", "model-a"}, {"restarts", true}}));
    assert(!ModelSwitchRestarts(Json{{"to", "model-b"}, {"restarts", false}}));
    const auto restartSwitchPrompt = ModelSwitchConfirmationText(
        Json{{"model", "deepseek-v4.1-flash"}, {"quant", "Q4"}, {"file", "deepseek.json"}},
        "GLM Q5", true, true);
    assert(restartSwitchPrompt.find("stops GLM Q5") != std::string::npos &&
           restartSwitchPrompt.find("deepseek.json") != std::string::npos &&
           restartSwitchPrompt.find("Your chats stay") != std::string::npos &&
           restartSwitchPrompt.find("standard parameters and thinking level") != std::string::npos &&
           restartSwitchPrompt.find("deepseek-v4.1-flash") != std::string::npos &&
           restartSwitchPrompt.find("answer being written finishes first") != std::string::npos);
    const auto deferredSwitchPrompt = ModelSwitchConfirmationText(
        Json{{"model", "qwen"}, {"quant", "Q4"}}, "", false, false);
    assert(deferredSwitchPrompt.find("at next start") != std::string::npos &&
           deferredSwitchPrompt.find("cannot restart itself") != std::string::npos);
    assert(ModelSwitchEnabled({{"ok", true}, {"running", false}}, false, false));
    assert(!ModelSwitchEnabled({{"ok", false}, {"running", false}}, false, false));
    assert(!ModelSwitchEnabled({{"ok", true}, {"running", true}}, false, false));
    assert(!ModelSwitchEnabled({{"ok", true}, {"running", false}}, true, false));
    assert(!ModelSwitchEnabled({{"ok", true}, {"running", false}}, false, true));
    assert(ModelSwitchWatchStep(false, true, true, false) == ModelSwitchWatchState::WAITING);
    assert(ModelSwitchWatchStep(true, false, false, false) == ModelSwitchWatchState::WAITING);
    assert(ModelSwitchWatchStep(true, true, false, false) == ModelSwitchWatchState::WAITING);
    assert(ModelSwitchWatchStep(true, true, true, false) == ModelSwitchWatchState::COMPLETE);
    assert(ModelSwitchWatchStep(true, false, false, true) == ModelSwitchWatchState::TIMED_OUT);
    const Json attachmentHealth{{"images", true}, {"videos", true}, {"max_text_file", 1000}};
    const Json missingAttachmentLimit = Json::object();
    const Json zeroAttachmentLimit{{"max_text_file", 0}};
    const Json nullAttachmentLimit{{"max_text_file", nullptr}};
    assert(MayaTextFileLimit(attachmentHealth) == 1000);
    assert(MayaTextFileLimit(missingAttachmentLimit) == 512 * 1024);
    assert(MayaTextFileLimit(zeroAttachmentLimit) == 512 * 1024);
    assert(MayaTextFileLimit(nullAttachmentLimit) == 512 * 1024);
    assert(MayaAttachmentSizeError(MayaAttachmentKind::TEXT, 512 * 1024, nullAttachmentLimit).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::TEXT, 512 * 1024 + 1, nullAttachmentLimit).empty());
    assert(MayaAttachmentType(std::filesystem::path(L"scan.PNG")) == MayaAttachmentKind::IMAGE);
    assert(MayaAttachmentType(std::filesystem::path(L"scan.webp")) == MayaAttachmentKind::IMAGE);
    assert(MayaAttachmentMimeType(std::filesystem::path(L"scan.PNG")) == L"image/png");
    assert(MayaAttachmentMimeType(std::filesystem::path(L"photo.jpeg")) == L"image/jpeg");
    assert(MayaAttachmentMimeType(std::filesystem::path(L"scan.tiff")) == L"image/tiff");
    const auto imageUrl = MayaAttachmentDataUrl(std::string("\x01\x02", 2), L"image/png");
    assert(imageUrl == "data:image/png;base64,AQI=");
    const auto imagePart = MayaMediaContentPart(MayaAttachmentKind::IMAGE, imageUrl);
    const auto encodedImageContent = MayaUserMessageContent("Inspect this", Json::array({imagePart}));
    assert(encodedImageContent.is_array() && encodedImageContent.size() == 2 &&
           encodedImageContent[0].value("text", std::string()) == "Inspect this" &&
           encodedImageContent[1]["image_url"].value("url", std::string()) == imageUrl);
    assert(MayaAttachmentType(std::filesystem::path(L"recording.mp4")) == MayaAttachmentKind::VIDEO);
    assert(MayaAttachmentType(std::filesystem::path(L"recording.MKV")) == MayaAttachmentKind::VIDEO);
    assert(MayaVideoSeekPosition(10000000, 5000) == 5000000);
    assert(MayaVideoSeekPosition(10000000, -1) == 0);
    assert(MayaVideoSeekPosition(10000000, 20000) == 10000000);
    assert(MayaVideoSeekSlider(10000000, 2500000) == 2500);
    assert(MayaVideoSeekSlider(0, 2500000) == 0);
    assert(MayaInlineVideoVisible(4, 24, 420, 260, 800, 600));
    assert(MayaInlineVideoVisible(4, -200, 420, 260, 800, 600));
    assert(!MayaInlineVideoVisible(4, -300, 420, 260, 800, 600));
    assert(!MayaInlineVideoVisible(900, 24, 420, 260, 800, 600));
    assert(!MayaInlineVideoVisible(4, 24, 0, 260, 800, 600));
    assert(std::wstring(MayaVideoEventStatus(MFP_EVENT_TYPE_MEDIAITEM_SET)) == L"Video ready.");
    assert(std::wstring(MayaVideoEventStatus(MFP_EVENT_TYPE_PLAY)) == L"Playing video.");
    assert(std::wstring(MayaVideoEventStatus(MFP_EVENT_TYPE_PAUSE)) == L"Video paused.");
    assert(std::wstring(MayaVideoEventStatus(MFP_EVENT_TYPE_PLAYBACK_ENDED)) == L"Playback ended.");
    assert(MayaVideoEventStatus(MFP_EVENT_TYPE_ACQUIRE_USER_CREDENTIAL) == nullptr);
    assert(MayaAttachmentMimeType(std::filesystem::path(L"recording.mp4")) == L"video/mp4");
    assert(MayaAttachmentMimeType(std::filesystem::path(L"recording.mov")) == L"video/quicktime");
    const auto videoUrl = MayaAttachmentDataUrl(std::string("\x01\x02", 2), L"video/mp4");
    assert(videoUrl == "data:video/mp4;base64,AQI=");
    const auto videoPart = MayaMediaContentPart(MayaAttachmentKind::VIDEO, videoUrl);
    assert((videoPart == Json{{"type", "video_url"}, {"video_url", {{"url", videoUrl}}}}));
    const auto encodedVideoContent = MayaUserMessageContent("Describe this", Json::array({videoPart}));
    assert(encodedVideoContent.is_array() && encodedVideoContent[1]["type"] == "video_url" &&
           encodedVideoContent[1]["video_url"]["url"] == videoUrl);
    assert(MayaAttachmentType(std::filesystem::path(L"notes.md")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L"Makefile")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L"build.dockerfile")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L".dockerfile")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L".env")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L".gitignore")) == MayaAttachmentKind::TEXT);
    assert(MayaAttachmentType(std::filesystem::path(L"report.pdf")) == MayaAttachmentKind::SERVER);
    assert(MayaAttachmentType(std::filesystem::path(L"report.docx")) == MayaAttachmentKind::SERVER);
    assert(MayaAttachmentType(std::filesystem::path(L"payload.exe")) == MayaAttachmentKind::SERVER);
    assert(MayaAttachmentType(std::filesystem::path(L"song.mp3")) == MayaAttachmentKind::UNSUPPORTED);
    assert(MayaAttachmentHeaderFilename(L"report one-ă.txt") == "report%20one-%C4%83.txt");
    assert(MayaAttachmentReadLimit(MayaAttachmentKind::SERVER, attachmentHealth) == 64ULL * 1024 * 1024);
    assert(MayaAttachmentSizeError(MayaAttachmentKind::SERVER, 64ULL * 1024 * 1024, attachmentHealth).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::SERVER, 64ULL * 1024 * 1024 + 1, attachmentHealth).empty());
    std::string extractedText = "Question";
    Json extractedImages = Json::array();
    MayaAppendServerAttachment("notes.pdf", {{"kind", "text"}, {"text", "hello"}}, extractedText, extractedImages);
    assert(extractedText == "Question\n\nFile: notes.pdf\n```\nhello\n```");
    const Json scannedPdf = {{"kind", "images"}, {"images", Json::array({Json{{"data", "data:image/jpeg;base64,AA=="}}})}};
    MayaAppendServerAttachment("scan.pdf", scannedPdf, extractedText, extractedImages);
    assert(extractedImages.size() == 1 && extractedImages[0]["image_url"]["url"] == "data:image/jpeg;base64,AA==");
    bool unreadableAttachment = false;
    try
    {
        MayaAppendServerAttachment("archive.zip", {{"error", {{"message", "attach the files inside it"}}}}, extractedText, extractedImages);
    }
    catch (const std::runtime_error &e)
    {
        unreadableAttachment = std::string(e.what()).find("attach the files inside it") != std::string::npos;
    }
    assert(unreadableAttachment);
    const std::vector<std::filesystem::path> clipboardFiles{L"scan.png", L"notes.md", L"unknown.bin"};
    assert(MayaClipboardImages(clipboardFiles, true) == std::vector<std::filesystem::path>{L"scan.png"});
    assert(MayaClipboardImages(clipboardFiles, false).empty());
    const wchar_t *webTextExtensions[] = {L".txt", L".md", L".markdown", L".rst", L".tex", L".py", L".pyi",
        L".ipynb", L".js", L".mjs", L".cjs", L".ts", L".tsx", L".jsx", L".vue", L".svelte", L".json",
        L".jsonl", L".csv", L".tsv", L".log", L".yml", L".yaml", L".toml", L".ini", L".cfg", L".conf",
        L".env", L".xml", L".htm", L".html", L".css", L".scss", L".less", L".c", L".cc", L".cpp", L".cxx",
        L".h", L".hh", L".hpp", L".cu", L".cuh", L".rs", L".go", L".java", L".kt", L".kts", L".swift",
        L".rb", L".php", L".pl", L".lua", L".r", L".jl", L".scala", L".sql", L".sh", L".bash", L".zsh",
        L".fish", L".ps1", L".psm1", L".bat", L".cmd", L".diff", L".patch", L".gradle", L".cmake", L".mk",
        L".dockerfile", L".gitignore", L".proto", L".graphql"};
    for (const auto *extension : webTextExtensions)
        assert(MayaAttachmentType(std::filesystem::path(std::wstring(L"file") + extension)) == MayaAttachmentKind::TEXT);
    for (const auto *name : {L"Makefile", L"Dockerfile", L"README", L"LICENSE"})
        assert(MayaAttachmentType(std::filesystem::path(name)) == MayaAttachmentKind::TEXT);
    assert(MayaTextFileBlock("notes.md", "hello") == "File: notes.md\n```\nhello\n```");
    assert(MayaTextFileBlock("code.txt", "`one`\n```") == "File: code.txt\n````\n`one`\n```\n````");
    const auto textOnlyContent = MayaUserMessageContent("question\n\nFile: notes.md\n```\nhello\n```", Json::array());
    assert(textOnlyContent.is_string() && textOnlyContent.get<std::string>().find("File: notes.md") != std::string::npos);
    const Json placeholderImagePart{{"type", "image_url"}, {"image_url", {{"url", "data:,"}}}};
    const auto imageContent = MayaUserMessageContent("", Json::array({placeholderImagePart}));
    assert(imageContent.is_array() && imageContent.size() == 2 && imageContent[0]["type"] == "text" &&
           imageContent[0]["text"] == "" && imageContent[1] == placeholderImagePart);
    const auto mixedContent = MayaUserMessageContent("question plus file", Json::array({placeholderImagePart}));
    assert(mixedContent.is_array() && mixedContent[0]["text"] == "question plus file");
    assert(MayaDecodeTextBytes(std::string("\xEF\xBB\xBFhello")) == "hello");
    assert(MayaDecodeTextBytes(std::string({'\xFF', '\xFE', 'h', '\0', 'i', '\0'})) == "hi");
    assert(MayaDecodeTextBytes(std::string({'\xFE', '\xFF', '\0', 'h', '\0', 'i'})) == "hi");
    assert(MayaDecodeTextBytes(std::string({'\xFF'})) == "\xEF\xBF\xBD");
    assert(MayaDecodeTextBytes(std::string({'\xFF', '\xFE', '\0', '\0'})).find('\0') != std::string::npos);
    std::string attachmentSnapshot;
    assert(MayaAttachmentData({}, {"captured at selection"}, 0, 64, attachmentSnapshot));
    assert(attachmentSnapshot == "captured at selection");
    assert(!MayaAttachmentData({}, {"oversized snapshot"}, 0, 4, attachmentSnapshot));
    assert(!MayaAttachmentData({}, {}, 0, 64, attachmentSnapshot));
    assert(MayaAttachmentFileSizeAllowed(1000, 1000));
    assert(!MayaAttachmentFileSizeAllowed(1001, 1000));
    const std::filesystem::path fixtureSource(__FILE__);
    uint64_t fixtureSize = 0;
    assert(MayaAttachmentFileSize(fixtureSource, fixtureSize) && fixtureSize > 0);
    assert(MayaReadFileBytes(fixtureSource, fixtureSize, attachmentSnapshot));
    assert(attachmentSnapshot.size() == fixtureSize);
    assert(!MayaReadFileBytes(fixtureSource, fixtureSize - 1, attachmentSnapshot));
    assert(attachmentSnapshot.empty());
    assert(MayaAttachmentReadLimit(MayaAttachmentKind::IMAGE, attachmentHealth) == 20000000ULL);
    assert(MayaAttachmentReadLimit(MayaAttachmentKind::VIDEO, attachmentHealth) == 100ULL * 1024 * 1024);
    assert(MayaAttachmentReadLimit(MayaAttachmentKind::TEXT, attachmentHealth) == 1000);
    assert(MayaAttachmentReadLimit(MayaAttachmentKind::UNSUPPORTED, attachmentHealth) == 0);
    assert(MayaAttachmentHasNul(std::string("a\0b", 3)));
    assert(!MayaAttachmentHasNul(std::string("abc")));
    assert(MayaAttachmentSizeError(MayaAttachmentKind::IMAGE, 20000000, attachmentHealth).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::IMAGE, 20000001, attachmentHealth).empty());
    assert(MayaAttachmentSizeError(MayaAttachmentKind::VIDEO, 100ULL * 1024 * 1024, attachmentHealth).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::VIDEO, 100ULL * 1024 * 1024 + 1, attachmentHealth).empty());
    assert((MayaInlineThumbnailSize(320, 240) == std::pair<unsigned, unsigned>{112, 84}));
    assert((MayaInlineThumbnailSize(400, 100) == std::pair<unsigned, unsigned>{144, 36}));
    assert((MayaInlineThumbnailSize(80, 80) == std::pair<unsigned, unsigned>{80, 80}));
    assert((MayaInlineThumbnailSize(0, 100) == std::pair<unsigned, unsigned>{0, 0}));
    assert(MayaAttachmentSizeError(MayaAttachmentKind::TEXT, 1000, attachmentHealth).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::TEXT, 1001, attachmentHealth).empty());
    const Json noVision{{"images", false}, {"videos", false}, {"max_text_file", 1000}};
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::IMAGE, 1, noVision).empty());
    assert(!MayaAttachmentSizeError(MayaAttachmentKind::VIDEO, 1, noVision).empty());
    assert(!MayaChatDropTarget(199, 120, 200, 120));
    assert(!MayaChatDropTarget(200, 119, 200, 120));
    assert(MayaChatDropTarget(200, 120, 200, 120));
    assert(MayaChatDropTarget(600, 400, 200, 120));
    const auto singleFiles = MayaParseFileSelection(L"C:\\QA\\one.png");
    assert(singleFiles.size() == 1 && singleFiles[0] == std::filesystem::path(L"C:\\QA\\one.png"));
    const wchar_t multiSelection[] = L"C:\\QA\0one.png\0two.md\0";
    const auto multipleFiles = MayaParseFileSelection(multiSelection);
    assert(multipleFiles.size() == 2 && multipleFiles[0] == std::filesystem::path(L"C:\\QA\\one.png") &&
           multipleFiles[1] == std::filesystem::path(L"C:\\QA\\two.md"));
    assert(MayaStoredAttachmentPath(L"C:\\Data", L"chat-1", L"copy-1", L"photo.png") ==
           std::filesystem::path(L"C:\\Data\\attachments\\chat-1\\copy-1\\photo.png"));
    const auto chatAttachmentRoot = std::filesystem::path(L"C:\\Data\\attachments\\chat-1");
    const auto ownedAttachment = MayaStoredAttachmentPath(L"C:\\Data", L"chat-1", L"copy-1", L"photo.png");
    assert(MayaPathIsWithin(chatAttachmentRoot, ownedAttachment));
    assert(!MayaPathIsWithin(chatAttachmentRoot, L"C:\\Data\\attachments\\chat-10\\copy\\photo.png"));
    const auto turbo = NativeTurboEnter(Json{{"deepseek", {{"temperature", 0.65}, {"top_k", 17}}}},
                                        "deepseek", "max", {"none", "low", "high", "max"});
    assert(NativeTurboInfoEligible(false, false) && !NativeTurboInfoEligible(true, false) &&
           !NativeTurboInfoEligible(false, true));
    assert(turbo["sampling"]["deepseek"]["temperature"] == 0 &&
           turbo["sampling"]["deepseek"]["reasoning_effort"] == "none");
    std::string restoredThinking;
    const auto restored = NativeTurboLeave(turbo["sampling"], turbo["snapshot"], "none", restoredThinking);
    assert(restored["deepseek"]["temperature"] == 0.65 && restored["deepseek"]["top_k"] == 17 &&
           restoredThinking == "max");
    const auto turboEmpty = NativeTurboEnter(Json::object(), "qwen", "xhigh", {"low", "medium", "xhigh"});
    const auto restoredEmpty = NativeTurboLeave(turboEmpty["sampling"], turboEmpty["snapshot"], "medium", restoredThinking);
    assert(!restoredEmpty.contains("qwen") && restoredThinking == "medium");
    assert(PullPercent(0, 100) == 0);
    assert(PullPercent(50, 100) == 50);
    assert(PullPercent(99, 100) == 99);
    assert(PullPercent(100, 100) == 100);
    assert(PullPercent(200, 100) == 100);
    assert(PullPercent(50, 0) == 0);
    assert(PullPercent(-1, 100) == 0);
    assert(ChatScrollAtBottom(0, 99, 90, 10));
    assert(!ChatScrollAtBottom(0, 99, 89, 10));
    assert(ChatScrollAtBottom(0, 0, 0, 0));
    assert(ChatScrollAtBottom(0, 99, 99, 0));
    const auto mdSpans = ChatMarkdownSpans(L"# Heading\n**bold** and `code`\n> quote\n```cpp\nx = 1;\n```");
    assert(mdSpans.size() >= 6);
    assert(mdSpans[0].style == ChatMarkdownStyle::TEXT && mdSpans[0].headingStyle == ChatMarkdownStyle::HEADING &&
           mdSpans[0].text == L"Heading\r\n");
    assert(mdSpans[1].style == ChatMarkdownStyle::EMPHASIS && mdSpans[1].text == L"bold");
    const auto inlineHeading = ChatMarkdownSpans(L"## **bold** *italic* ~~strike~~ `code` [site](https://example.com)");
    assert(std::any_of(inlineHeading.begin(), inlineHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.style == ChatMarkdownStyle::EMPHASIS && span.text == L"bold";
    }));
    assert(std::any_of(inlineHeading.begin(), inlineHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.style == ChatMarkdownStyle::ITALIC && span.text == L"italic";
    }));
    assert(std::any_of(inlineHeading.begin(), inlineHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.style == ChatMarkdownStyle::STRIKETHROUGH && span.text == L"strike";
    }));
    assert(std::any_of(inlineHeading.begin(), inlineHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.style == ChatMarkdownStyle::CODE && span.text == L"code";
    }));
    assert(std::any_of(inlineHeading.begin(), inlineHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.style == ChatMarkdownStyle::LINK &&
               span.text == L"site" && span.target == L"https://example.com";
    }));
    const auto levelTwoHeading = ChatMarkdownSpans(L"## Section heading");
    assert(levelTwoHeading.size() == 1 && levelTwoHeading[0].headingStyle == ChatMarkdownStyle::HEADING);
    const auto levelThreeHeading = ChatMarkdownSpans(L"### Subsection heading");
    assert(levelThreeHeading.size() == 1 && levelThreeHeading[0].headingStyle == ChatMarkdownStyle::SUBHEADING);
    const auto levelSixHeading = ChatMarkdownSpans(L"###### Deep heading");
    assert(levelSixHeading.size() == 1 && levelSixHeading[0].headingStyle == ChatMarkdownStyle::SUBHEADING);
    const auto italic = ChatMarkdownSpans(L"*slanted*");
    assert(italic.size() >= 1 && italic[0].style == ChatMarkdownStyle::ITALIC && italic[0].text == L"slanted");
    const auto boundaryItalic = ChatMarkdownSpans(L"word *slanted* end");
    assert(std::any_of(boundaryItalic.begin(), boundaryItalic.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::ITALIC && span.text == L"slanted";
    }));
    const auto intrawordAsterisks = ChatMarkdownSpans(L"word*not italic*ending");
    assert(std::none_of(intrawordAsterisks.begin(), intrawordAsterisks.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::TEXT && span.text.find(L"****") != std::wstring::npos;
    }));
    const auto tabHeading = ChatMarkdownSpans(L"#\tTabbed heading");
    assert(!tabHeading.empty() && tabHeading[0].headingStyle == ChatMarkdownStyle::HEADING &&
           tabHeading[0].text == L"Tabbed heading\r\n");
    const auto unicodeWhitespaceHeading = ChatMarkdownSpans(L"##\u00A0\uFEFFUnicode heading");
    assert(std::any_of(unicodeWhitespaceHeading.begin(), unicodeWhitespaceHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING && span.text == L"Unicode heading\r\n";
    }));
    const auto indentedHeading = ChatMarkdownSpans(L"  # not a heading");
    assert(std::none_of(indentedHeading.begin(), indentedHeading.end(), [](const auto &span) {
        return span.headingStyle == ChatMarkdownStyle::HEADING || span.headingStyle == ChatMarkdownStyle::SUBHEADING;
    }));
    const auto struck = ChatMarkdownSpans(L"before ~~deleted~~ after");
    assert(std::any_of(struck.begin(), struck.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::STRIKETHROUGH && span.text == L"deleted";
    }));
    assert(std::any_of(mdSpans.begin(), mdSpans.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == L"1";
    }));
    const auto blockquoteSpacing = ChatMarkdownSpans(L">\tquoted\n>\u00A0also\n>  twice\n  > indented\n");
    std::vector<int> quoteSpaces;
    for (const auto &span : blockquoteSpacing)
        if (span.blockQuote && span.style == ChatMarkdownStyle::TEXT) quoteSpaces.push_back(span.paragraphSpaceBeforeTwips);
    assert((quoteSpaces == std::vector<int>{0, 180, 180}));
    std::vector<std::wstring> quotedBlocks;
    for (const auto &span : blockquoteSpacing)
        if (span.blockQuote && span.style == ChatMarkdownStyle::TEXT) quotedBlocks.push_back(span.text);
    assert((quotedBlocks == std::vector<std::wstring>{L"quoted", L"also", L" twice"}));
    const auto styledQuote = ChatMarkdownSpans(L"> **bold** *italic* ~~struck~~ `code` [site](https://example.com)\n");
    assert(std::all_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) { return span.blockQuote; }));
    assert(std::any_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) {
        return span.blockQuote && span.style == ChatMarkdownStyle::EMPHASIS && span.text == L"bold";
    }));
    assert(std::any_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) {
        return span.blockQuote && span.style == ChatMarkdownStyle::ITALIC && span.text == L"italic";
    }));
    assert(std::any_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) {
        return span.blockQuote && span.style == ChatMarkdownStyle::STRIKETHROUGH && span.text == L"struck";
    }));
    assert(std::any_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) {
        return span.blockQuote && span.style == ChatMarkdownStyle::CODE && span.text == L"code";
    }));
    assert(std::any_of(styledQuote.begin(), styledQuote.end(), [](const auto &span) {
        return span.blockQuote && span.style == ChatMarkdownStyle::LINK && span.text == L"site" && span.target == L"https://example.com";
    }));
    const auto py = ChatSyntaxSpans(L"def run(): # note\n    print('ok')", L"python");
    assert(std::any_of(py.begin(), py.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"def";
    }));
    assert(std::any_of(py.begin(), py.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT && span.text == L"# note";
    }));
    const auto pyLiterals = ChatSyntaxSpans(L"True False None true false none", L"python");
    for (const auto *literal : {L"True", L"False", L"None"})
        assert(std::any_of(pyLiterals.begin(), pyLiterals.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == literal;
        }));
    for (const auto *literal : {L"true", L"false", L"none"})
        assert(std::none_of(pyLiterals.begin(), pyLiterals.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == literal;
        }));
    const auto jsonKeys = ChatSyntaxSpans(L"{\"name\": \"Maya\", \"count\": 2}", L"json");
    assert(std::any_of(jsonKeys.begin(), jsonKeys.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_ATTRIBUTE && span.text == L"\"name\"";
    }));
    assert(std::any_of(jsonKeys.begin(), jsonKeys.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"\"Maya\"";
    }));
    const auto sqlExtras = ChatSyntaxSpans(L"RETURNING count", L"sql");
    for (const auto *keyword : {L"RETURNING", L"count"})
        assert(std::any_of(sqlExtras.begin(), sqlExtras.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == keyword;
        }));
    const auto jsTemplate = ChatSyntaxSpans(L"`value ${x}\nnext`", L"js");
    assert(std::any_of(jsTemplate.begin(), jsTemplate.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"`value ${x}\nnext`";
    }));
    const auto jsEscapedNewline = ChatSyntaxSpans(L"\"first\\\nsecond\"", L"js");
    assert(std::any_of(jsEscapedNewline.begin(), jsEscapedNewline.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"\"first\\\nsecond\"";
    }));
    const auto jsRawNewline = ChatSyntaxSpans(L"\"first\nsecond\"", L"js");
    assert(std::none_of(jsRawNewline.begin(), jsRawNewline.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text.find(L'\n') != std::wstring::npos;
    }));
    const auto pyTriple = ChatSyntaxSpans(L"'''first\nsecond'''", L"python");
    assert(std::any_of(pyTriple.begin(), pyTriple.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"'''first\nsecond'''";
    }));
    const auto pyPrefixedTriple = ChatSyntaxSpans(L"rf'''first\nsecond'''", L"python");
    assert(std::any_of(pyPrefixedTriple.begin(), pyPrefixedTriple.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"rf'''first\nsecond'''";
    }));
    const auto pyRaw = ChatSyntaxSpans(L"r\"raw\"", L"python");
    assert(std::any_of(pyRaw.begin(), pyRaw.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING && span.text == L"r\"raw\"";
    }));
    const auto jsNumbers = ChatSyntaxSpans(L"-1 1e-3 0xCAFEu 0b10_01", L"js");
    assert(std::none_of(jsNumbers.begin(), jsNumbers.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == L"-1";
    }));
    for (const auto &number : {L"1", L"1e-3", L"0xCAFEu", L"0b10_01"})
        assert(std::any_of(jsNumbers.begin(), jsNumbers.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == number;
        }));
    const auto jsonNumbers = ChatSyntaxSpans(L"-12.5e+2", L"json");
    assert(jsonNumbers.size() == 1 && jsonNumbers.front().style == ChatMarkdownStyle::CODE_NUMBER &&
           jsonNumbers.front().text == L"-12.5e+2");
    const auto cssNumbers = ChatSyntaxSpans(L"-1.5rem 12px 33%", L"css");
    for (const auto &number : {L"-1.5rem", L"12px", L"33%"})
        assert(std::any_of(cssNumbers.begin(), cssNumbers.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == number;
        }));
    const auto cssSyntax = ChatSyntaxSpans(L"/* note */ @supports (color: #abc) { .card { color: #1a2b3c; width: 12vh; } }", L"css");
    assert(std::any_of(cssSyntax.begin(), cssSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT && span.text == L"/* note */";
    }));
    assert(std::any_of(cssSyntax.begin(), cssSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"@supports";
    }));
    assert(std::any_of(cssSyntax.begin(), cssSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_ATTRIBUTE && span.text == L"color";
    }));
    for (const auto &token : {L"#abc", L"#1a2b3c"})
        assert(std::any_of(cssSyntax.begin(), cssSyntax.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == token;
        }));
    assert(std::any_of(cssSyntax.begin(), cssSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == L"12vh";
    }));
    const auto yamlSyntax = ChatSyntaxSpans(L"service.name: true\nanswer=off\nupper: TRUE", L"yaml");
    assert(std::any_of(yamlSyntax.begin(), yamlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_ATTRIBUTE && span.text == L"service.name";
    }));
    const auto numericYamlKeys = ChatSyntaxSpans(L"123: item\n1.2\t= value\nanswer: 5", L"yaml");
    assert(std::any_of(numericYamlKeys.begin(), numericYamlKeys.end(), [](const auto &span) {
        return span.text == L"123" && span.style == ChatMarkdownStyle::CODE_ATTRIBUTE;
    }));
    assert(std::any_of(numericYamlKeys.begin(), numericYamlKeys.end(), [](const auto &span) {
        return span.text == L"1.2" && span.style == ChatMarkdownStyle::CODE_ATTRIBUTE;
    }));
    assert(std::any_of(numericYamlKeys.begin(), numericYamlKeys.end(), [](const auto &span) {
        return span.text == L"5" && span.style == ChatMarkdownStyle::CODE_NUMBER;
    }));
    for (const auto *literal : {L"true", L"off"})
        assert(std::any_of(yamlSyntax.begin(), yamlSyntax.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == literal;
        }));
    assert(std::none_of(yamlSyntax.begin(), yamlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"TRUE";
    }));
    const auto pythonSlash = ChatSyntaxSpans(L"marker // text", L"python");
    assert(std::none_of(pythonSlash.begin(), pythonSlash.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT;
    }));
    const auto jsonUpper = ChatSyntaxSpans(L"TRUE false", L"json");
    assert(std::none_of(jsonUpper.begin(), jsonUpper.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"TRUE";
    }));
    const auto pyDecorators = ChatSyntaxSpans(L"@property @package.module", L"python");
    for (const auto *decorator : {L"@property", L"@package.module"})
        assert(std::any_of(pyDecorators.begin(), pyDecorators.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_FUNCTION && span.text == decorator;
        }));
    const auto pyNewKeywords = ChatSyntaxSpans(L"case match cls", L"python");
    for (const auto *keyword : {L"case", L"match", L"cls"})
        assert(std::any_of(pyNewKeywords.begin(), pyNewKeywords.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == keyword;
        }));
    const auto cNewKeywords = ChatSyntaxSpans(L"constexpr readonly Self __global__ usize", L"cpp");
    for (const auto *keyword : {L"constexpr", L"readonly", L"Self", L"__global__", L"usize"})
        assert(std::any_of(cNewKeywords.begin(), cNewKeywords.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == keyword;
        }));
    const auto shellVariables = ChatSyntaxSpans(L"echo $HOME ${USER} $? $-", L"sh");
    for (const auto *variable : {L"$HOME", L"${USER}", L"$?", L"$-"})
        assert(std::any_of(shellVariables.begin(), shellVariables.end(), [&](const auto &span) {
            return span.style == ChatMarkdownStyle::CODE_VARIABLE && span.text == variable;
        }));
    const auto shellFunction = ChatSyntaxSpans(L"my-command() plain ()", L"sh");
    assert(std::any_of(shellFunction.begin(), shellFunction.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_FUNCTION && span.text == L"my-command";
    }));
    assert(std::none_of(shellFunction.begin(), shellFunction.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_FUNCTION && span.text == L"plain";
    }));
    const auto pyBacktick = ChatSyntaxSpans(L"`plain`", L"python");
    assert(std::none_of(pyBacktick.begin(), pyBacktick.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_STRING;
    }));
    const std::vector<std::wstring> codeAliases = {L"mjs", L"cjs", L"cc", L"cxx", L"cuh", L"golang"};
    for (const auto &alias : codeAliases)
    {
        const auto spans = ChatSyntaxSpans(L"return", alias);
        assert(!spans.empty() && spans.front().style == ChatMarkdownStyle::CODE_KEYWORD);
    }
    const auto cppDirectives = ChatSyntaxSpans(L"#include <vector>\n# define MAX 4", L"cpp");
    assert(std::any_of(cppDirectives.begin(), cppDirectives.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"#include";
    }));
    assert(std::any_of(cppDirectives.begin(), cppDirectives.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"# define";
    }));
    const auto consoleComment = ChatSyntaxSpans(L"# note", L"console");
    assert(!consoleComment.empty() && consoleComment.front().style == ChatMarkdownStyle::CODE_COMMENT);
    const auto shellHash = ChatSyntaxSpans(L"echo foo#bar\n echo # note", L"sh");
    assert(std::none_of(shellHash.begin(), shellHash.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT && span.text.find(L"foo#bar") != std::wstring::npos;
    }));
    assert(std::any_of(shellHash.begin(), shellHash.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT && span.text == L"# note";
    }));
    const auto htmlSyntax = ChatSyntaxSpans(L"<!-- note --><img data-id='3' class=hero>&copy;", L"html");
    assert(std::any_of(htmlSyntax.begin(), htmlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_COMMENT && span.text == L"<!-- note -->";
    }));
    assert(std::any_of(htmlSyntax.begin(), htmlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == L"<img";
    }));
    assert(std::any_of(htmlSyntax.begin(), htmlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_ATTRIBUTE && span.text == L"data-id";
    }));
    assert(std::any_of(htmlSyntax.begin(), htmlSyntax.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE_NUMBER && span.text == L"&copy;";
    }));
    const auto linked = ChatMarkdownSpans(L"[site](https://example.com) [bad](file:///secret) [bad](https://not a host)");
    assert(linked[0].style == ChatMarkdownStyle::LINK && linked[0].target == L"https://example.com");
    assert(std::none_of(linked.begin(), linked.end(), [](const auto &span) { return span.style == ChatMarkdownStyle::LINK && span.target.rfind(L"file:", 0) == 0; }));
    assert(std::none_of(linked.begin(), linked.end(), [](const auto &span) { return span.style == ChatMarkdownStyle::LINK && span.target.find(L' ') != std::wstring::npos; }));
    const auto unicodeWhitespaceLinks = ChatMarkdownSpans(L"[nbsp](https://example.com\u00A0/path) [bom](https://example.com\uFEFF/path)");
    assert(std::none_of(unicodeWhitespaceLinks.begin(), unicodeWhitespaceLinks.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LINK;
    }));
    const auto emptyCode = ChatMarkdownSpans(L"``");
    assert(std::any_of(emptyCode.begin(), emptyCode.end(), [](const auto &span) { return span.style == ChatMarkdownStyle::TEXT && span.text.find(L"``") != std::wstring::npos; }));
    const auto emptyBold = ChatMarkdownSpans(L"x****y");
    assert(std::any_of(emptyBold.begin(), emptyBold.end(), [](const auto &span) { return span.style == ChatMarkdownStyle::TEXT && span.text.find(L"****") != std::wstring::npos; }));
    const auto emptyStrike = ChatMarkdownSpans(L"~~~~");
    assert(std::any_of(emptyStrike.begin(), emptyStrike.end(), [](const auto &span) { return span.style == ChatMarkdownStyle::TEXT && span.text.find(L"~~~~") != std::wstring::npos; }));
    const auto emptyLabelLink = ChatMarkdownSpans(L"[](https://example.com)");
    assert(std::none_of(emptyLabelLink.begin(), emptyLabelLink.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LINK;
    }));
    std::wstring emptyLabelText;
    for (const auto &span : emptyLabelLink) emptyLabelText += span.text;
    assert(emptyLabelText.find(L"[](https://example.com)") != std::wstring::npos);
    const auto whitespaceInline = ChatMarkdownSpans(L"` ` ** ** ~~ ~~");
    assert(std::any_of(whitespaceInline.begin(), whitespaceInline.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE && span.text == L" ";
    }));
    assert(std::any_of(whitespaceInline.begin(), whitespaceInline.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::EMPHASIS && span.text == L" ";
    }));
    assert(std::any_of(whitespaceInline.begin(), whitespaceInline.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::STRIKETHROUGH && span.text == L" ";
    }));
    const auto styledLink = ChatMarkdownSpans(L"[**bold** *italic* ~~strike~~ `code`](https://example.com)");
    assert(std::any_of(styledLink.begin(), styledLink.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::EMPHASIS && span.text == L"bold" && span.target == L"https://example.com";
    }));
    assert(std::any_of(styledLink.begin(), styledLink.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::ITALIC && span.text == L"italic" && span.target == L"https://example.com";
    }));
    assert(std::any_of(styledLink.begin(), styledLink.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::STRIKETHROUGH && span.text == L"strike" && span.target == L"https://example.com";
    }));
    assert(std::any_of(styledLink.begin(), styledLink.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE && span.text == L"code" && span.target == L"https://example.com";
    }));
    const auto listed = ChatMarkdownSpans(L"- first\n  2) nested");
    assert(listed[0].style == ChatMarkdownStyle::LIST && listed[0].text == L"• " && listed[0].listDepth == 0);
    assert(std::any_of(listed.begin(), listed.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LIST && span.text == L"2. " && span.listDepth == 1;
    }));
    const auto listSpacing = ChatMarkdownSpans(L"- first\n- second\n  - nested\n  - nested sibling\n- third\n\n- fresh list");
    std::vector<int> listMarkerGaps;
    for (const auto &span : listSpacing)
        if (span.style == ChatMarkdownStyle::LIST) listMarkerGaps.push_back(span.paragraphSpaceBeforeTwips);
    assert((listMarkerGaps == std::vector<int>{0, 90, 90, 90, 90, 180}));
    const auto spacedParagraphs = ChatMarkdownSpans(L"first paragraph\n\nsecond paragraph");
    std::wstring spacedParagraphText;
    int secondParagraphSpace = -1;
    for (const auto &span : spacedParagraphs)
    {
        spacedParagraphText += span.text;
        if (span.text.find(L"second paragraph") != std::wstring::npos) secondParagraphSpace = span.paragraphSpaceBeforeTwips;
    }
    assert(spacedParagraphText == L"first paragraph\r\nsecond paragraph\r\n" && secondParagraphSpace == 180);
    const auto leadingBlank = ChatMarkdownSpans(L"\nfirst paragraph");
    assert(!leadingBlank.empty() && leadingBlank[0].paragraphSpaceBeforeTwips == 0);
    const auto streamedParagraphTail = ChatMarkdownSpans(L"\n\nstreamed paragraph", 180);
    assert(std::any_of(streamedParagraphTail.begin(), streamedParagraphTail.end(), [](const auto &span) {
        return span.text.find(L"streamed paragraph") != std::wstring::npos && span.paragraphSpaceBeforeTwips == 180;
    }));
    const auto listAfterParagraph = ChatMarkdownSpans(L"paragraph\n- item");
    assert(std::any_of(listAfterParagraph.begin(), listAfterParagraph.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LIST && span.paragraphSpaceBeforeTwips == 180;
    }));
    assert(ChatMarkdownNormalizeLists(L"5. first\n99) second\n- third\n\n3) reset") ==
           L"5. first\n6. second\n7. third\n\n3. reset");
    assert(ChatMarkdownNormalizeLists(L"0. first\n8. second") == L"1. first\n2. second");
    assert(ChatMarkdownNormalizeLists(L"-   spaced   ") == L"- spaced   ");
    assert(ChatMarkdownNormalizeLists(L"1.\t\u00A0spaced") == L"1. spaced");
    assert(ChatMarkdownNormalizeLists(L"\u00A0\u00A0- item") == L"- item");
    assert(ChatMarkdownNormalizeLists(L"- first\n  continued  \n- next") ==
           L"- first continued\n- next");
    assert(ChatMarkdownNormalizeLists(L"- parent\n  5. nested\n  - mixed\n- next") ==
           L"- parent\n  5. nested\n  6. mixed\n- next");
    assert(ChatMarkdownNormalizeLists(L"- root\n        1. child\n- sibling") ==
           L"- root\n  1. child\n- sibling");
    const auto nestedDepths = ChatMarkdownSpans(L"- root\n    1. child\n        - grandchild\n- next");
    assert(std::count_if(nestedDepths.begin(), nestedDepths.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LIST && span.listDepth == 0;
    }) == 2);
    assert(std::any_of(nestedDepths.begin(), nestedDepths.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LIST && span.listDepth == 1 && span.text == L"1. ";
    }));
    assert(std::any_of(nestedDepths.begin(), nestedDepths.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LIST && span.listDepth == 2 && span.text == L"• ";
    }));
    assert(ChatMarkdownNormalizeLists(L"- first\n  continued **text**\n- next") ==
           L"- first continued **text**\n- next");
    assert(ChatMarkdownNormalizeLists(L"- parent\n  - nested\n    continued\n- next") ==
           L"- parent\n  - nested continued\n- next");
    const auto rootBulletIndent = ChatMarkdownListIndents(0, 0, 96);
    assert(rootBulletIndent.first == 360 && rootBulletIndent.second == -180);
    const auto nestedBulletIndent = ChatMarkdownListIndents(1, 0, 96);
    assert(nestedBulletIndent.first == 720 && nestedBulletIndent.second == -180);
    const auto scaledOrderedIndent = ChatMarkdownListIndents(0, 2, 144);
    assert(scaledOrderedIndent.first == 540 && scaledOrderedIndent.second == -540);
    assert(ChatMarkdownNormalizeLists(L"```text\n- literal\n  continuation\n```") ==
           L"```text\n- literal\n  continuation\n```");
    const auto continuedList = ChatMarkdownSpans(L"- first\n  continued **text**\n- next");
    assert(std::any_of(continuedList.begin(), continuedList.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::TEXT && span.text.find(L"first continued ") != std::wstring::npos;
    }));
    assert(std::any_of(continuedList.begin(), continuedList.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::EMPHASIS && span.text == L"text";
    }));
    const auto table = ChatMarkdownSpans(L"| **Name** | Value |\n| :--- | ---: |\n| CPU | 55C |\n| GPU | [link](https://example.com) |");
    assert(table[0].style == ChatMarkdownStyle::EMPHASIS && table[0].text == L"Name");
    std::wstring alignedHeader;
    for (const auto &span : table)
    {
        if (!span.tableCell) continue;
        const auto newline = span.text.find(L"\r\n");
        alignedHeader += span.text.substr(0, newline);
        if (newline != std::wstring::npos) break;
    }
    assert(alignedHeader == L"Name  Value");
    assert(std::all_of(table.begin(), table.end(), [](const auto &span) { return span.tableCell; }));
    assert(std::any_of(table.begin(), table.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::LINK && span.target == L"https://example.com";
    }));
    assert(!ChatMarkdownTableSeparator(L"----"));
    assert(ChatMarkdownTableSeparator(L"| : | -- |"));
    assert(ChatMarkdownTableSeparator(L"|\u00A0:\uFEFF-\u00A0|"));
    assert(ChatMarkdownTableSeparator(L"---|---"));
    assert(!ChatMarkdownTableSeparator(L"plain|---"));
    assert(!ChatMarkdownTableSeparator(L"|---|text|"));
    const auto looseTableSeparator = ChatMarkdownSpans(L"| A | B |\n| : | -- |\n| x | y |");
    assert(std::any_of(looseTableSeparator.begin(), looseTableSeparator.end(), [](const auto &span) {
        return span.tableCell && span.style == ChatMarkdownStyle::TABLE_HEADER;
    }));
    const auto unwrappedPipes = ChatMarkdownSpans(L"plain|text\n---|---\n");
    assert(std::none_of(unwrappedPipes.begin(), unwrappedPipes.end(), [](const auto &span) { return span.tableCell; }));
    assert((ChatMarkdownTableCells(L"\u00A0| \uFEFFA\u00A0 | \u00A0B \uFEFF|\u00A0") ==
            std::vector<std::wstring>{L"A", L"B"}));
    const auto boundedTableRows = ChatMarkdownSpans(L"| A |\n| --- |\nordinary | prose\n| B |\n");
    assert(std::none_of(boundedTableRows.begin(), boundedTableRows.end(), [](const auto &span) {
        return span.tableCell && span.text.find(L"ordinary") != std::wstring::npos;
    }));
    assert(ChatMarkdownHorizontalRule(L"---"));
    assert(ChatMarkdownHorizontalRule(L" - - - "));
    assert(ChatMarkdownHorizontalRule(L"***"));
    assert(ChatMarkdownHorizontalRule(L"___"));
    assert(ChatMarkdownHorizontalRule(L"\u00A0-\uFEFF-\u00A0-\uFEFF"));
    assert(!ChatMarkdownHorizontalRule(L"-*-"));
    assert(!ChatMarkdownHorizontalRule(L"--"));
    const auto rule = ChatMarkdownSpans(L"before\n  - - -  \nafter");
    assert(std::any_of(rule.begin(), rule.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::HORIZONTAL_RULE;
    }));
    const auto fencedTable = ChatMarkdownSpans(L"```text\n| a | b |\n| --- | --- |\n```");
    assert(std::any_of(fencedTable.begin(), fencedTable.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::CODE && span.text.find(L"| --- | --- |") != std::wstring::npos;
    }));
    assert(std::none_of(fencedTable.begin(), fencedTable.end(), [](const auto &span) {
        return span.style == ChatMarkdownStyle::TABLE_HEADER || span.style == ChatMarkdownStyle::TABLE_ROW;
    }));
    const std::wstring streamedMarkdown(6000, L'x');
    assert(ChatMarkdownStreamCut(streamedMarkdown + L"\n\ncommitted", 0) == 6000);
    const std::wstring streamedCode = streamedMarkdown + L"\n```cpp\ncode\n\nmore\n```\n\nnext";
    assert(ChatMarkdownStreamCut(streamedCode, 0) == streamedCode.rfind(L"\n\n", streamedCode.size() - 3));
    assert(ChatMarkdownStreamCut(streamedMarkdown + L"\n```cpp\ncode\n\nmore", 0) == 0);
    assert(ChatMarkdownStreamCut(streamedMarkdown + L"\n\nnext", 6000) == 6000);
    assert(ChatUtf8PrefixCodeUnits(u8"A😀B", 5) == 3);
    assert(MayaCodeExtension(L"python", L"print(1)") == L"py");
    assert(MayaCodeExtension(L" python 3.11", L"print(1)") == L"py");
    assert(MayaCodeExtension(L"\uFEFFpython\u00A0version", L"print(1)") == L"py");
    assert(MayaCodeExtension(L"C++", L"int main()") == L"cpp");
    assert(MayaCodeExtension(L"jsx", L"const App = () => null") == L"jsx");
    assert(MayaCodeExtension(L"TSX extra", L"const App = () => null") == L"tsx");
    const std::pair<const wchar_t *, const wchar_t *> webCodeExtensions[] = {
        {L"python", L"py"}, {L"py", L"py"}, {L"javascript", L"js"}, {L"js", L"js"}, {L"jsx", L"jsx"},
        {L"typescript", L"ts"}, {L"ts", L"ts"}, {L"tsx", L"tsx"}, {L"html", L"html"}, {L"htm", L"html"},
        {L"css", L"css"}, {L"scss", L"scss"}, {L"json", L"json"}, {L"bash", L"sh"}, {L"sh", L"sh"},
        {L"shell", L"sh"}, {L"zsh", L"sh"}, {L"c", L"c"}, {L"h", L"h"}, {L"cpp", L"cpp"},
        {L"c++", L"cpp"}, {L"hpp", L"hpp"}, {L"cuda", L"cu"}, {L"cu", L"cu"}, {L"rust", L"rs"},
        {L"rs", L"rs"}, {L"go", L"go"}, {L"java", L"java"}, {L"kotlin", L"kt"}, {L"swift", L"swift"},
        {L"sql", L"sql"}, {L"yaml", L"yaml"}, {L"yml", L"yml"}, {L"toml", L"toml"}, {L"xml", L"xml"},
        {L"svg", L"svg"}, {L"markdown", L"md"}, {L"md", L"md"}, {L"ps1", L"ps1"},
        {L"powershell", L"ps1"}, {L"php", L"php"}, {L"ruby", L"rb"}, {L"rb", L"rb"}, {L"lua", L"lua"},
        {L"dockerfile", L"Dockerfile"}
    };
    for (const auto &[language, extension] : webCodeExtensions)
        assert(MayaCodeExtension(language, L"code") == extension);
    assert(MayaCodeExtension(L"yml", L"key: value") == L"yml");
    assert(MayaCodeExtension(L"yaml", L"key: value") == L"yaml");
    assert(MayaCodeExtension(L"xhtml", L"<div>fragment</div>") == L"html");
    assert(MayaCodeExtension(L"scss", L"$color: red;") == L"scss");
    assert(MayaCodeExtension(L"css", L"body { color: red; }") == L"css");
    assert(MayaCodeExtension(L"", L"<!doctype html><h1>Hi</h1>") == L"html");
    assert(MayaCodeExtension(L"", L"\u00A0\uFEFF<!DOCTYPE html><h1>Hi</h1>") == L"html");
    assert(MayaCodeExtension(L"", L"<!doctype svg>") == L"txt");
    assert(MayaCodeExtension(L"", L"<htmlish>") == L"txt");
    assert(MayaCodeExtension(L"", L"<html lang='en'>") == L"html");
    assert(MayaCodeExtension(L"unknown", L"plain text") == L"txt");
    assert(MayaCodeExtension(L"unknown", L"<!doctype html><h1>Hi</h1>") == L"txt");
    assert(MayaCodeFilename(L"Dockerfile") == L"Dockerfile");
    assert(MayaCodeFilename(L"jsx") == L"maya-code.jsx");
    const auto preview = MayaSandboxPreviewDocument("<script>document.write(\"A&B\")</script>");
    assert(preview.find("sandbox=\"allow-scripts allow-pointer-lock allow-modals\"") != std::string::npos);
    assert(preview.find("srcdoc=\"&lt;script&gt;document.write(&quot;A&amp;B&quot;)&lt;/script&gt;\"") != std::string::npos);
    assert(preview.find("allow-same-origin") == std::string::npos);
    assert(MayaHasHtmlPreview(true, L"xhtml", L"<div>fragment</div>"));
    assert(MayaHasHtmlPreview(true, L" html extra", L"fragment"));
    assert(MayaHasHtmlPreview(true, L"", L"  <!DOCTYPE html><html></html>"));
    assert(MayaHasHtmlPreview(true, L"", L"\u00A0\uFEFF<!DOCTYPE html><html></html>"));
    assert(MayaHasHtmlPreview(true, L"", L"\uFEFF<html\u00A0lang='en'></html>"));
    assert(!MayaHasHtmlPreview(true, L"", L"<!doctype svg>"));
    assert(!MayaHasHtmlPreview(true, L"", L"<htmlish>"));
    assert(!MayaHasHtmlPreview(false, L"html", L"<!doctype html><p>still streaming</p>"));
    assert(!MayaHasHtmlPreview(true, L"unknown", L"<!doctype html><p>not an HTML-tagged block</p>"));
    assert(ChatMarkdownHasUnclosedFence(L"```html\n<p>in progress</p>"));
    assert(!ChatMarkdownHasUnclosedFence(L"```html\n<p>complete</p>\n```"));
    assert(ChatMarkdownHasUnclosedFence(L"```html\n```not-a-closer\n- still code\n"));
    assert(!ChatMarkdownHasUnclosedFence(L"```html\ntext\n``` \t"));
    const auto fakeFenceCloser = ChatMarkdownSpans(L"```html\n```not-a-closer\n- still code\n```\n");
    assert(std::any_of(fakeFenceCloser.begin(), fakeFenceCloser.end(), [](const auto &span) {
        return span.fenced && span.text.find(L"- still code") != std::wstring::npos;
    }));
    const auto indentedFence = ChatMarkdownSpans(L"  ```html\n<p>plain</p>\n  ```\n");
    assert(std::none_of(indentedFence.begin(), indentedFence.end(), [](const auto &span) { return span.fenced; }));
    const auto trimmedLanguageFence = ChatMarkdownSpans(L"``` \uFEFFcpp\u00A0 extra \nreturn 0;\n```\n");
    assert(std::any_of(trimmedLanguageFence.begin(), trimmedLanguageFence.end(), [](const auto &span) {
        return span.fenced && span.target == L"cpp" && span.text == L"return" && span.style == ChatMarkdownStyle::CODE_KEYWORD;
    }));
    assert(NativeShortcutForKey(13, false, false) == NativeShortcut::SEND);
    assert(NativeShortcutForKey(13, true, false) == NativeShortcut::SEND);
    assert(NativeShortcutForKey(13, true, true) == NativeShortcut::NEWLINE);
    assert(NativeShortcutForKey(13, false, true) == NativeShortcut::NEWLINE);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x27) == 3);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x26) == 3);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x25) == 1);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x28) == 1);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x24) == 0);
    assert(NativeThinkingSelectionAfterKey(2, 5, 0x23) == 4);
    assert(NativeThinkingSelectionAfterKey(0, 5, 0x25) == 0);
    assert(NativeThinkingSelectionAfterKey(4, 5, 0x27) == 4);
    assert(NativeThinkingSelectionAfterKey(2, 5, 13) == -1);
    assert(NativeThinkingComboUsesCustomNavigation(false, false));
    assert(!NativeThinkingComboUsesCustomNavigation(true, false));
    assert(!NativeThinkingComboUsesCustomNavigation(false, true));
    assert(NativeSegmentSelectionAfterKey(0, 3, 0x25) == 2);
    assert(NativeSegmentSelectionAfterKey(0, 3, 0x28) == 1);
    assert(NativeSegmentSelectionAfterKey(2, 3, 0x27) == 0);
    assert(NativeSegmentSelectionAfterKey(2, 3, 0x26) == 1);
    assert(NativeSegmentSelectionAfterKey(-1, 3, 0x25) == 0);
    assert(NativeSegmentSelectionAfterKey(3, 3, 0x27) == 0);
    assert(NativeSegmentSelectionAfterKey(0, 0, 0x27) == -1);
    assert(NativeSegmentSelectionAfterKey(1, 3, 0x24) == -1);
    assert(NativeModelSettingSelectionAfterKey(2, 3, 0x27, true) == 0);
    assert(NativeModelSettingSelectionAfterKey(-1, 3, 0x25, true) == 0);
    assert(NativeModelSettingSelectionAfterKey(2, 3, 0x27, false) == 2);
    assert(NativeModelSettingSelectionAfterKey(2, 3, 0x23, false) == 2);
    assert(NativeShortcutForKey('O', true, true) == NativeShortcut::NEW_CHAT);
    assert(NativeShortcutForKey('K', true, false) == NativeShortcut::SEARCH);
    assert(NativeSearchNeedsChatPage(false));
    assert(!NativeSearchNeedsChatPage(true));
    assert(!NativeSearchShouldClose(false));
    assert(NativeSearchShouldClose(true));
    assert(NativeShortcutForKey(0xBC, true, false) == NativeShortcut::SETTINGS);
    assert(NativeShortcutForKey(',', true, false) == NativeShortcut::NONE);
    assert(NativeShortcutForKey('S', true, true) == NativeShortcut::TOGGLE_SIDEBAR);
    assert(NativeShortcutForKey(27, false, false) == NativeShortcut::ESCAPE);
    assert(NativeShortcutForKey('K', false, false) == NativeShortcut::NONE);
    assert(NativeEscapeShouldYieldToPopup(true));
    assert(!NativeEscapeShouldYieldToPopup(false));
    assert(NativePageFocusMustReset(true, false, true));
    assert(NativePageFocusMustReset(true, true, false));
    assert(!NativePageFocusMustReset(true, true, true));
    assert(!NativePageFocusMustReset(false, false, false));
    assert(NativeEnterMaySubmit(false));
    assert(!NativeEnterMaySubmit(true));
    assert(!NativeEnterMaySubmit(false, true));
    assert(!NativeEnterMaySubmit(true, true));
    assert(NativeAltShortcutMayContinue(13, true));
    assert(!NativeAltShortcutMayContinue(13, false));
    assert(!NativeAltShortcutMayContinue('K', true));
    assert(NativeThinkingBudgetValid(0));
    assert(NativeThinkingBudgetValid(256));
    assert(NativeThinkingBudgetValid(262144));
    assert(!NativeThinkingBudgetValid(255));
    assert(!NativeThinkingBudgetValid(262145));
    assert(NativeEscapeForState(true, false, false, false, false, false) == NativeEscapeAction::CLOSE_SETTINGS);
    assert(NativeEscapeForState(false, false, false, false, false, true) == NativeEscapeAction::NONE);
    assert(NativeEscapeForState(false, true, true, false, false, true) == NativeEscapeAction::CLEAR_SEARCH);
    assert(NativeEscapeForState(false, true, false, true, false, true) == NativeEscapeAction::CLEAR_SEARCH);
    assert(NativeEscapeForState(false, true, false, false, true, true) == NativeEscapeAction::CANCEL_EDIT);
    assert(NativeEscapeForState(false, true, false, false, false, true) == NativeEscapeAction::STOP_CHAT);
    assert(NativeEscapeForState(false, true, false, false, false, false) == NativeEscapeAction::NONE);
    {
        const Json health{{"version", "1.1.50"}, {"model", "deepseek-v4.1-flash"},
                          {"max_context", 272000}, {"images", true}};
        const Json metrics{{"engine", {{"maya_version", "1.1.50"}, {"version", "v1.1.28"},
                                        {"model", "deepseek-v4.1-flash"}, {"engine_kind", "ds4"},
                                        {"max_context", 272000}, {"trained_context", 1000000},
                                        {"kv", "fp4"}, {"kv_resident", 131072}, {"experts", 10000},
                                        {"expert_slots", 80}, {"ram_slots", 3000}, {"dspark", 1}, {"images", true}}},
            {"hardware", {{"gpu_mem_total", 16ULL * 1024 * 1024 * 1024}, {"ram_total", 64ULL * 1024 * 1024 * 1024},
                           {"gpu_pcie_gen_max", 4}, {"gpu_pcie_width", 16},
                           {"gpus", Json::array({{{"index", 0}, {"pcie_gen_max", 4}, {"pcie_width", 16}}})}}},
            {"hardware_static", {{"gpu_name", "NVIDIA GeForce RTX 4080"}, {"cpu_name", "Test CPU"}, {"threads", 16}}},
            {"storage", {{"path", "C:/Maya/models"}, {"free", 100ULL * 1024 * 1024 * 1024},
                         {"total", 500ULL * 1024 * 1024 * 1024}}},
            {"tiers", {{"now", {{"drafts", 0.5}}}}}};
        const auto facts = MayaAboutFacts(health, metrics, "http://127.0.0.1:8082");
        assert(facts.find("Project Maya v1.1.50") != std::string::npos);
        assert(facts.find("Model: deepseek-v4.1-flash") != std::string::npos);
        assert(facts.find("Engine: DeepSeek-V4.1 (maya-ds4)") != std::string::npos);
        assert(facts.find("Context: 272000 tokens") != std::string::npos);
        assert(facts.find("Trained for: up to 1000000 tokens") != std::string::npos);
        assert(facts.find("KV cache: 4-bit FP4 as DeepSeek stores it (the recent-token window FP8), streamed: 131072 positions per layer in VRAM, the rest in RAM") != std::string::npos);
        assert(facts.find("Experts: 10000 routed; room for 80 in VRAM and 3000 in RAM, the rest read from the SSD") != std::string::npos);
        assert(facts.find("Speculation: DSpark: its drafter proposes a few tokens at a time, the full model checks them (50% of them kept in the last answer)") != std::string::npos);
        assert(facts.find("Images: On") != std::string::npos);
        assert(facts.find("GPU: NVIDIA GeForce RTX 4080 · 16 GB") != std::string::npos);
        assert(facts.find("PCIe: Gen4 x16") != std::string::npos);
        assert(facts.find("CPU: Test CPU, 16 threads") != std::string::npos);
        assert(facts.find("RAM: 64 GB") != std::string::npos);
        assert(facts.find("Model folder: C:/Maya/models · 100 GB free of 500 GB") != std::string::npos);
        assert(facts.find("http://127.0.0.1:8082") != std::string::npos);
        assert(MayaAboutFacts(health, Json::object(), "api").find("Model: deepseek-v4.1-flash") != std::string::npos);
    }
    const auto connect = BuildApiConnectInfo("0.0.0.0", 8082, "deepseek-v4.1-flash", true, true);
    const auto lanConnect = BuildApiConnectInfo(
        ApiConnectDisplayHost("0.0.0.0", "192.168.1.222"), 8082, "deepseek-v4.1-flash", true, true);
    assert(lanConnect.openaiBase == "http://192.168.1.222:8082/v1" &&
           lanConnect.anthropicBase == "http://192.168.1.222:8082");
    assert(ApiConnectDisplayHost("0.0.0.0", "") == "127.0.0.1" &&
           ApiConnectDisplayHost("127.0.0.1", "192.168.1.222") == "127.0.0.1");
    assert(MayaApiKeyTextValid(L"secret-test-value"));
    assert(!MayaApiKeyTextValid(L""));
    assert(!MayaApiKeyTextValid(L"bad\rkey"));
    assert(!MayaApiKeyTextValid(std::wstring(L"bad\0key", 7)));
    assert(!MayaApiKeyTextValid(std::wstring(2049, L'x')));
    assert(connect.openaiBase == "http://127.0.0.1:8082/v1" && connect.anthropicBase == "http://127.0.0.1:8082" &&
           connect.model == "deepseek-v4.1-flash" && connect.authentication == "API key supplied by selected profile");
    const auto connectExamples = BuildApiConnectExamples(connect, true);
    assert(connectExamples[0].find("<your API key>") != std::string::npos &&
           connectExamples[1].find("$env:MAYA_MODEL = 'deepseek-v4.1-flash'") != std::string::npos &&
           connectExamples[2].find("curl.exe 'http://127.0.0.1:8082/v1/chat/completions'") != std::string::npos &&
           connectExamples[2].find("Authorization: Bearer <your API key>") != std::string::npos);
    const auto noAuthExamples = BuildApiConnectExamples(connect, false);
    assert(noAuthExamples[2].find("Authorization: Bearer") == std::string::npos &&
           noAuthExamples[1].find("$env:OPENAI_API_KEY = 'maya'") != std::string::npos);
    assert(PowerShellLiteral("model's name") == "'model''s name'");
    assert(BuildApiConnectInfo("::", 8082, "Maya", false, false).openaiBase == "http://[::1]:8082/v1");
    assert(BuildApiConnectInfo("2001:db8::1", 8082, "Maya", true, false).anthropicBase == "http://[2001:db8::1]:8082");
    assert(IsNativeChatDataPath("C:/Users/Daniel/AppData/Local",
        "C:/Users/Daniel/AppData/Local/MayaXS/WinManager/chats"));
    assert(!IsNativeChatDataPath("C:/Users/Daniel/AppData/Local",
        "C:/Users/Daniel/AppData/Local/MayaXS/WinManager"));
    std::vector<int> clearDataSteps;
    assert(NativeClearLocalData([&] { clearDataSteps.push_back(1); return false; },
                                [&] { clearDataSteps.push_back(2); return true; }) ==
           NativeClearDataResult::API_KEY_FAILED);
    assert((clearDataSteps == std::vector<int>{1}));
    clearDataSteps.clear();
    assert(NativeClearLocalData([&] { clearDataSteps.push_back(1); return true; },
                                [&] { clearDataSteps.push_back(2); return false; }) ==
           NativeClearDataResult::CHAT_DATA_FAILED);
    assert((clearDataSteps == std::vector<int>{1, 2}));
    const auto dataPage = NativeDataPageText(12, 3, L"C:\\Users\\Daniel\\AppData\\Local\\MayaXS\\WinManager\\chats");
    assert(dataPage.find(L"12 chats and 3 folders") != std::wstring::npos);
    assert(dataPage.find(L"Recycle Bin") != std::wstring::npos);
    assert(dataPage.find(L"theme and colors are kept") != std::wstring::npos);
    assert(dataPage.find(L"sidebar returns to open") != std::wstring::npos);
    assert(dataPage.find(L"profile API keys are kept; the local API-key override is cleared") != std::wstring::npos);
    assert(ChatSamplingFamily("deepseek-v4.1-flash") == "deepseek");
    assert(ChatSamplingFamily("Qwen-CYBER-FROST") == "qwen");
    assert(ChatSamplingFamily("MiniMax-M3.1") == "minimax");
    assert(ChatSamplingEffortLabel("none") == "Off");
    assert(ChatSamplingEffortLabel("auto") == "Auto");
    assert(ChatSamplingEffortLabel("xhigh") == "XHigh");
    assert(ChatSamplingEffortLabel("max") == "Max");
    assert(ChatSamplingEffortForModel("xhigh", "deepseek-v4.1-flash") == "max");
    assert(ChatSamplingEffortForModel("medium", "deepseek-v4.1-flash") == "high");
    assert(ChatSamplingEffortForModel("high", "qwen3.8-flash-next") == "medium");
    assert(ChatSamplingEffortForModel("max", "qwen3.8-flash-next") == "xhigh");
    assert(ChatSamplingEffortForModel("xhigh", "minimax-m3.1") == "xhigh");
    assert(ChatSamplingEffortForModel("none", "qwen3.8-flash-next") == "none");
    assert(ChatSamplingEffortForChat("xhigh", "deepseek-v4.1-flash", "deepseek-v4.1-flash", "high") == "max");
    assert(ChatSamplingEffortForChat("high", "minimax-m3.1", "qwen3.8-flash-next", "low") == "low");
    assert(ChatSamplingEffortForChat("none", "|legacy-family", "qwen3.8-flash-next", "xhigh") == "none");
    assert(ChatSamplingEffortForChat("none", "qwen3.8-flash-next", "qwen3.8-flash-next", "xhigh") == "none");
    assert(ChatSamplingEffortForChat("", "", "deepseek-v4.1-flash", "high") == "high");
    const Json glmThinkHealth{{"thinking_budget_max", 262144}};
    const Json noLongThinkHealth = Json::object();
    const Json savedMaxThink{{"glm", {{"reasoning_effort", "max"}}}};
    const Json savedHighThink{{"glm", {{"reasoning_effort", "high"}}}};
    assert(NativeChatDefaultThinkBudget(glmThinkHealth, Json::object(), "glm-5.3", "max", false, "glm-5.3"));
    assert(!NativeChatDefaultThinkBudget(glmThinkHealth, Json::object(), "glm-5.3", "max", true, "glm-5.3"));
    assert(NativeChatDefaultThinkBudget(glmThinkHealth, Json::object(), "glm-5.3", "max", true, "other-model"));
    assert(!NativeChatDefaultThinkBudget(glmThinkHealth, savedMaxThink, "glm-5.3", "max", false, "glm-5.3"));
    assert(NativeChatDefaultThinkBudget(glmThinkHealth, savedHighThink, "glm-5.3", "max", false, "glm-5.3"));
    assert(!NativeChatDefaultThinkBudget(noLongThinkHealth, Json::object(), "glm-5.3", "max", false, "glm-5.3"));
    Json defaultThinkRequest = Json::object();
    AddNativeChatDefaultThinkBudget(defaultThinkRequest, glmThinkHealth, Json::object(),
                                    "glm-5.3", "max", false, "glm-5.3");
    assert(defaultThinkRequest.value("strata_level_default", false));
    Json pickedThinkRequest = Json::object();
    AddNativeChatDefaultThinkBudget(pickedThinkRequest, glmThinkHealth, Json::object(),
                                    "glm-5.3", "max", true, "glm-5.3");
    assert(!pickedThinkRequest.contains("strata_level_default"));
    assert(ChatSamplingEfforts("deepseek") == std::vector<std::string>({"none", "low", "high", "max"}));
    assert(ChatSamplingEfforts("qwen") == std::vector<std::string>({"none", "low", "medium", "xhigh"}));
    assert(ChatSamplingEfforts("minimax") == std::vector<std::string>({"none", "auto", "low", "medium", "high", "xhigh", "max"}));
    const auto qwenOff = ResolveChatSampling(Json{{"qwen", {{"reasoning_effort", "none"}}}},
                                             "qwen3.8-flash-next", Json::object());
    assert(qwenOff["reasoning_effort"] == "none");
    const auto qwenOffValidated = ValidateChatSampling(Json{{"family", "qwen"}, {"reasoning_effort", "none"},
        {"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}});
    assert(qwenOffValidated["reasoning_effort"] == "none");
    Json recommended{{"temperature", 0.2}, {"reasoning_effort", "high"}};
    Json sampling = ResolveChatSampling(Json::object(), "deepseek-v4.1-flash", recommended);
    assert(sampling["temperature"] == 0.2 && sampling["top_k"] == 20 && sampling["reasoning_effort"] == "high");
    assert(ChatSamplingUsesFilters(sampling));
    assert(!ChatSamplingUsesFilters(Json{{"temperature", 0.0}}));
    assert(!ChatSamplingUsesFilters(Json{{"temperature", nullptr}}));
    Json deepseekPrefs = SaveChatSampling(Json::object(), "deepseek-v4.1-flash",
        Json{{"family", "deepseek"}, {"reasoning_effort", "max"}, {"temperature", 0.0},
             {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}, {"seed", nullptr}});
    assert(deepseekPrefs.contains("deepseek") && !deepseekPrefs.contains("glm"));
    sampling = ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 0.0}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}});
    auto greedy = ApplyChatSampling(Json::object(), sampling);
    assert(greedy["temperature"] == 0.0 && !greedy.contains("top_p"));
    const auto webSamplingLimits = ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 1.5}, {"top_p", 0.05}, {"top_k", 64}, {"min_p", 0.5},
        {"frequency_penalty", 0}, {"presence_penalty", 2}});
    assert(webSamplingLimits["temperature"] == 1.5 && webSamplingLimits["top_p"] == 0.05 &&
           webSamplingLimits["top_k"] == 64 && webSamplingLimits["min_p"] == 0.5);
    MustThrow([] { ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 1.5001}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}}); }, "temperature");
    MustThrow([] { ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0},
        {"frequency_penalty", -0.1}}); }, "frequency_penalty");
    MustThrow([] { ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0},
        {"presence_penalty", -0.1}}); }, "presence_penalty");
    const auto zeroSeed = ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 0.6}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}, {"seed", 0}});
    assert(zeroSeed["seed"] == 0);
    assert(!ApplyChatSampling(Json::object(), zeroSeed).contains("seed")); // web chat treats seed 0 as unset
    auto sampled = ApplyChatSampling(Json::object(), Json{{"temperature", 0.6}, {"top_p", 0.95},
        {"top_k", 20}, {"min_p", 0.0}, {"seed", 42}, {"max_tokens", 1000}});
    assert(sampled["top_k"] == 20 && sampled["seed"] == 42 && sampled["max_tokens"] == 1000);
    const auto floatingSampled = ApplyChatSampling(Json::object(), Json{{"temperature", 0.6}, {"top_p", 0.95},
        {"top_k", 20.0}, {"min_p", 0.0}, {"seed", 42.0}, {"max_tokens", 1000.0}});
    assert(floatingSampled["top_k"].is_number_integer() && floatingSampled["seed"].is_number_integer() &&
           floatingSampled["max_tokens"].is_number_integer());
    MustThrow([] { ApplyChatSampling(Json::object(), Json{{"temperature", 0.6}, {"top_k", 20.5}}); }, "top_k");
    const auto sharedDefaults = SharedChatDefaults(Json{{"reasoning_effort", "max"}, {"temperature", 0.6},
        {"top_p", 0.9}, {"top_k", 18}, {"min_p", 0.02}, {"seed", nullptr}, {"max_tokens", 1200},
        {"frequency_penalty", 0.2}});
    assert(sharedDefaults["reasoning_effort"] == "max" && sharedDefaults["min_p"] == 0.02 &&
           !sharedDefaults.contains("seed") && sharedDefaults["max_tokens"] == 1200 &&
           sharedDefaults["frequency_penalty"] == 0.2);
    const auto sharedSummary = ChatSharedDefaultsSummary(Json{{"shared", true}, {"defaults", sharedDefaults}});
    assert(sharedSummary.find("API clients get now:") == 0 && sharedSummary.find("thinking max") != std::string::npos &&
           sharedSummary.find("max tokens 1200") != std::string::npos &&
           sharedSummary.find("frequency penalty 0.2") != std::string::npos);
    assert(ChatSharedDefaultsSummary(Json{{"shared", false}, {"defaults", sharedDefaults}}) ==
           "API clients get them for anything they don't set themselves.");
    assert(ChatSharedDefaultsSummary(Json{{"shared", true}, {"defaults", Json::object()}}) ==
           "API clients get them for anything they don't set themselves.");
    const auto sharedGreedy = SharedChatDefaults(Json{{"reasoning_effort", "high"}, {"temperature", 0.0},
        {"top_p", 0.9}, {"top_k", 18}, {"min_p", 0.02}});
    assert(sharedGreedy.size() == 2 && sharedGreedy["temperature"] == 0.0 && !sharedGreedy.contains("top_p"));
    MustThrow([] { ValidateChatSampling(Json{{"family", "deepseek"}, {"reasoning_effort", "max"},
        {"temperature", 3.0}, {"top_p", 0.95}, {"top_k", 20}, {"min_p", 0.0}}); }, "temperature");
    assert(ThoughtSummaryLabel(2.36) == L"Thought for 2.4 s");
    assert(ThoughtSummaryLabel(0.0) == L"Thoughts");
    assert(ThoughtSummaryLabel(std::numeric_limits<double>::quiet_NaN()) == L"Thoughts");
    assert(NativeThoughtIsStreaming(true, true, 3, 3, false)); // remains live through tool calls and tool limits
    assert(!NativeThoughtIsStreaming(true, true, 3, 3, true));
    assert(!NativeThoughtIsStreaming(false, true, 3, 3, false));
    assert(!NativeThoughtIsStreaming(true, false, 3, 3, false));
    assert(!NativeThoughtIsStreaming(true, true, 2, 3, false));
    assert(NativeThoughtRefreshOnAnswerStart(true, true));
    assert(!NativeThoughtRefreshOnAnswerStart(false, true));
    assert(!NativeThoughtRefreshOnAnswerStart(true, false));
    assert(ThoughtLinkHit(10, 20, 14, 15));
    assert(ThoughtLinkHit(10, 20, 20, 20) == false);
    assert(ThoughtLinkHit(10, 20, 12, 12));
    std::set<std::string> expandedThoughts, collapsedThoughts;
    assert(!ThoughtIsExpanded(expandedThoughts, collapsedThoughts, "chat:2", false));
    assert(ThoughtIsExpanded(expandedThoughts, collapsedThoughts, "chat:2", true));
    assert(!ToggleThoughtExpansion(expandedThoughts, collapsedThoughts, "chat:2", true));
    assert(!ThoughtIsExpanded(expandedThoughts, collapsedThoughts, "chat:2", true));
    assert(ToggleThoughtExpansion(expandedThoughts, collapsedThoughts, "chat:2", false));
    assert(ThoughtIsExpanded(expandedThoughts, collapsedThoughts, "chat:2", false));
    long thoughtLinkStart = 18, thoughtLinkEnd = 32;
    ShiftThoughtRangeAfterInsert(thoughtLinkStart, thoughtLinkEnd, 18, 4);
    assert(thoughtLinkStart == 22 && thoughtLinkEnd == 36);
    long selectionStart = 2, selectionEnd = 8;
    ShiftThoughtRangeAfterInsert(selectionStart, selectionEnd, 5, 3);
    assert(selectionStart == 2 && selectionEnd == 11);
    assert(ChatPreferences("Answer briefly", false) == Json({{"system", "Answer briefly"}, {"show_thinking", false}}));
    assert(ChatProfileComboIndex(L"maya-ds4.json") == 0 && ChatProfileComboIndex(L"maya-ds4-abliterated.json") == 1);
    assert(ChatSavedProfile(Json{{"profile", "maya-ds4.json"}}) == L"maya-ds4.json" &&
           ChatSavedProfile(Json{{"profile", "unknown.json"}}) == L"maya-ds4-abliterated.json" &&
           ChatSavedProfile(Json::object()) == L"maya-ds4-abliterated.json");
    assert(ChatPreferences("", true, Json::object(), "", "", nullptr, true, {}, true, {}, {},
                           "maya-ds4.json")["profile"] == "maya-ds4.json");
    const Json turboSnapshot{{"family", "deepseek"}, {"fast", "none"}};
    assert(ChatPreferences("", true, Json::object(), "dark", "default", turboSnapshot)["turbo_snapshot"] == turboSnapshot);
    const auto noMcp = ChatPreferences("", true, Json::object(), "", "", nullptr, false);
    assert(noMcp["mcp"] == false && !ChatMcpEnabled(noMcp));
    assert(ChatMcpEnabled(ChatPreferences("", true)));
    assert(ChatMcpEnabled(Json{{"mcp", "false"}}));
    const auto openedGpuPrefs = ChatPreferences("", true, Json::object(), "", "", nullptr, true, "2");
    assert(ChatMonitorGpuOpenKey(openedGpuPrefs) == "2");
    assert(ChatMonitorGpuOpenKey(Json::object()).empty());
    const std::vector<Json> gpuRows{{{"index", 4}}, {{"index", 2}}, {{"dev", 1}}};
    assert(MonitorGpuRowKey(gpuRows[2]) == "d1");
    assert(MonitorGpuOpenIndex(gpuRows, "2") == 1);
    assert(MonitorGpuOpenIndex(gpuRows, "4") == 0);
    assert(MonitorGpuOpenIndex(gpuRows, "9") == -1);
    assert(!ChatSidebarOpen(ChatPreferences("", true, Json::object(), "", "", nullptr, true, {}, false)));
    assert(ChatSidebarOpen(ChatPreferences("", true)));
    assert(ChatSidebarOpen(Json{{"side_open", "false"}}));
    const std::vector<std::wstring> savedChatIds{L"newer", L"last-opened"};
    assert(ChatPreferredIndex(savedChatIds, L"last-opened") == 1);
    assert(ChatPreferredIndex(savedChatIds, L"missing") == 0);
    assert(ChatPreferredIndex({}, L"last-opened") == -1);
    const auto chatSelection = ChatPreferences("", true, Json::object(), "", "", nullptr, true, {}, true, L"last-opened");
    assert(ChatCurrentChatId(chatSelection) == L"last-opened");
    const auto modelPrefs = ChatPreferences("", true, Json::object(), "", "", nullptr, true, {}, true,
                                            L"last-opened", "deepseek-v4.1-flash|DeepSeek V4.1 Flash");
    assert(ChatSavedModelId(modelPrefs) == "deepseek-v4.1-flash|DeepSeek V4.1 Flash");
    assert(ChatModelIdentity(Json{{"model", "deepseek-v4.1-flash"}, {"model_label", "DeepSeek V4.1 Flash"}}) ==
           "deepseek-v4.1-flash|DeepSeek V4.1 Flash");
    assert(!ChatModelChanged("", "deepseek-v4.1-flash|DeepSeek V4.1 Flash"));
    assert(!ChatModelChanged("deepseek-v4.1-flash|DeepSeek V4.1 Flash", "deepseek-v4.1-flash|DeepSeek V4.1 Flash"));
    assert(ChatModelChanged("deepseek-v4.1-flash|DeepSeek V4.1 Flash", "qwen|Qwen"));
    assert(ChatModelListNeedsRefreshAfterStatus(true, false));
    assert(!ChatModelListNeedsRefreshAfterStatus(false, false));
    assert(!ChatModelListNeedsRefreshAfterStatus(true, true));
    const auto clearedSampling = ResetChatSamplingForModelChange(Json{
        {"deepseek", {{"temperature", 0.2}, {"reasoning_effort", "max"}}},
        {"minimax", {{"top_k", 18}, {"other", "preserved"}}}});
    assert(!clearedSampling.contains("deepseek") && clearedSampling["minimax"]["other"] == "preserved" &&
           !clearedSampling["minimax"].contains("top_k"));
    assert(ChatCurrentChatId(Json{{"current_chat", 7}}).empty());
    assert(ChatPreferences("Answer briefly", false, Json::object(), "light", "violet") ==
           Json({{"system", "Answer briefly"}, {"show_thinking", false}, {"theme", "light"}, {"scheme", "violet"}}));
    assert(ChatDefaultInstructions(ChatPreferences("Answer briefly", false)) == "Answer briefly");
    assert(!ChatShowThinking(ChatPreferences("Answer briefly", false)));
    assert(ChatShowThinking(ChatPreferences("Answer briefly")));
    assert(ChatShowThinking(Json::object()));
    assert(ChatShowThinking(Json{{"show_thinking", "no"}}));
    assert(ChatDefaultInstructions(Json{{"system", nullptr}}).empty());
    assert(ChatDefaultInstructions(Json::array()).empty());
    assert(NativeInstructionAutosaveNeeded(true, true));
    assert(!NativeInstructionAutosaveNeeded(false, true));
    assert(!NativeInstructionAutosaveNeeded(true, false));
    assert(ChatThemeComboIndex("system") == 0 && ChatThemeComboIndex("dark") == 1 &&
           ChatThemeComboIndex("light") == 2 && ChatThemeComboIndex("unknown") == 1);
    assert(ChatSchemeComboIndex("default") == 0 && ChatSchemeComboIndex("violet") == 1 &&
           ChatSchemeComboIndex("unknown") == 0);
    assert(ChatSavedScheme(Json::object()) == "violet" &&
           ChatSavedScheme(Json{{"scheme", "violet"}}) == "violet" &&
           ChatSavedScheme(Json{{"scheme", "default"}}) == "default" &&
           ChatSavedScheme(Json{{"scheme", "unknown"}}) == "default" &&
           ChatSavedScheme(Json{{"scheme", nullptr}}) == "violet");
    assert(CatalogCanDownload(Json{{"id", "glm-maya-xs"}, {"installed", false}}));
    assert(!CatalogCanDownload(Json{{"id", "deepseek-maya-xs"}, {"installed", true}}));
    const Json groupedCatalog = Json::array({
        Json{{"id", "other-first"}, {"group", "other"}},
        Json{{"id", "maya"}, {"group", "maya"}},
        Json{{"id", "other-second"}, {"group", "other"}}});
    const auto catalogRows = CatalogListRowModelIndices(groupedCatalog);
    assert((catalogRows == std::vector<int>{1, -1, 0, 2}));
    assert(CatalogModelIndexForListRow(catalogRows, 1) == -1);
    assert(CatalogModelIndexForListRow(catalogRows, 2) == 0);
    assert(CatalogModelIndexForListRow(catalogRows, 4) == -1);
    const Json mayaOnlyCatalog = Json::array({Json{{"id", "maya"}, {"group", "maya"}}});
    assert((CatalogListRowModelIndices(mayaOnlyCatalog) == std::vector<int>{0}));
    const Json malformedGroup = Json::array({Json{{"id", "unknown-group"}, {"group", 1}}});
    assert((CatalogListRowModelIndices(malformedGroup) == std::vector<int>{0}));
    assert(CatalogListRowModelIndices(Json::object()).empty());
    const Json pullableSetup{{"id", "glm-maya-xs"}, {"installed", false}};
    assert(CatalogCanStartPull(pullableSetup, false, false, false));
    assert(!CatalogCanStartPull(pullableSetup, true, false, false));
    assert(!CatalogCanStartPull(pullableSetup, false, true, false));
    assert(!CatalogCanStartPull(pullableSetup, false, false, true));
    const Json failedPull{{"model", "glm-maya-xs"}, {"state", "error"}, {"error", "checksum mismatch"}};
    assert(CatalogDownloadFailed(pullableSetup, failedPull));
    assert(!CatalogDownloadFailed(Json{{"id", "other"}}, failedPull));
    assert(!CatalogDownloadFailed(pullableSetup, Json{{"model", "glm-maya-xs"}, {"state", "success"}}));
    assert(!CatalogDownloadFailed(pullableSetup, Json{{"model", nullptr}, {"state", "error"}}));
    assert(ModelCatalogHasSnapshot(Json{{"models", Json::array()}}));
    assert(ModelCatalogHasSnapshot(Json{{"models", Json::array({Json{{"id", "cached"}}})}}));
    assert(!ModelCatalogHasSnapshot(Json::object()));
    assert(!ModelCatalogHasSnapshot(Json{{"models", nullptr}}));
    const auto catalogErrorStatus = CatalogUnavailableStatus("HTTP 503");
    assert(catalogErrorStatus == "Model catalog unavailable: HTTP 503");
    assert(CatalogStatusIsUnavailable(catalogErrorStatus));
    assert(!CatalogStatusIsUnavailable("Model catalog loaded."));
    assert(CatalogSizeLabel(Json{{"size_gb", 96.5}}) == "about 97 GB");
    assert(std::wstring(NativeModelSetupCommand(1)) == L"START-MAYA.bat --setup");
    assert(std::wstring(NativeModelSetupCommand(2)) == L"./maya.sh --setup");
    assert(NativeModelSetupCommand(0) == nullptr);
    static_assert(NativeAboutReportControlId != NativeModelSetupCommandControlId);
    std::ostringstream localizedCatalogSize;
    try { localizedCatalogSize.imbue(std::locale("")); }
    catch (...) {}
    localizedCatalogSize << std::fixed << std::setprecision(0) << std::round(1234.5);
    assert(CatalogSizeLabel(Json{{"size_gb", 1234.5}}) == "about " + localizedCatalogSize.str() + " GB");
    assert(CatalogSizeLabel(Json{{"size_gb", 0}}) == "size shown when it starts");
    assert(CatalogSizeLabel(Json{{"size_gb", nullptr}}) == "size shown when it starts");
    const Json qwenCatalogOffer{{"id", "qwen-ud-iq2_xs"}, {"family", "qwen"},
                                {"name", "Qwen-3.8-Flash-Next · unsloth UD-IQ2_XS"}, {"group", "other"},
                                {"by", "unsloth"}, {"note", ""}, {"installed", false}, {"size_gb", 80.9}};
    assert(CatalogDetailsLabel(qwenCatalogOffer) == "by unsloth · about 81 GB");
    assert(CatalogCanStartPull(qwenCatalogOffer, false, false, false));
    const Json gatedCatalogOffer{{"id", "deepseek-abliterated-128g"}, {"group", "other"},
                                 {"by", "orcarouter"},
                                 {"note", "gated repo: needs your Hugging Face token (HF_TOKEN or `hf auth login`)"},
                                 {"installed", false}, {"size_gb", 275.3}};
    assert(CatalogDetailsLabel(gatedCatalogOffer) ==
           "by orcarouter · about 275 GB · gated repo: needs your Hugging Face token (HF_TOKEN or `hf auth login`)");
    assert(!CatalogCanDownload(Json{{"installed", false}}));
    assert(!CatalogCanDownload(Json{{"id", nullptr}, {"installed", false}}));
    assert(std::wstring(NativeModelLoadButtonText(false, true, false)) == L"Loading…");
    assert(std::wstring(NativeModelLoadButtonText(true, false, true)) == L"Unloading…");
    assert(std::wstring(NativeModelLoadButtonText(false, false, false)) == L"Load model");
    assert(std::wstring(NativeModelLoadButtonText(true, false, false)) == L"Unload model");
    assert(NativeModelLoadButtonEnabled(true, true, false));
    assert(!NativeModelLoadButtonEnabled(true, true, true));
    assert(!NativeModelLoadButtonEnabled(false, true, false));
    assert(!NativeModelLoadButtonEnabled(true, false, false));
    assert(!CatalogInstalled(Json{{"installed", nullptr}}));
    assert(!CatalogInstalled(Json{{"installed", ""}}));
    assert(!CatalogInstalled(Json{{"installed", 0}}));
    assert(CatalogInstalled(Json{{"installed", "yes"}}));
    assert(CatalogInstalled(Json{{"installed", 1}}));
    assert(CatalogInstalled(Json{{"installed", Json::array()}}));
    assert(CatalogCanDownload(Json{{"id", "x"}, {"installed", nullptr}}));
    assert(!CatalogCanDownload(Json{{"id", "x"}, {"installed", "yes"}}));
    assert(CatalogCanDownload(Json{{"id", "x"}, {"installed", ""}}));
    assert(!ModelCatalogVisible(false, false));
    assert(ModelCatalogVisible(true, false));
    assert(ModelCatalogVisible(false, true));
    assert(PullStatusText(Json{{"model", "glm-maya-xs"}, {"state", "starting"},
                               {"message", "Preparing the model"}}) == "Preparing the model");
    assert(PullStatusText(Json{{"model", "glm-maya-xs"}, {"state", "downloading"},
                               {"completed", 5000000000LL}, {"total", 10000000000LL},
                               {"bytes_per_s", 20000000.0}}).find("50%") != std::string::npos);
    assert(PullProgressBarPosition(Json{{"state", "starting"}}) == 4);
    assert(PullProgressBarPosition(Json{{"state", "downloading"}, {"completed", 50}, {"total", 100}}) == 4);
    assert(PullProgressBarPosition(Json{{"state", "downloading"}, {"percent", 37.8}}) == 37);
    assert(PullProgressBarPosition(Json{{"state", "downloading"}, {"percent", -2.0}}) == 0);
    assert(PullProgressBarPosition(Json{{"state", "downloading"}, {"percent", 145.0}}) == 100);
    assert(PullProgressBarPosition(Json{{"state", "error"}, {"percent", 40.0}}) == 40);
    assert(PullProgressBarPosition(Json{{"state", "error"}}) == 4);
    assert(PullStatusText(Json{{"model", "glm-maya-xs"}, {"state", "success"}}) == "Download complete: glm-maya-xs");
    assert(PullStatusText(Json{{"state", "cancelled"}}).find("Partial files are retained for resume") != std::string::npos);
    assert(PullStatusText(Json{{"state", "error"}, {"error", "checksum failed"}}).find("checksum failed") != std::string::npos);
    const auto deletePreview = ModelDeleteRequest("model.json", true);
    assert(deletePreview == Json({{"file", "model.json"}, {"files", true}, {"dry_run", true}}));
    const auto deleteConfig = ModelDeleteRequest("model.json", false);
    assert(deleteConfig == Json({{"file", "model.json"}, {"files", false}, {"then", nullptr}}));
    const auto deleteFiles = ModelDeleteRequest("model.json", false, true, "other.json");
    assert(deleteFiles == Json({{"file", "model.json"}, {"files", true}, {"then", "other.json"}}));
    assert(ModelDeleteItemLabel(Json{{"what", "folder"}}).find("weights") != std::string::npos);
    assert(ModelDeleteItemLabel(Json{{"what", "pack"}}) == "Maya-built model pack");
    assert(ModelDeleteItemLabel(Json{{"what", "gguf"}}) == "GGUF model file");
    assert(ModelDeleteItemLabel(Json{{"what", "vision"}}) == "Image encoder");
    assert(ModelDeleteItemLabel(Json{{"what", "drafter"}}) == "DFlash2 drafter");
    assert(ModelDeleteItemLabel(Json{{"what", "future-type"}}) == "Model file");
    const auto storagePlanText = ModelDeletePlanReport(Json{
        {"ok", true}, {"bytes", 4096},
        {"items", Json::array({Json{{"what", "gguf"}, {"bytes", 4096}, {"path", "C:/models/part.gguf"}}})},
        {"shared", Json::array({Json{{"what", "vision"}, {"path", "C:/models/vision.gguf"},
                                  {"users", Json::array({"other.json"})}}})},
        {"linked", 1}, {"kept", Json::array({"Shared tables stay."})}});
    assert(storagePlanText.find("4096 bytes") != std::string::npos &&
           storagePlanText.find("C:/models/part.gguf") != std::string::npos &&
           storagePlanText.find("C:/models/vision.gguf") != std::string::npos &&
           storagePlanText.find("other.json") != std::string::npos &&
           storagePlanText.find("hard links") != std::string::npos &&
           storagePlanText.find("Shared tables stay.") != std::string::npos);
    assert(ModelDeletePlanReport(Json{{"ok", false}, {"why", "outside Maya's model folders"}})
               .find("outside Maya's model folders") != std::string::npos);
    const auto unavailableDeletePlan = ModelDeletePlanFailure("Maya could not inspect the model files.");
    assert(!unavailableDeletePlan.value("ok", true) && unavailableDeletePlan["items"].empty() &&
           ModelDeletePlanReport(unavailableDeletePlan).find("Maya could not inspect the model files.") !=
               std::string::npos);
    assert(ModelDeletePlanReport(Json{{"ok", true}, {"items", Json::array()}})
               .find("No downloaded model files") != std::string::npos);
    const auto deleteTargets = ModelDeleteTargets(Json::array({
        Json{{"file", "running.json"}, {"running", true}, {"ok", true}},
        Json{{"file", "broken.json"}, {"running", false}, {"ok", false}},
        Json{{"file", "first.json"}, {"running", false}, {"ok", true}},
        Json{{"file", nullptr}, {"running", false}, {"ok", true}},
        Json{{"file", ""}, {"running", false}, {"ok", true}},
        Json{{"file", "second.json"}, {"running", false}, {"ok", true}}
    }));
    assert(deleteTargets == std::vector<std::string>({"first.json", "second.json"}));
    const auto cancelledDeleteReview = ResolveModelDeleteReview(false, true, true, true, deleteTargets, 0);
    assert(!cancelledDeleteReview.accepted && !cancelledDeleteReview.deleteFiles && cancelledDeleteReview.then.empty());
    const auto runningDeleteReview = ResolveModelDeleteReview(true, true, true, false, deleteTargets, 1);
    assert(runningDeleteReview.accepted && !runningDeleteReview.deleteFiles && runningDeleteReview.then == "second.json");
    const auto fileDeleteReview = ResolveModelDeleteReview(true, false, true, true, deleteTargets, 99);
    assert(fileDeleteReview.accepted && fileDeleteReview.deleteFiles && fileDeleteReview.then.empty());
    const auto unavailableFilesReview = ResolveModelDeleteReview(true, false, false, true, deleteTargets, 0);
    assert(unavailableFilesReview.accepted && !unavailableFilesReview.deleteFiles);
    assert(!ResolveModelDeleteReview(true, true, true, false, deleteTargets, deleteTargets.size()).accepted);
    assert(ModelDeleteOutcomeKey(Json{{"time", 1730000000}}) == "1730000000");
    assert(ModelDeleteOutcomeKey(Json{{"time", nullptr}}).empty());
    assert(ModelDeleteOutcomeIncludesFiles(Json{{"files", true}}));
    assert(!ModelDeleteOutcomeIncludesFiles(Json{{"files", nullptr}}));
    assert(ModelDeleteOutcomeFreed(Json{{"freed", 1234}}) == 1234 && ModelDeleteOutcomeFreed(Json{{"freed", -1}}) == 0);
    assert(ModelDeleteOutcomeText(Json{{"deleted", "old-model.json"}, {"files", true}}) ==
           "old-model.json deleted. Its downloaded files were also removed.");
    assert(ModelDeleteOutcomeText(Json{{"model", "DeepSeek"}, {"quant", "Q4"}, {"drafter_users", {"other.json"}}}) ==
           "DeepSeek Q4 deleted. Models now using MTP: other.json.");
    assert(ModelDeleteOutcomeText(Json{{"file", "old-model.json"}, {"error", "it is running"}}) ==
           "old-model.json was not deleted: it is running");
    assert(ModelDeleteOutcomeText(Json{{"model", nullptr}, {"quant", nullptr}, {"files", nullptr}}).empty());
      const Json runningDeleteModel{{"file", "running.json"}, {"running", true}};
      const Json stoppedDeleteModel{{"file", "stopped.json"}, {"running", false}};
      assert(ModelDeleteEnabled(stoppedDeleteModel, Json::array(), false));
      assert(!ModelDeleteEnabled(runningDeleteModel, Json::array({runningDeleteModel}), true));
      assert(!ModelDeleteEnabled(runningDeleteModel, Json::array({runningDeleteModel,
          Json{{"file", "broken.json"}, {"running", false}, {"ok", false}}}), true));
      assert(ModelDeleteEnabled(runningDeleteModel, Json::array({runningDeleteModel,
          Json{{"file", "ready.json"}, {"running", false}, {"ok", true}}}), true));
    Json savingsFixture{{"usd", 0.42}, {"start", {{"usd", nullptr}}}};
    savingsFixture["models"] = Json::array({Json{{"model", "unknown-model"}, {"quant", nullptr}, {"running", true},
        {"tokens", {{"requests", 3}, {"input", 20}, {"cached", 5}, {"output", 7}}}, {"usd", nullptr}}});
    savingsFixture["price_list"] = Json::array({Json{{"model", "unknown-model"}, {"api", nullptr},
        {"input", nullptr}, {"cached", nullptr}, {"output", nullptr}}});
    savingsFixture["since"] = 1793341905.0;
    savingsFixture["start"] = Json{{"model", "deepseek-v4.1-flash"}, {"quant", "Q4"},
                                    {"started", 1000}, {"usd", 0.123}};
    savingsFixture["price_list"] = Json::array({
        Json{{"model", "unknown-model"}, {"api", nullptr}, {"input", nullptr}, {"cached", nullptr}, {"output", nullptr}},
        Json{{"model", "deepseek-v4.1-flash"}, {"api", "DeepSeek"},
             {"input", 0.15}, {"cached", 0.003}, {"output", 0.6}, {"peak", 2.0}}});
    savingsFixture["prices"] = Json::array({Json{{"api", "DeepSeek"}, {"peak", 2.0}}});
    const auto savingsText = SavingsReportText(savingsFixture);
    assert(savingsText.find("SAVED TOTAL   $0.42") != std::string::npos);
    assert(SavingsMoney(0.004) == "< $0.01" && SavingsPriceMoney(0.003) == "$0.003");
    assert(SavingsPriceMoney(1e-6) == "$0.000001" && SavingsPriceMoney(1e-9) == "$1e-9" &&
           SavingsPriceMoney(1e-7) == "$1e-7");
    assert(SavingsPriceEditorText(0.5, false) == "0.50");
    assert(SavingsPriceEditorText(0.0, false) == "0.00");
    assert(SavingsPriceEditorText(0.003, false) == "0.003");
    assert(SavingsPriceEditorText(0.1234567890123456, false) == "0.1234567890123456");
    assert(SavingsPriceEditorText(1e-9, false) == "1e-9");
    assert(SavingsPriceEditorText(2.0, true) == "2");
    assert(SavingsPriceEditorText(Json(), false).empty());
    assert(SavingsUptime(1000, 1120) == "2 min" && SavingsUptime(1000, 4720) == "1 h 2 min");
    assert(!SavingsDate(Json()).size() && !SavingsDate(-1.0).size());
    assert(SavingsSidebarLabel({{"savings", {{"priced", true}, {"usd", 5.409294}}}}) ==
           "Saved $5.41 vs the API");
    assert(SavingsSidebarLabel({{"savings", {{"priced", true}, {"usd", nullptr}}}}) ==
           "Saved – vs the API");
    assert(SavingsSidebarLabel({{"savings", {{"priced", false}, {"usd", 0.0}}}}).empty());
    assert(SavingsSidebarLabel(Json::object()).empty());
    assert(savingsText.find("Requests: 3 · read 25 (5 cached) · wrote 7 · saved no API price") != std::string::npos);
    assert(savingsText.find("THIS START    DeepSeek V4.1 Flash · Q4 · up ") != std::string::npos &&
           savingsText.find("What DeepSeek's own API would have charged for the same tokens, since ") != std::string::npos &&
           savingsText.find("cached $0.003") != std::string::npos &&
           savingsText.find("peak ×2") != std::string::npos &&
           savingsText.find("electricity is not counted") != std::string::npos);
    auto multipleApiSavings = savingsFixture;
    multipleApiSavings["prices"].push_back(Json{{"api", "OpenAI"}});
    assert(SavingsReportText(multipleApiSavings).find("DeepSeek's and OpenAI's own APIs") != std::string::npos);
    assert(savingsText.find("input – · cached – · output –") != std::string::npos);
    assert(SavingsMoney(Json()) == "–" && SavingsPriceMoney(Json()) == "–");
    const auto customPrice = SavingsPriceRequest("deepseek-v4.1-flash", {"0.15", "0.003", "0.60", "2"}, true);
    assert(customPrice["model"] == "deepseek-v4.1-flash" && customPrice["price"]["peak"] == 2.0 &&
           customPrice["price"]["cached"] == 0.003);
    assert(SavingsPriceNumber(" .5 ", "Input price", 0, 1000) == 0.5);
    assert(SavingsPriceNumber("\xC2\xA0.5\xC2\xA0", "Input price", 0, 1000) == 0.5);
    assert(SavingsPriceNumber("\xEF\xBB\xBF" "1." "\xEF\xBB\xBF", "Input price", 0, 1000) == 1.0);
    assert(SavingsPriceNumber("1.", "Input price", 0, 1000) == 1.0);
    assert(!SavingsPriceDraftChanged({"0.50", "0.005", "1.25"}, {"0.50", "0.005", "1.25"}));
    assert(!SavingsPriceDraftChanged({" 0.50\xC2\xA0", "0.005", "1.25"}, {"0.50", "0.005", "1.25"}));
    assert(SavingsPriceDraftChanged({"0.51", "0.005", "1.25"}, {"0.50", "0.005", "1.25"}));
    const std::vector<std::string> savedPrices{"0.50", "0.005", "1.25"};
    const std::vector<std::string> editedPrices{"0.51", "0.005", "1.25"};
    assert(SavingsPriceDraftCanRestore(editedPrices, savedPrices, savedPrices));
    assert(!SavingsPriceDraftCanRestore(editedPrices, savedPrices, {"0.60", "0.005", "1.25"}));
    assert(!SavingsPriceDraftCanRestore({"0.51"}, savedPrices, savedPrices));
    assert(SavingsPriceDraftChanged({"0.50", "0.005"}, {"0.50", "0.005", "1.25"}));
    for (const auto *bad : {"+1", "-0", "1e2", ".", "1 0"})
        MustThrow([&] { SavingsPriceNumber(bad, "Input price", 0, 1000); }, "must be a number");
    MustThrow([&] { SavingsPriceRequest("model", {"0", "0", "1001"}, false); }, "Output price");
    MustThrow([&] { SavingsPriceRequest("deepseek-v4.1-flash", {"0.1", "0.1", "0.1", "10.1"}, true); },
              "Peak-hours multiplier");
    const auto settingsPreview = ModelSettingsRequest("maya-model.json", {{"context", 131072}, {"images", true}}, true);
    assert(settingsPreview["file"] == "maya-model.json" && settingsPreview["dry_run"] == true &&
           settingsPreview["settings"]["context"] == 131072 && settingsPreview["settings"]["env"].is_object() &&
           settingsPreview["settings"]["env"].empty());
    const auto environmentPreview = ModelSettingsRequest("maya-model.json",
        {{"env", {{whitespaceEnvKey, "ignored"}, {" CACHE ", "kept"}, {"CACHE", 8}}}}, true);
    assert(environmentPreview["settings"]["env"] == (Json{{" CACHE ", "kept"}, {"CACHE", 8}}));
    const auto textNumberPreview = ModelSettingsRequest("maya-model.json",
        {{"context", "131072"}, {"prefill_chunk", ""}, {"reserve_gb", ".5"},
         {"cache_gb", "1e2"}, {"cpu_threads", "not-a-number"}, {"ram_gb", "8"}}, true);
    assert(textNumberPreview["settings"]["context"] == 131072 &&
           textNumberPreview["settings"]["prefill_chunk"].is_null() &&
           textNumberPreview["settings"]["reserve_gb"] == 0.5 &&
           textNumberPreview["settings"]["cache_gb"] == "1e2" &&
           textNumberPreview["settings"]["cpu_threads"] == "not-a-number" &&
           textNumberPreview["settings"]["ram_gb"] == "8");
    const auto settingsSave = ModelSettingsRequest("maya-model.json", {{"parallel", 1}}, false);
    assert(!settingsSave.contains("dry_run") && settingsSave["settings"]["parallel"] == 1);
    MustThrow([&] { ModelSettingsRequest("maya-model.json", Json::array(), true); }, "JSON object");
    assert(ModelContextTelemetryRequested({{"running", "maya-model.json"}, {"loaded", false}}));
    assert(!ModelContextTelemetryRequested({{"running", nullptr}, {"loaded", true}}));
    assert(!ModelContextTelemetryRequested(Json::object()));
    const auto contextHint = ModelSettingsContextHint({{"context", 262144}}, {{"context", 262144}});
    assert(contextHint.find(L"GPU memory from experts") != std::wstring::npos);
    const Json measuredVram{{"context", 131072}, {"kv_gb", 4.0}, {"kv_ctx", 131072},
                            {"kv_resident", 0}, {"vram_gb", 16.0}, {"vram_slots", 4}};
    const auto liveVramHint = ModelSettingsContextHint(
        {{"context", 131072}, {"parallel", 1}}, {{"context", 262144}, {"parallel", 1}},
        measuredVram, true, "deepseek");
    assert(liveVramHint.find(L"8.0 GB of GPU memory") != std::wstring::npos &&
           liveVramHint.find(L"fewer experts held in VRAM") != std::wstring::npos);
    const Json measuredRam{{"context", 131072}, {"kv_gb", 0.0}, {"kv_ctx", 0},
                           {"kv_resident", 32768}, {"kv_ram_gb", 8.0}};
    const auto liveRamHint = ModelSettingsContextHint(
        {{"context", 131072}, {"parallel", 1}}, {{"context", 262144}, {"parallel", 1}},
        measuredRam, true, "deepseek");
    assert(liveRamHint.find(L"16.0 GB of RAM for the context") != std::wstring::npos &&
           liveRamHint.find(L"8.0 GB more than now") != std::wstring::npos);
    const auto zeroContextHint = ModelSettingsContextHint(
        {{"context", 0}, {"parallel", 1}}, {{"context", 262144}, {"parallel", 1}},
        measuredRam, true, "deepseek");
    assert(zeroContextHint.find(L"8.0 GB more than now") != std::wstring::npos);
    const auto changedLiveSetting = ModelSettingsContextHint(
        {{"context", 131072}, {"parallel", 1}}, {{"context", 262144}, {"parallel", 2}},
        measuredVram, true, "deepseek");
    assert(changedLiveSetting.find(L"GPU memory from experts") != std::wstring::npos);
    const auto glmStreamingHint = ModelSettingsContextHint(
        {{"context", 131072}, {"parallel", 2}, {"kv_streaming", true}, {"kv_resident", 32768}},
        {{"context", 65536}, {"parallel", 2}, {"kv_streaming", true}, {"kv_resident", 32768}},
        Json::object(), false, "glm");
    assert(glmStreamingHint.find(L"1.5 GB of RAM for the context") != std::wstring::npos);
    const auto glmWholeCacheHint = ModelSettingsContextHint(
        {{"context", 32768}, {"parallel", 1}, {"kv_streaming", true}, {"kv_resident", 32768}},
        {{"context", 16384}, {"parallel", 1}, {"kv_streaming", true}, {"kv_resident", 32768}},
        Json::object(), false, "glm");
    assert(glmWholeCacheHint.find(L"whole cache stays in VRAM") != std::wstring::npos);
    assert(ModelSettingsContextHint({{"context", 262144}}, {{"context", 131072}}) == contextHint);
    assert(ModelSettingsContextHint({{"context", 262144}}, {{"context", 272000}}).find(L"untested") == std::wstring::npos);
    assert(ModelSettingsContextHint({{"context", 262144}}, {{"context", 300000}}) == contextHint);
    assert(ModelSettingsContextHint(Json::object(), {{"context", 300000}}) == contextHint);
    Json modelFields{{"context", 32768}, {"images", false}, {"draft", "mtp"}, {"prefill_chunk", nullptr},
                     {"env", {{"CACHE", "1"}}}};
    assert(ModelSettingsSetScalar(modelFields, "context", "131072") && modelFields["context"] == 131072 &&
           ModelSettingsSetScalar(modelFields, "images", "true") && modelFields["images"] == true &&
           ModelSettingsSetScalar(modelFields, "draft", "dspark") && modelFields["draft"] == "dspark" &&
           ModelSettingsSetScalar(modelFields, "prefill_chunk", "") && modelFields["prefill_chunk"].is_null() &&
           !ModelSettingsSetScalar(modelFields, "context", "12oops") && !ModelSettingsSetScalar(modelFields, "env", "x") &&
           modelFields["env"]["CACHE"] == "1");
    Json decimalFields{{"reserve_gb", nullptr}, {"cache_gb", 1.0}, {"dspark_conf", 0.5}};
    assert(ModelSettingsSetScalar(decimalFields, "reserve_gb", ".5") && decimalFields["reserve_gb"] == 0.5 &&
           ModelSettingsSetScalar(decimalFields, "cache_gb", "1.") && decimalFields["cache_gb"] == 1.0 &&
           ModelSettingsSetScalar(decimalFields, "dspark_conf", "\xC2\xA0.7\xC2\xA0") && decimalFields["dspark_conf"] == 0.7 &&
           !ModelSettingsSetScalar(decimalFields, "cache_gb", "-1") &&
           !ModelSettingsSetScalar(decimalFields, "cache_gb", "1e2") &&
           !ModelSettingsSetScalar(decimalFields, "cache_gb", "+1"));
    Json wholeFields{{"context", 32768}, {"parallel", 1}, {"ram_gb", nullptr}};
    assert(ModelSettingsSetScalar(wholeFields, "context", "00131072") && wholeFields["context"] == 131072 &&
           ModelSettingsSetScalar(wholeFields, "ram_gb", "8") && wholeFields["ram_gb"] == 8 &&
           ModelSettingsSetScalar(wholeFields, "ram_gb", "") && wholeFields["ram_gb"].is_null() &&
           !ModelSettingsSetScalar(wholeFields, "ram_gb", "0.5") &&
           !ModelSettingsSetScalar(wholeFields, "parallel", "-1") &&
           !ModelSettingsSetScalar(wholeFields, "parallel", "1.5") &&
           !ModelSettingsSetScalar(wholeFields, "parallel", "1e2"));
    assert(ModelSettingsSetScalar(wholeFields, "context", "\xC2\xA0" "65536" "\xC2\xA0") &&
           wholeFields["context"] == 65536);
    Json gpuSettings{{"gpus", Json::array({0})}, {"context", 131072}};
    assert(ModelSettingsSetGpuIndexes(gpuSettings, "gpus", {2, 0}) &&
           gpuSettings["gpus"] == Json::array({0, 2}) && gpuSettings["context"] == 131072);
    const auto selectedGpus = gpuSettings["gpus"];
    assert(!ModelSettingsSetGpuIndexes(gpuSettings, "gpus", {}) &&
           !ModelSettingsSetGpuIndexes(gpuSettings, "gpus", {0, 0}) &&
           !ModelSettingsSetGpuIndexes(gpuSettings, "gpus", {-1}) &&
           gpuSettings["gpus"] == selectedGpus);
    assert(ModelLeftoverRequest("D:/Maya-data/models/orphan") == Json({{"path", "D:/Maya-data/models/orphan"}}));
    assert(ModelLeftoverDeleteEnabled(true, false, false, false, false) &&
           !ModelLeftoverDeleteEnabled(false, false, false, false, false) &&
           !ModelLeftoverDeleteEnabled(true, true, false, false, false) &&
           !ModelLeftoverDeleteEnabled(true, false, true, false, false) &&
           !ModelLeftoverDeleteEnabled(true, false, false, true, false) &&
           !ModelLeftoverDeleteEnabled(true, false, false, false, true));
    MustThrow([&] { ModelLeftoverRequest(""); }, "Select an unused file");
    assert(ModelLeftoverFreed(Json{{"freed", 1234567}}) == 1234567 &&
           ModelLeftoverFreed(Json{{"freed", -1}}) == 0 && ModelLeftoverFreed(Json(nullptr)) == 0);
    const Json detailModel = {{"model", "deepseek-v4.1-flash"}, {"quant", "IQ4_XS"},
        {"backend", "cuda"}, {"running", false}, {"ok", false}, {"file", "maya-ds4.json"},
        {"path", "C:/Maya/maya-ds4.json"}, {"script", nullptr}, {"folder", "C:/Maya/models/ds4"},
        {"port", 8082}, {"size", 2000000000ULL}, {"written", nullptr}, {"used", 12345.0},
        {"missing", "model files are missing"}, {"settings", {{"context", 131072}}}};
    const auto modelDetails = ModelDetailsReport(detailModel, false);
    const auto modelOverview = ModelOverviewReport(detailModel, false);
    const auto modelEngine = ModelEngineReport(detailModel);
    const auto settingsOverview = ModelOverviewReport(Json{{"model", "Qwen"}, {"settings", {
        {"context", 272000}, {"draft", "mtp"}, {"gpus", Json::array({0, 1})},
        {"images", true}, {"parallel", 3}}}}, false);
    assert(settingsOverview.find("Context: 272k\nDraft: MTP\nGPUs: 0, 1\nImages: on\nAt once: 3\n") !=
           std::string::npos);
    const auto defaultSettingsSummary = "Context: 128K\n";
    const auto firstSettingsSummary = modelDetails.find(defaultSettingsSummary);
    assert(firstSettingsSummary != std::string::npos &&
           modelDetails.find(defaultSettingsSummary, firstSettingsSummary + 1) == std::string::npos);
    const auto missingWarning = modelOverview.find(
        "It can't start: model files are missing. Run START-MAYA.bat --setup to repair it.\n");
    const auto setupFact = modelOverview.find("Setup: ");
    assert(missingWarning != std::string::npos && setupFact != std::string::npos && missingWarning < setupFact);
    const auto runningMissingOverview = ModelOverviewReport(
        Json{{"model", "Qwen"}, {"running", true}, {"missing", "model files are missing"}}, true);
    assert(runningMissingOverview.find("It can't start:") == std::string::npos);
    assert(modelDetails.find("DeepSeek V4.1 Flash  IQ4_XS") != std::string::npos);
    assert(modelDetails.find("Setup: CUDA · 2.0 GB") != std::string::npos &&
           ModelBackendLabel(Json{{"backend", "hip"}}) == "HIP" &&
           ModelBackendLabel(Json{{"backend", "custom"}}) == "custom");
    assert(modelOverview.find("Config: C:/Maya/maya-ds4.json") != std::string::npos &&
           modelOverview.find("Engine settings") == std::string::npos &&
           modelEngine.find("Engine settings") != std::string::npos);
    const auto portFact = modelOverview.find("Port: 8082\n");
    const auto apiModelFact = modelOverview.find("API model name: deepseek-v4.1-flash\n");
    assert(portFact != std::string::npos && apiModelFact != std::string::npos && portFact < apiModelFact);
    const Json falseyPorts[] = {0, false, "", nullptr};
    for (const auto &port : falseyPorts)
        assert(ModelOverviewReport(Json{{"model", "Qwen"}, {"port", port}}, false).find("Port: 8080\n") !=
               std::string::npos);
    assert(ModelOverviewReport(Json{{"model", "Qwen"}}, false).find("Port: 8080\n") != std::string::npos);
    assert(modelDetails.find("Start script: –") != std::string::npos);
    assert(modelDetails.find("Model folder: C:/Maya/models/ds4") != std::string::npos);
    const auto missingFolderDetails = ModelDetailsReport(Json{{"model", "Qwen"}, {"quant", "Q4"}}, false);
    assert(missingFolderDetails.find("Model folder: –") != std::string::npos);
    assert(ModelDetailsStartScript(Json{{"script", "start-maya.sh"}}) == "./start-maya.sh");
    assert(ModelDetailsStartScript(Json{{"script", ""}}) == "–");
    assert(ModelDetailsStartScript(Json{{"script", false}}) == "–" &&
           ModelDetailsStartScript(Json{{"script", 0}}) == "–");
    assert(ModelOverviewReport(Json{{"model", "Qwen"}, {"folder", ""}}, false).find("Model folder: –\n") !=
               std::string::npos &&
           ModelOverviewReport(Json{{"model", "Qwen"}, {"folder", false}}, false).find("Model folder: –\n") !=
               std::string::npos &&
           ModelDetailsTruthy(Json::array()) && ModelDetailsTruthy(Json::object()));
    assert(ModelSheetNextTab(0, 1, true) == 1 && ModelSheetNextTab(2, 1, true) == 0 &&
           ModelSheetNextTab(0, 1, false) == 2 && ModelSheetNextTab(2, -1, false) == 0);
    const auto detailModelList = ModelListLabel(detailModel, false);
    assert(detailModelList.rfind("DeepSeek V4.1 Flash  IQ4_XS  ·  CUDA · 2.0 GB", 0) == 0 &&
           detailModelList.find("  ·  last used ") != std::string::npos &&
           detailModelList.find("  [INCOMPLETE]") != std::string::npos);
    assert(ModelListLabel({{"model", "Qwen"}, {"quant", "Q4"}, {"ok", false},
                           {"missing", "its model files are missing"}}, false) ==
           "Qwen  Q4  ·  CUDA  [FILES MISSING]");
    const auto modelListNow = std::time(nullptr);
    const auto modelListUsed = static_cast<double>(modelListNow - 8 * 24 * 60 * 60);
    const auto expectedModelListLastUsed = ModelLastUsed({{"used", modelListUsed}}, modelListNow,
                                                          LOCALE_NAME_USER_DEFAULT);
    const auto usedModelList = ModelListLabel({{"model", "Qwen"}, {"quant", "Q4"}, {"ok", true},
                                                {"used", modelListUsed}}, false);
    assert(expectedModelListLastUsed != "not set" && expectedModelListLastUsed != "not yet" &&
           usedModelList.find("  ·  last used " + expectedModelListLastUsed) != std::string::npos);
    const auto neverUsedModelList = ModelListLabel({{"model", "Qwen"}, {"quant", "Q4"}, {"ok", true}}, false);
    assert(neverUsedModelList.find("last used") == std::string::npos &&
           neverUsedModelList.find("running now") == std::string::npos);
    assert(ModelListLabel({{"model", "deepseek-v4.1-flash"}, {"quant", "IQ4"}, {"running", true}, {"ok", false}}, true) ==
           "DeepSeek V4.1 Flash  IQ4  ·  CUDA  ·  running now  [RUNNING]");
    assert(NativeModelListDetailsKey(WM_KEYDOWN, VK_RETURN) &&
           !NativeModelListDetailsKey(WM_KEYDOWN, VK_ESCAPE) &&
           !NativeModelListDetailsKey(WM_CHAR, VK_RETURN));
    assert(ModelCardAbout(detailModel, L"en-US") == "CUDA · 2.0 GB" &&
           ModelCardAbout({{"backend", "hip"}, {"size", 99000000}}, L"en-US") == "HIP" &&
           ModelCardAbout({{"backend", "cuda"}, {"size", 2000000000ULL}, {"written", "2026-11-01 23:07"}},
                          L"en-US") == "CUDA · 2.0 GB · set up Nov 1");
    std::ostringstream roundedModelSize;
    try { roundedModelSize.imbue(std::locale("")); }
    catch (...) {}
    roundedModelSize << std::fixed << std::setprecision(1) << 1.3;
    assert(ModelCardAbout(Json{{"size", 1250000000ULL}}, L"en-US").find(roundedModelSize.str() + " GB") !=
               std::string::npos &&
           ModelCardAbout(Json{{"size", 10500000000ULL}}, L"en-US").find("11 GB") != std::string::npos);
    assert(ModelDisplayName("cyber-frost-3.8-ps") == "Qwen-3.8-Flash-Next (CYBER-FROST)");
    assert(ModelDisplayName("CYBER-FROST-3.8-PS") == "Qwen-3.8-Flash-Next (CYBER-FROST)");
    assert(ModelSettingsButtonEnabled(Json{{"ok", true}}));
    assert(!ModelSettingsButtonEnabled(Json{{"ok", false}}));
    assert(!ModelSettingsButtonEnabled(Json::object()));
    assert(ModelStatusLabel({{"running", true}}, true) == "running" &&
           ModelStatusLabel({{"running", true}}, false) == "unloaded" &&
           ModelStatusLabel({{"running", false}}, true) == "installed" &&
           ModelStatusLabel({{"running", false}, {"ok", false}, {"missing", "its model files are missing"}}, false) == "files missing" &&
           ModelStatusLabel({{"running", false}, {"ok", false}, {"missing", "backend unavailable"}}, false) == "incomplete" &&
           ModelStatusLabel(Json(nullptr), false) == "unknown");
    assert(modelDetails.find("Model files size: 2.0 GB") != std::string::npos);
    assert(modelDetails.find("It can't start: model files are missing. Run START-MAYA.bat --setup to repair it.") !=
           std::string::npos);
    assert(ModelSetupDate({{"written", "2026-11-01 23:07"}}, L"en-US") == "Nov 1");
    const Json falseyWritten[] = {0, false, "", nullptr};
    for (const auto &written : falseyWritten)
        assert(ModelSetupDate(Json{{"written", written}}, L"en-US") == "not set");
    const char *invalidWritten[] = {"not-a-date", "2026-02-30 23:07", "2026-11-01 24:07",
                                    "2026-11-01 23:07trailing"};
    for (const auto *written : invalidWritten)
        assert(ModelSetupDate(Json{{"written", written}}, L"en-US") == "not set");
    assert(ModelCardAbout(Json{{"backend", "cuda"}, {"written", false}}, L"en-US") == "CUDA" &&
           ModelCardAbout(Json{{"backend", "cuda"}, {"written", "not-a-date"}}, L"en-US") == "CUDA");
    const auto now = std::time(nullptr);
    std::tm nowLocal{};
    assert(localtime_s(&nowLocal, &now) == 0);
    SYSTEMTIME modelToday{};
    modelToday.wHour = static_cast<WORD>(nowLocal.tm_hour);
    modelToday.wMinute = static_cast<WORD>(nowLocal.tm_min);
    modelToday.wSecond = static_cast<WORD>(nowLocal.tm_sec);
    wchar_t modelTodayText[128]{};
    const int modelTodayLength = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, TIME_NOSECONDS, &modelToday, nullptr,
                                                  modelTodayText, _countof(modelTodayText));
    assert(modelTodayLength > 1 && ModelLastUsed({{"used", static_cast<double>(now)}}, now) ==
           Utf8(std::wstring(modelTodayText, modelTodayLength - 1)));
    assert(ModelRelativeTime(static_cast<double>(now), now) == Utf8(std::wstring(modelTodayText, modelTodayLength - 1)));
    assert(ModelRelativeTime(static_cast<double>(std::numeric_limits<std::time_t>::max()), now) == "not set");
    auto yesterday = now;
    std::tm yesterdayLocal{};
    assert(localtime_s(&yesterdayLocal, &yesterday) == 0);
    --yesterdayLocal.tm_mday;
    yesterdayLocal.tm_hour = 12;
    yesterdayLocal.tm_min = yesterdayLocal.tm_sec = 0;
    yesterday = std::mktime(&yesterdayLocal);
    assert(yesterday != static_cast<std::time_t>(-1));
    assert(ModelLastUsed({{"used", static_cast<double>(yesterday)}}, now) == "Yesterday");
    assert(ModelOverviewReport(Json{{"model", "Qwen"}, {"running", true}, {"used", static_cast<double>(now)}}, true)
               .find("Last used: running now\n") != std::string::npos);
    assert(ModelRelativeTime(static_cast<double>(yesterday), now) == "Yesterday");
    assert(ModelLastUsed(Json::object(), now) == "not yet" && ModelLastUsed({{"used", 0.0}}, now) == "not yet" &&
           ModelOverviewReport(Json{{"model", "Qwen"}}, false).find("Last used: not yet\n") != std::string::npos);
    const Json modelSettings = {{"context", 272000}, {"draft", "none"}, {"gpus", Json::array({0, 1})},
                                {"images", true}, {"parallel", 3}};
    assert(ModelSettingsSummary(modelSettings) == "Context: 272k\nDraft: off\nGPUs: 0, 1\nImages: on\nAt once: 3\n");
    assert(ModelContextLabel(131072) == "128K" && ModelContextLabel(1048576) == "1M" &&
           ModelContextLabel(272000) == "272k");
    assert(ModelSettingsChoiceLabel("context", 272000) == "272k");
    assert(ModelSettingsChoiceLabel("draft", "dspark") == "DSpark" &&
           ModelSettingsChoiceLabel("draft", "none") == "Off" &&
           ModelSettingsChoiceLabel("draft", "dflash2") == "DFlash2");
    assert(ModelSettingsChoiceLabel("mmap", "on") == "From the files" &&
           ModelSettingsChoiceLabel("mmap", "off") == "In RAM" &&
           ModelSettingsChoiceLabel("tp", 0) == "Off");
    assert(ModelSettingsChoiceLabel("kv_cache", "bf16") == "BF16" &&
           ModelSettingsChoiceLabel("kv", "k8v4") == "K8 V4" &&
           ModelSettingsChoiceLabel("custom", "raw") == "raw");
    const auto chatMeta = ParseChatMetadata(
        SerializeChatMetadata({"Named chat", "Be concise", "folder-1", "max", "deepseek-v4.1-flash", true, "job-123", true}));
    assert(chatMeta.title == "Named chat" && chatMeta.system == "Be concise" && chatMeta.folder == "folder-1" &&
           chatMeta.thinking == "max" && chatMeta.thinkingFor == "deepseek-v4.1-flash" && chatMeta.pinned &&
           chatMeta.pendingJob == "job-123" && chatMeta.titleManual);
    assert(!ParseChatMetadata(Json{{"title", "legacy"}}).titleManual);
    ChatMetadata ablationMeta;
    ablationMeta.hasAblate = true;
    ablationMeta.ablate = true;
    ablationMeta.ablateFor = "deepseek-v4.1-flash|label";
    ablationMeta.thinkingPicked = true;
    const auto ablationRoundTrip = ParseChatMetadata(SerializeChatMetadata(ablationMeta));
    assert(ablationRoundTrip.hasAblate && ablationRoundTrip.ablate &&
           ablationRoundTrip.ablateFor == "deepseek-v4.1-flash|label" && ablationRoundTrip.thinkingPicked);
    assert(!SerializeChatMetadata(ChatMetadata{}).contains("ablate"));
    assert(!SerializeChatMetadata(ChatMetadata{}).contains("thinkingPicked"));
    const Json ablationHealth{{"ablate", {{"available", true}, {"default", false}}}};
    const Json noAblationHealth{{"ablate", {{"available", false}, {"default", true}}}};
    assert(NativeChatAblationAvailable(ablationHealth) && !NativeChatAblationAvailable(noAblationHealth));
    assert(NativeChatAblationRequestValue(ablationHealth, "model-a", false, false, "").is_boolean() &&
           !NativeChatAblationRequestValue(ablationHealth, "model-a", false, false, "").get<bool>());
    assert(NativeChatAblationRequestValue(ablationHealth, "model-a", true, true, "model-a").get<bool>());
    assert(!NativeChatAblationRequestValue(ablationHealth, "model-a", true, true, "model-b").get<bool>());
    assert(NativeChatAblationRequestValue(noAblationHealth, "model-a", false, false, "").is_null());
    Json ablationRequest = Json::object();
    AddNativeChatAblation(ablationRequest, ablationHealth, "model-a", true, true, "model-a");
    assert(ablationRequest.value("ablate", false));
    Json unavailableRequest = Json::object();
    AddNativeChatAblation(unavailableRequest, noAblationHealth, "model-a", false, false, "");
    assert(!unavailableRequest.contains("ablate"));
    assert(NativeChatTitleIsAuto(L"New chat", false, L"question"));
    assert(NativeChatTitleIsAuto(std::wstring(80, L'x'), false, std::wstring(81, L'x')));
    assert(!NativeChatTitleIsAuto(std::wstring(80, L'x'), true, std::wstring(81, L'x')));
    assert(!NativeChatTitleIsAuto(L"Custom", false, L"question"));
    assert(ChatTitleFrom(L"  first\t message\n next  ", {}) == L"first message next" &&
           ChatTitleFrom(L"", {L"photo.png", L"notes.txt"}) == L"photo.png, notes.txt" &&
           ChatTitleFrom(std::wstring(61, L'x'), {}).size() == 60 && ChatTitleFrom(L" \t ", {}) == L"New chat");
    assert(TrimAndLimitName(L" \u00A0Project  Maya\t ", 60) == L"Project  Maya" &&
           TrimAndLimitName(std::wstring(81, L'x'), 80).size() == 80 && TrimAndLimitName(L" \t\u00A0", 60).empty());
    assert(TrimChatInput(L"\t \u00A0Maya\u3000\n") == L"Maya");
    assert(TrimChatInput(L"\uFEFF text \u2028") == L"text");
    assert(TrimChatInput(L"\u0085text\u0085") == L"\u0085text\u0085");
    assert(NativeChatSearchQuery(L" \t Search me \u00A0") == L"Search me" &&
           NativeChatSearchQuery(std::wstring(160, L'x')).size() == 160);
    assert(NativeChatSearchClearOnSelect(L" matching ") && !NativeChatSearchClearOnSelect(L" \t "));
    assert(NativeChatSearchEnter(true) == NativeChatSearchEnterAction::OPEN_SELECTED &&
           NativeChatSearchEnter(false) == NativeChatSearchEnterAction::NONE);
    const std::vector<bool> selectableSearchRows{false, true, true, false, true};
    const auto searchRowSelectable = [&](int row) { return selectableSearchRows[row]; };
    assert(NativeChatSearchMoveSelection(-1, 5, true, searchRowSelectable) == 1 &&
           NativeChatSearchMoveSelection(1, 5, true, searchRowSelectable) == 2 &&
           NativeChatSearchMoveSelection(2, 5, true, searchRowSelectable) == 4 &&
           NativeChatSearchMoveSelection(4, 5, true, searchRowSelectable) == 4);
    assert(NativeChatSearchMoveSelection(4, 5, false, searchRowSelectable) == 2 &&
           NativeChatSearchMoveSelection(2, 5, false, searchRowSelectable) == 1 &&
           NativeChatSearchMoveSelection(1, 5, false, searchRowSelectable) == 1);
    assert(NativeChatSearchMoveSelection(0, 5, true, searchRowSelectable) == 1 &&
           NativeChatSearchMoveSelection(3, 5, false, searchRowSelectable) == 2 &&
           NativeChatSearchMoveSelection(-1, 0, true, searchRowSelectable) == -1);
    assert((NativeChatSearchLimitedOrder({4, 1, 3, 2, 0}, {true, true, true, true, true}) ==
            std::vector<int>{4, 1, 3, 2, 0}));
    std::vector<int> manySearchMatches(55);
    std::vector<bool> manySearchFlags(55, true);
    for (size_t i = 0; i < manySearchMatches.size(); ++i) manySearchMatches[i] = (int)i;
    const auto limitedSearch = NativeChatSearchLimitedOrder(manySearchMatches, manySearchFlags);
    assert(limitedSearch.size() == 50 && limitedSearch.front() == 0 && limitedSearch.back() == 49);
    assert(NativeChatSearchText(L"  first\tline\r\nsecond  ") == L"first line second");
    assert(NativeChatSearchPreview(L"same\t title", L"title", L"same title").empty());
    assert(NativeChatSearchFind(L"\u0218\u021A\u0102\u00CE\u00C2", L"\u0219\u021B\u0103\u00EE\u00E2") == 0);
    assert(NativeChatSearchFind(L"matching", L"absent") == std::wstring::npos);
    const auto romanianPreview = NativeChatSearchPreview(L"Temperatura: " L"\u0218" L"AN" L"\u021A",
                                                         L"\u0219" L"an" L"\u021B", L"Different title");
    assert(romanianPreview.find(L"\u0218" L"AN" L"\u021A") != std::wstring::npos);
    const auto searchPreview = NativeChatSearchPreview(std::wstring(40, L'x') + std::wstring(160, L'a'),
                                                      L"a", L"Different title");
    assert(searchPreview.size() == 121 && searchPreview.front() == L'…' && searchPreview.back() == L'a');
    assert(NativeChatAssistantPreview(L"## First **answer**\n- with `code`") == L"First answer with code");
    assert(NativeChatAssistantPreview(std::wstring(100, L'x')).size() == 90);
    assert(ThinkingForModel(chatMeta, "deepseek-v4.1-flash") == "max");
    assert(ThinkingForModel(chatMeta, "other-model") == "high");
    assert(FormatContextMeter(12345, 272000, false) == "Context: 12,345 / 272,000 (4.5%)");
    assert(!NativeContextMeterVisible(true, false, true, true));
    assert(!NativeContextMeterVisible(true, true, false, true));
    assert(!NativeContextMeterVisible(true, true, true, false));
    assert(NativeContextMeterVisible(true, true, true, true));
    assert(!NativeContextMeterVisible(false, true, true, true));
    assert(NativeContextMeterCountMatches(L"chat-a", L"chat-a"));
    assert(!NativeContextMeterCountMatches(L"chat-b", L"chat-a"));
    assert(!NativeContextMeterCountMatches(L"", L"chat-a"));
    assert(ContextMeterNeedsRefresh(true, 272000, 272000) &&
           ContextMeterNeedsRefresh(false, 272000, 131072) &&
           !ContextMeterNeedsRefresh(false, 272000, 272000) &&
           ContextMeterNeedsRefresh(false, 272000, 272000, true));
    assert(FormatContextMeter(300, 272, false) == "Context: 300 / 272 (100.0%)");
    assert(FormatContextMeter(12, 0, true) == "~Context: 12");
    const Json liveContextMetrics{{"engine", {{"max_context", 272000}}},
        {"live", {{"state", "generating"}, {"prompt_tokens", 900}, {"prompt_reused", 600},
                   {"generated", 100}, {"tok_s", 4.5}, {"queued", 0}}}};
    const auto liveContext = MonitorContextUsageValues(liveContextMetrics);
    assert(liveContext.live && liveContext.used == 1000 && liveContext.reused == 600 &&
           liveContext.fresh == 300 && liveContext.written == 100);
    const auto windowContext = MonitorContextUsageValues(
        {{"engine", {{"max_context", 272000}, {"kv_resident", 131072}}}});
    assert(windowContext.window == 131072);
    assert(MonitorContextUsageValues({{"engine", {{"max_context", 272000}, {"kv_resident", 0}}}}).window == 0);
    assert(MonitorContextUsageValues({{"engine", {{"max_context", 272000}, {"kv_resident", 272000}}}}).window == 0);
    assert(MonitorContextUsageValues({{"engine", {{"max_context", 272000}, {"kv_resident", 300000}}}}).window == 0);
    assert(MonitorContextUsageValues({{"engine", {{"max_context", 0}, {"kv_resident", 131072}}}}).window == 0);
    const Json idleContextMetrics{{"engine", {{"max_context", 272000}}}, {"live", {{"state", "idle"}}},
        {"requests", Json::array({{{"prompt_tokens", 1100}, {"reused", 900}, {"output_tokens", 300}}})}};
    const auto idleContext = MonitorContextUsageValues(idleContextMetrics);
    assert(!idleContext.live && idleContext.used == 1400 && idleContext.reused == 900 &&
           idleContext.fresh == 200 && idleContext.written == 300);
    const auto monitor = MonitorReport({{"engine", {{"max_context", 272000}}},
                                        {"live", {{"state", "generating"}, {"prompt_tokens", 900},
                                                   {"prompt_reused", 600}, {"generated", 100}, {"tok_s", 4.5}, {"queued", 0}}},
                                        {"tiers", {{"now", {{"vram_hit", 0.25}, {"ms_tok", 222.0}}}}},
                                        {"totals", {{"requests", 7}, {"peak_decode", {{"tok_s", 10.7}}}}},
                                        {"requests", Json::array({{{"finish", "done"}, {"output_tokens", 100}}})}});
    assert(monitor.find(L"Context used: 1000 / 272000 tokens") != std::wstring::npos);
    assert(monitor.find(L"Prompt: 900 tokens (600 reused from cache, 300 read new) · Written: 100 tokens · Free: 271000 tokens") != std::wstring::npos);
    const auto windowReport = MonitorReport({{"engine", {{"max_context", 272000}, {"kv_resident", 131072}}},
        {"live", {{"state", "generating"}, {"prompt_tokens", 900}, {"prompt_reused", 600}, {"generated", 100}}}});
    assert(windowReport.find(L"VRAM window: 131072 / 272000 tokens") != std::wstring::npos &&
           windowReport.find(L"Change it in Models > Engine settings > Prompts and cache") != std::wstring::npos);
    assert(monitor.find(L"VRAM hit: 25.0%") != std::wstring::npos);
    assert(monitor.find(L"Peak: 10.7 tok/s   Requests: 7") != std::wstring::npos);
    assert(monitor.find(L"Speculation: not reported") != std::wstring::npos);
    assert(MonitorReport(idleContextMetrics).find(L"Context used: 1400 / 272000 tokens") != std::wstring::npos);
    assert(MonitorReport(idleContextMetrics).find(L"900 reused from cache, 200 read new") != std::wstring::npos);
    const Json noReuseMetrics{{"engine", {{"max_context", 272000}}},
        {"live", {{"state", "reading"}, {"prompt_tokens", 300}}}};
    assert(!MonitorContextUsageValues(noReuseMetrics).reusedReported &&
           MonitorReport(noReuseMetrics).find(L"cache reuse not reported, 300 read new") != std::wstring::npos);
    const Json monitorStatsFallback = {
        {"live", {{"state", "idle"}, {"active", 0}}},
        {"requests", Json::array({{{"decode_tok_s", 20.0}, {"hit_rate", 0.75}}})},
        {"tiers", {{"now", {{"time", 969}, {"ms_tok", 12.5}, {"vram_hit", 0.25}}}}}};
    assert(MonitorTimePerTokenValue(monitorStatsFallback) == L"50.0" &&
           MonitorVramHitValue(monitorStatsFallback, 1000.0) == L"75.0%");
    auto currentMonitorStats = monitorStatsFallback;
    currentMonitorStats["live"] = {{"state", "generating"}, {"active", 1}, {"tok_s", 4.0}};
    currentMonitorStats["tiers"]["now"] = {{"time", 1000}, {"ms_tok", 175.0}, {"vram_hit", 0.25}};
    assert(MonitorTimePerTokenValue(currentMonitorStats) == L"175.0" &&
           MonitorVramHitValue(currentMonitorStats, 1000.0) == L"25.0%" &&
           MonitorVramHitTierFresh(currentMonitorStats, 1030.0) &&
           !MonitorVramHitTierFresh(currentMonitorStats, 1031.0));
    currentMonitorStats["tiers"]["now"].erase("ms_tok");
    assert(MonitorTimePerTokenValue(currentMonitorStats) == L"250.0");
    const Json parallelMonitorStats = {
        {"live", {{"state", "generating"}, {"active", 2}, {"tok_s", 8.0}}},
        {"history", {{"tok_s", Json::array({3.26})}}}};
    assert(MonitorHeroValue(parallelMonitorStats, Json::object(), "decode") == L"3.3" &&
           MonitorTimePerTokenValue(parallelMonitorStats) == L"306.7");
    const Json noMonitorStats = {{"live", {{"state", "idle"}}}};
    assert(MonitorTimePerTokenValue(noMonitorStats) == L"—" &&
           MonitorVramHitValue(noMonitorStats, 1000.0) == L"—");
    const auto malformedMonitor = MonitorReport({{"engine", {{"max_context", nullptr}}},
        {"live", {{"state", "idle"}, {"prompt_tokens", nullptr}, {"generated", "unknown"}}},
        {"totals", {{"prompt_tokens", nullptr}, {"reused", "unknown"}, {"output_tokens", nullptr}}},
        {"requests_kept", nullptr}, {"requests", Json::array({{{"reused", nullptr}}})}});
    assert(malformedMonitor.find(L"Context used: —") != std::wstring::npos &&
           malformedMonitor.find(L"reused —") != std::wstring::npos);
    const Json monitorTotals = {{"since", 1000}, {"requests", 4}, {"prompt_tokens", 800}, {"reused", 100},
        {"output_tokens", 200}, {"prompt_ms", 3500.0}, {"decode_ms", 10000.0}};
    const Json monitorGuard = {{"on", true}, {"detected", 2}, {"penalised", 1}, {"stopped", 1},
        {"tool_repeats", 1}, {"tool_blocked", 1}, {"kept_going", 2}, {"collapsed", 3}};
    const auto totalsLine = MonitorTotalsLine(monitorTotals, monitorGuard);
    assert(totalsLine.find(L"700 prompt tokens read at 200 tok/s (100 reused) · 200 written at 20.0 tok/s") != std::wstring::npos &&
           totalsLine.find(L"2 loops seen (1 cut with a short penalty, 1 ended the answer), 1 repeated tool call noted, 1 blocked, 2 kept going, 3 prompts without the repeats") != std::wstring::npos);
    assert(MonitorTotalsLine({{"requests", 0}}, Json::object()).empty() &&
           MonitorTotalsLine({{"requests", 3}}, Json::object()).empty() &&
           MonitorTotalsLine(monitorTotals, {{"on", false}}).find(L"loop guard off") != std::wstring::npos);
    const auto summaryReport = MonitorReport({{"totals", monitorTotals}, {"loop_guard", monitorGuard}});
    assert(summaryReport.find(totalsLine) != std::wstring::npos);
    assert(MonitorSpeculationText({{"engine", {{"speculation", Json::array()}}}}) == L"off");
    assert(MonitorSpeculationText({{"engine", {{"speculation", Json::array({"MTP"})}}}}, 1000) ==
           L"MTP · waiting for drafts");
    const Json liveDrafting = {
        {"engine", {{"speculation", Json::array({"MTP"})}}},
        {"live", {{"state", "generating"}, {"elapsed_s", 10}}},
        {"tiers", {{"now", {{"time", 995}, {"draft_acc", 124}, {"draft_off", 200}}}}}};
    assert(MonitorSpeculationText(liveDrafting, 1000) == L"MTP · 62% (124 of 200 drafts kept)");
    auto staleDrafting = liveDrafting;
    staleDrafting["tiers"]["now"]["time"] = 900;
    assert(MonitorSpeculationText(staleDrafting, 1000) == L"MTP · waiting for drafts");
    auto disabledDrafting = liveDrafting;
    disabledDrafting["live"]["spec_off"] = true;
    assert(MonitorSpeculationText(disabledDrafting, 1000) == L"off");
    const Json completedDrafting = {{"engine", {{"speculation", Json::array({"DSpark", "Lookup"})}}},
        {"requests", Json::array({{{"drafts_accepted", 30}, {"drafts_offered", 50}}})}};
    assert(MonitorSpeculationText(completedDrafting, 1000) == L"DSpark · 60% (30 of 50 drafts kept)");
    const Json lookupOnly = {{"engine", {{"speculation", Json::array({"Lookup"})}}}};
    assert(MonitorSpeculationText(lookupOnly, 1000) == L"Lookup · waiting for drafts");
    const Json completedWithoutDrafting = {{"engine", {{"speculation", Json::array({"MTP"})}}},
        {"requests", Json::array({{{"spec_off", true}}})}};
    assert(MonitorSpeculationText(completedWithoutDrafting, 1000) == L"off");
    const Json undocumentedDrafting = {{"engine", {{"speculation", Json::array({"MTP"})}}},
        {"live", {{"state", "generating"}}}, {"tiers", {{"now", {{"drafts", 0.5}, {"time", 999}}}}}};
    assert(MonitorSpeculationText(undocumentedDrafting, 1000) == L"MTP · 50%");
    const auto monitorAlerts = MonitorReport({
        {"reload", {{"state", "waiting"}, {"to", 272000}}},
        {"engine", {{"gpu_commit_capped_gb", 12.5}, {"pagefile_hint_gb", 96.0}}},
        {"hardware", {{"gpus", Json::array({{{"index", 1}, {"temp", 89.0}}})}}}});
    assert(monitorAlerts.find(L"Reloading the model with 272k context") != std::wstring::npos &&
           monitorAlerts.find(L"12.5 GB of experts are staying off the GPU") != std::wstring::npos &&
           monitorAlerts.find(L"Suggested page file: 96.0 GB") != std::wstring::npos &&
           monitorAlerts.find(L"GPU 1 is at 89 °C") != std::wstring::npos);
    const auto monitorBanners = MonitorBanners({
        {"reload", {{"state", "waiting"}, {"to", 272000}}},
        {"engine", {{"gpu_commit_capped_gb", 12.5}, {"pagefile_hint_gb", 96.0}}},
        {"hardware", {{"gpus", Json::array({{{"index", 1}, {"temp", 89.0}}})}}}});
    assert(monitorBanners.size() == 3 && monitorBanners[0].tone == MonitorBannerTone::info &&
           monitorBanners[1].tone == MonitorBannerTone::warning &&
           monitorBanners[2].tone == MonitorBannerTone::warning && !monitorBanners[2].reportTag);
    const auto reloadFailure = MonitorReport({{"reload", {{"state", "failed"}, {"ended", std::time(nullptr)},
        {"error", "fixture failure"}, {"context", 32768}}}});
    assert(reloadFailure.find(L"The context change failed: fixture failure") != std::wstring::npos &&
           reloadFailure.find(L"running again with 32K context") != std::wstring::npos &&
           MonitorWarnings({{"reload", {{"state", "failed"}, {"ended", std::time(nullptr) - 601}}}}).empty());
    assert(MonitorBanners({{"reload", {{"state", "failed"}, {"ended", std::time(nullptr)},
        {"error", "fixture failure"}, {"context", 32768}}}}).front().tone == MonitorBannerTone::danger);
    const auto emptyGpuRows = MonitorBanners({{"hardware", {{"gpus", Json::array()}, {"gpu_temp", 92.0}}}});
    assert(emptyGpuRows.empty() && MonitorWarnings({{"hardware", {{"gpus", Json::array()}, {"gpu_temp", 92.0}}}}).empty());
    const auto missingGpuRows = MonitorBanners({{"hardware", {{"gpu_temp", 92.0}}}});
    const auto nullGpuRows = MonitorBanners({{"hardware", {{"gpus", nullptr}, {"gpu_temp", 92.0}}}});
    assert(missingGpuRows.size() == 1 && nullGpuRows.size() == 1 &&
           missingGpuRows[0].text.find(L"GPU is at 92 °C") != std::wstring::npos);
    assert(MonitorContextSizeLabel(131072) == L"128K" && MonitorContextSizeLabel(1048576) == L"1M" &&
           MonitorContextSizeLabel(1000) == L"1.0k" && MonitorContextSizeLabel(123) == L"123");
    assert(MonitorWarnings({{"reload", nullptr}, {"engine", nullptr}, {"hardware", nullptr}}).empty());
    assert(MonitorGpuTemperatureLevel({{"temp", 79.9}}) == MonitorGpuTemperatureTone::normal);
    assert(MonitorGpuTemperatureLevel({{"temp", 80.0}}) == MonitorGpuTemperatureTone::warm);
    assert(MonitorGpuTemperatureLevel({{"temp", 86.9}}) == MonitorGpuTemperatureTone::warm);
    assert(MonitorGpuTemperatureLevel({{"temp", 87.0}}) == MonitorGpuTemperatureTone::hot);
    assert(MonitorGpuTemperatureLevel({{"temp", nullptr}}) == MonitorGpuTemperatureTone::normal);
    Json requestRows = Json::array();
    for (int i = 0; i < 13; ++i)
        requestRows.push_back({{"finish", "request-" + std::to_string(i)}, {"prompt_tokens", i},
                               {"output_tokens", i + 1}, {"decode_tok_s", 2.0}});
    const Json requestMetrics{{"requests", requestRows}, {"requests_kept", 13}};
    assert(MonitorReport(requestMetrics).find(L"request-11") != std::wstring::npos &&
           MonitorReport(requestMetrics).find(L"request-12") == std::wstring::npos);
    assert(MonitorReport(requestMetrics, true).find(L"request-12") != std::wstring::npos);
    const Json monitorMetrics = {
        {"engine", {{"engine_kind", "ds4"}, {"experts", 100}, {"cpu_experts", 40}, {"max_context", 131072}}},
        {"tiers", {{"now", {{"vram_hit", 0.75}, {"vram_used", 60}, {"ram_used", 25}, {"vram_gb", 12.0},
                               {"ram_gb", 8.0}, {"disk_gb", 2.0}, {"ram_fetch", 1.25}, {"disk", 0.5},
                               {"promo", 2.0}, {"engram_ms", 0.3}, {"eg_cache_rows", 80}, {"eg_cache_cap", 100},
                               {"eg_lookup_rows", 100}, {"eg_lookup_hits", 75}, {"eg_lookup_reads", 25}}}}},
        {"gpu_map", Json::array({{{"index", 0}, {"name", "RTX fixture"}, {"total", 8ULL * 1024 * 1024 * 1024},
                                  {"free", 2ULL * 1024 * 1024 * 1024}, {"measured", "nvml"},
                                  {"parts", {{"weights", 4ULL * 1024 * 1024 * 1024}, {"vision", 1ULL * 1024 * 1024 * 1024}}}}})},
        {"hardware", {{"gpus", Json::array({{{"index", 0}, {"util", 50}, {"temp", 60}, {"power", 120},
                                                  {"pcie_gen_max", 4}, {"pcie_width", 16}, {"pcie_rx_mb", 2.5}}})}}}};
    const auto monitorDetail = MonitorReport(monitorMetrics);
    auto highPcieMetrics = monitorMetrics;
    highPcieMetrics["hardware"]["gpus"][0]["pcie_rx_mb"] = 1536.0;
    const auto highPcieReport = MonitorReport(highPcieMetrics);
    const auto pcieReportAt = highPcieReport.find(L"PCIe Gen4 x16");
    assert(pcieReportAt != std::wstring::npos &&
           highPcieReport.find(L"1.5 GB/s", pcieReportAt) != std::wstring::npos);
    assert(monitorDetail.find(L"ENGINE EXPERT COUNTERS (ds4") != std::wstring::npos);
    assert(monitorDetail.find(L"From RAM: 1.25 /tok") != std::wstring::npos);
    assert(monitorDetail.find(L"From SSD:") != std::wstring::npos);
    assert(monitorDetail.find(L"75% from RAM cache (80 of 100 rows held; 100 lookups, 75 RAM hits, 25 SSD reads)") != std::wstring::npos);
    assert(monitorDetail.find(L"GPU 0 · RTX fixture · 6.0 GB used / 8.0 GB total · current reading") != std::wstring::npos);
    assert(monitorDetail.find(L"weights: 4.0 GB") != std::wstring::npos &&
           monitorDetail.find(L"vision: 1.0 GB") != std::wstring::npos &&
           monitorDetail.find(L"Sensors: 50% load · 60 °C · 120.0 W · PCIe Gen4 x16 · 2.5 MB/s") != std::wstring::npos);
    const Json qwenTierMetrics{{"engine", {{"engine_kind", "qwen"}, {"experts", 10}}},
        {"tiers", {{"now", {{"vram_used", 5}, {"ram_used", 4}, {"vram_gb", 8.0}, {"ram_gb", 16.0},
                              {"disk_gb", 2.0}, {"ram_fetch", 1.0}, {"disk", 3.5}, {"disk_mb", 1.25}}}}}};
    const auto qwenExpertReport = MonitorExpertReport(qwenTierMetrics);
    assert(MonitorExpertFileReadLabel(qwenTierMetrics) == L"model file" &&
           qwenExpertReport.find(L"From model file: 3.50 /tok") != std::wstring::npos &&
           qwenExpertReport.find(L"Model-file reads: 1.25 MB/tok") != std::wstring::npos &&
           qwenExpertReport.find(L"more RAM, a smaller model or a shorter context") != std::wstring::npos);
    auto qwenNoSsdExperts = qwenTierMetrics;
    qwenNoSsdExperts["engine"]["experts"] = 9;
    assert(MonitorExpertReport(qwenNoSsdExperts).find(L"more RAM, a smaller model") == std::wstring::npos);
    const Json gpuDetails{{"engine", {{"engine_kind", "ds4"}, {"max_context", 272000}, {"kv_resident", 131072}}},
        {"gpu_map", Json::array({Json{{"index", 0}, {"total", 16ULL * 1024 * 1024 * 1024},
            {"free", 1ULL * 1024 * 1024 * 1024}, {"layers", Json::array({0, 45})}, {"ctx", 272000},
            {"kv_ram", 8ULL * 1024 * 1024 * 1024}, {"tp_n", 2}, {"slots", 320},
            {"borrow", 1ULL * 1024 * 1024 * 1024}, {"drafter", 2ULL * 1024 * 1024 * 1024},
            {"window", 512ULL * 1024 * 1024}, {"vision_lent", true}, {"vision_own", 0},
            {"parts", {{"weights", 1ULL * 1024 * 1024 * 1024}, {"kv", 1ULL * 1024 * 1024 * 1024},
                        {"tp", 1ULL * 1024 * 1024 * 1024}, {"drafter", 1ULL * 1024 * 1024 * 1024},
                        {"pool", 1ULL * 1024 * 1024 * 1024}, {"helper", 1ULL * 1024 * 1024 * 1024},
                        {"vision", 1ULL * 1024 * 1024 * 1024}}}}})}};
    const auto gpuDetailsText = MonitorGpuMemoryReport(gpuDetails);
    assert(gpuDetailsText.find(L"layers 0–45") != std::wstring::npos &&
           gpuDetailsText.find(L"131072 of 272000 positions here, the rest in RAM") != std::wstring::npos &&
           gpuDetailsText.find(L"8.0 GB pinned in RAM") != std::wstring::npos &&
           gpuDetailsText.find(L"320 expert slots · prompts borrow up to 1.0 GB") != std::wstring::npos &&
           gpuDetailsText.find(L"verify window 0.5 GB") != std::wstring::npos &&
           gpuDetailsText.find(L"expert-pool tail lent to the encoder") != std::wstring::npos);
    const auto gpuDetailsRows = MonitorGpuRows(gpuDetails);
    const auto weightsHit = MonitorGpuSegmentAt(gpuDetailsRows, 40, 440, 10, 383, 600, -1);
    const auto freeHit = MonitorGpuSegmentAt(gpuDetailsRows, 290, 440, 10, 383, 600, -1);
    assert(weightsHit.row == 0 && weightsHit.key == "weights" && weightsHit.label == L"Model weights" &&
           weightsHit.bytes == 1ULL * 1024 * 1024 * 1024 && weightsHit.total == 16ULL * 1024 * 1024 * 1024 &&
           freeHit.row == 0 && freeHit.key == "free" && freeHit.label == L"Free" &&
           MonitorGpuSegmentAt(gpuDetailsRows, 5, 440, 10, 383, 600, -1).row == -1);
    const auto gpuExpanded = MonitorGpuExpandedText(gpuDetails["gpu_map"][0], gpuDetails["engine"]);
    assert(gpuExpanded.find(L"layers 0–45") != std::wstring::npos &&
           gpuExpanded.find(L"KV window 131072 / 272000 positions; remainder in RAM") != std::wstring::npos &&
           gpuExpanded.find(L"expert slots") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "kv").find(L"131072 of 272000 positions here; the rest in RAM") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "kv").find(L"pinned in RAM") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "drafter").find(L"verify window") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "pool").find(L"320 expert slots") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "vision").find(L"pool's tail") != std::wstring::npos &&
           MonitorGpuSegmentHint(gpuDetails["gpu_map"][0], gpuDetails, "free").find(L"Kept free") != std::wstring::npos &&
            MonitorGpuPanelHeight(2, true) == 214 &&
            MonitorGpuRowAt(50, 220, 10, 100, 100, 3, 1) == 1 &&
            MonitorGpuRowAt(50, 330, 10, 100, 100, 3, 1) == 2 &&
           MonitorGpuRowAt(5, 200, 10, 100, 100, 3, 1) == -1);
    const Json heroIdle{{"live", {{"state", "idle"}, {"active", 0}, {"tok_s", 0.0}}},
                        {"requests", Json::array({{{"decode_tok_s", 9.8}, {"prompt_tokens", 1000},
                                                   {"reused", 200}, {"prompt_ms", 800.0}}})}};
    assert(MonitorHeroValue(heroIdle, Json::object(), "decode") == L"9.8");
    assert(MonitorHeroValue(heroIdle, Json::object(), "prefill") == L"1000");
    assert(MonitorHeroValue(heroIdle, Json::object(), "context") == L"—");
    assert(MonitorHeroValue(Json::object(), Json{{"max_context", 272000}}, "context") == L"272k");
    assert(MonitorHeroValue(Json{{"engine", {{"max_context", 131072}}}}, Json::object(), "context") == L"128K");
    const Json heroLive{{"live", {{"state", "generating"}, {"active", 1}, {"tok_s", 3.26},
                                  {"prefill_tok_s_mean", 120.0}}}};
    assert(MonitorHeroValue(heroLive, Json{{"max_context", 272000}}, "decode") == L"3.3");
    assert(MonitorHeroValue(heroLive, Json::object(), "prefill") == L"120");
    MonitorPrefillPeakState prefillPeakState;
    auto prefillPeak = UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "reading"}, {"prefill_tok_s_mean", 920.0}}}, {"totals", {{"requests", 7}}}},
        prefillPeakState);
    assert(prefillPeak.visible && prefillPeak.rate == 920.0 && prefillPeak.label == L"reading now");
    prefillPeak = UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "reading"}, {"prefill_tok_s_mean", 750.0}}}, {"totals", {{"requests", 7}}}},
        prefillPeakState);
    assert(prefillPeak.visible && prefillPeak.rate == 920.0 && MonitorPrefillPeakValue(prefillPeak) == L"920");
    prefillPeak = UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "generating"}}}, {"totals", {{"requests", 7}}}}, prefillPeakState);
    assert(prefillPeak.visible && prefillPeak.label == L"peak this request");
    prefillPeak = UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "idle"}}}, {"totals", {{"requests", 8}}}}, prefillPeakState);
    assert(prefillPeak.visible && prefillPeak.label == L"peak last request");
    assert(!UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "idle"}}}, {"totals", {{"requests", 9}}}}, prefillPeakState).visible);
    prefillPeak = UpdateMonitorPrefillPeak(Json{
        {"live", {{"state", "reading"}, {"prefill_tok_s_mean", 1100.0}}}, {"totals", {{"requests", 9}}}},
        prefillPeakState);
    assert(prefillPeak.visible && prefillPeak.rate == 1100.0 && prefillPeak.label == L"reading now");
    assert(MonitorPeakDecodeValue(Json{{"totals", {{"peak_decode", {{"tok_s", 10.74}}}}}}) == L"10.7");
    assert(MonitorPeakDecodeValue(Json{{"totals", {{"peak_decode", {{"tok_s", "bad"}}}}}}) == L"—");
    assert(MonitorPeakDecodeValue(Json::object()) == L"—");
    const auto peakDetails = MonitorPeakDecodeDetails(Json{{"tok_s", 10.74}, {"time", 1000}, {"tokens", 400}});
    assert(peakDetails.find(L"10.7 tok/s at ") == 0 &&
           peakDetails.find(L", 400 tokens into the answer") != std::wstring::npos);
    assert(MonitorPeakDecodeDetails(Json{{"tok_s", 0.0}}) == L"0.0 tok/s");
    assert(MonitorPeakDecodeDetails(Json{{"tok_s", "bad"}}) == L"—");
    assert(MonitorDiskReadRate(Json{{"disk_read_mb", 999.4}}) == L"999 MB/s");
    assert(MonitorDiskReadRate(Json{{"disk_read_mb", 9.9}}) == L"9.9 MB/s");
    assert(MonitorDiskReadRate(Json{{"disk_read_mb", 1536.0}}) == L"1.50 GB/s");
    assert(MonitorDiskReadRate(Json::object()) == L"—");
    assert(MonitorDiskWriteRate(Json{{"disk_write_mb", 12.34}}) == L"12.3 MB/s");
    assert(MonitorDiskWriteRate(Json::object()).empty());
    assert(MonitorDiskWriteLabel(Json{{"disk_write_mb", 12.34}}) == L"write 12.3 MB/s");
    assert(MonitorDiskWriteLabel(Json::object()).empty());
    const Json hardwareCards{
        {"hardware", {{"gpus", Json::array({
            Json{{"index", 0}, {"util", 50.4}, {"mem_used", 4ULL * 1024 * 1024 * 1024},
                 {"temp", 52.8}, {"power", 200.3}, {"power_limit", 320}, {"pcie_gen", 3},
                 {"pcie_gen_max", 4}, {"pcie_width", 16}, {"pcie_rx_mb", 1536.0}},
            Json{{"index", 1}, {"util", 60.4}, {"mem_used", 5ULL * 1024 * 1024 * 1024},
                 {"temp", 56.8}, {"power", 180.3}, {"power_limit", 260}, {"pcie_gen", 5},
                 {"pcie_gen_max", 5}, {"pcie_width", 16}, {"pcie_rx_mb", 800.0}}})},
            {"gpu_power_limit", 320}, {"gpu_pcie_rx_mb", 1536.0}, {"gpu_pcie_gen", 3},
            {"gpu_pcie_gen_max", 4}, {"gpu_pcie_width", 16}, {"disk_read_mb", 100.0},
            {"disk_write_mb", 8.26}}},
        {"hardware_static", {{"cores", 8}, {"threads", 16}, {"psutil", true}}},
        {"engine", {{"expert_slots", 15000}, {"ram_gb", 8.0}}}};
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_util", false, L"RTX 4080") == L"mean of 2 cards");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_mem_used", false, L"") == L"2 cards · 15000 experts cached");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_temp", false, L"") == L"hottest of 2 cards");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_power", false, L"") == L"of 320 W limit (all 2 cards)");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_pcie_rx_mb", false, L"") == L"to GPU 1.5 GB/s · idle Gen3");
    assert(MonitorHardwareSubtext(hardwareCards, "cpu", false, L"") == L"8 cores · 16 threads");
    assert(MonitorHardwareSubtext(hardwareCards, "ram_used", false, L"") == L"8.0 GB pinned for experts");
    assert(MonitorHardwareSubtext(hardwareCards, "disk_read_mb", false, L"") == L"write 8.3 MB/s");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_util", true, L"") == L"GPU 0 50% · GPU 1 60%");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_mem_used", true, L"") == L"GPU 0 4.0 GB · GPU 1 5.0 GB");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_temp", true, L"") == L"GPU 0 53° · GPU 1 57°");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_power", true, L"") == L"GPU 0 200/320 W · GPU 1 180/260 W");
    assert(MonitorHardwareSubtext(hardwareCards, "gpu_pcie_rx_mb", true, L"") ==
           L"GPU 0 Gen4 x16 1.5 GB/s · GPU 1 Gen5 x16 800 MB/s");
    assert(MonitorDataRate(Json()) == L"—" && MonitorDataRate(Json{{"rate", "bad"}}) == L"—");
    assert(MonitorDataRate(9.9) == L"9.9 MB/s" && MonitorDataRate(10.0) == L"10 MB/s" &&
           MonitorDataRate(999.4) == L"999 MB/s" && MonitorDataRate(1000.0) == L"1.0 GB/s" &&
           MonitorDataRate(1536.0) == L"1.5 GB/s");
    const Json requestRow{{"time", 1790000000}, {"finish", "stop"}, {"api", "openai"},
                          {"prompt_tokens", 1000}, {"reused", 200}, {"prompt_ms", 800.0},
                          {"output_tokens", 50}, {"decode_tok_s", 4.5}, {"hit_rate", 0.75}, {"duration_s", 12.3}};
    const auto requestCells = MonitorRequestColumns(requestRow);
    assert(requestCells[0] != L"—" && requestCells[1] == L"Done" && requestCells[2] == L"OpenAI" &&
           requestCells[3] == L"1000" && requestCells[4] == L"200" && requestCells[5] == L"1000" &&
           requestCells[6] == L"50" && requestCells[7] == L"4.5" && requestCells[8] == L"75.0%" &&
           requestCells[9] == L"12.3");
    const auto emptyRequestCells = MonitorRequestColumns(Json::object());
    assert(emptyRequestCells[0] == L"—" && emptyRequestCells[1] == L"—" && emptyRequestCells[9] == L"—");
    const auto requestText = MonitorRequestText(requestRow);
    assert(requestText.find(L"Done · OpenAI · prompt 1000 · reused 200 · prefill 1000 tok/s") != std::wstring::npos);
    assert(requestText.find(L"output 50 · decode 4.5 tok/s · VRAM hit 75.0% · took 12.3 s") != std::wstring::npos);
    assert(MonitorRequestText(Json::object()).find(L"— · — ·  · prompt —") != std::wstring::npos);
    assert(MonitorRequestText(Json::object()).find(L"· reused — · prefill — tok/s") != std::wstring::npos);
    assert(MonitorCountLabel(Json::object(), "count") == L"—" &&
           MonitorCountLabel(Json{{"count", 0}}, "count") == L"0" &&
           MonitorCountLabel(Json{{"count", 42}}, "count") == L"42");
    assert(MonitorRequestApiName("web") == L"Chat" && MonitorRequestApiName("anthropic") == L"Anthropic" &&
           MonitorRequestApiName("custom-provider") == L"custom-provider");
    const auto allReused = MonitorRequestText(Json{{"prompt_tokens", 100}, {"reused", 100}, {"prompt_ms", 10}});
    assert(allReused.find(L"prefill — tok/s") != std::wstring::npos);
    const Json dsTierMetrics{{"tiers", {{"now", {{"vram_used", 60}, {"ram_used", 25}, {"cpu_n", 15},
                                                  {"vram_gb", 12.0}, {"ram_gb", 8.0}, {"disk_gb", 0.0}}}}},
                               {"engine", {{"engine_kind", "ds4"}, {"cpu_experts", 99}, {"experts", 100}}}};
    const auto dsShares = MonitorTierShares(dsTierMetrics);
    assert(dsShares["total"] == 75 && dsShares["vram"] == 60 && dsShares["ram"] == 25 && dsShares["disk"] == 0);
    const Json nullCpuDsTierMetrics{{"tiers", {{"now", {{"vram_used", 60}, {"ram_used", 25}, {"cpu_n", nullptr}}}}},
                                    {"engine", {{"engine_kind", "ds4"}, {"cpu_experts", 99}}}};
    const auto nullCpuDsShares = MonitorTierShares(nullCpuDsTierMetrics);
    assert(nullCpuDsShares["total"] == 159 && nullCpuDsShares["disk"] == 74);
    const Json zeroDsTierMetrics{{"tiers", {{"now", {{"vram_used", 0}, {"ram_used", 12}, {"cpu_n", 0}}}}},
                                 {"engine", {{"engine_kind", "ds4"}, {"cpu_experts", 0}}}};
    const auto zeroDsShares = MonitorTierShares(zeroDsTierMetrics);
    assert(zeroDsShares["total"] == 12 && zeroDsShares["disk"] == 0);
    const Json glmTierMetrics{{"tiers", {{"now", {{"vram_used", 60}, {"ram_used", 25}}}}},
                              {"engine", {{"engine_kind", "glm"}, {"experts", 100}}}};
    const auto glmShares = MonitorTierShares(glmTierMetrics);
    assert(glmShares["total"] == 100 && glmShares["disk"] == 15);
    const Json zeroGlmTierMetrics{{"tiers", {{"now", {{"vram_used", 40}, {"ram_used", 60}}}}},
                                  {"engine", {{"engine_kind", "glm"}, {"experts", 0}}}};
    const auto zeroGlmShares = MonitorTierShares(zeroGlmTierMetrics);
    const auto inconsistentBar = MonitorTierBarFractions(Json{{"tiers", {{"now", {{"vram_used", 80}, {"ram_used", 60}}}}},
        {"engine", {{"engine_kind", "glm"}, {"experts", 100}}}});
    assert(zeroGlmShares["total"] == 100 && zeroGlmShares["disk"] == 0 &&
           inconsistentBar.size() == 3 && std::abs(inconsistentBar[0] - 80.0 / 140.0) < 1e-9 &&
           std::abs(inconsistentBar[1] - 60.0 / 140.0) < 1e-9 && inconsistentBar[2] == 0.0);
    const auto nullTierNow = MonitorTierNow(Json{{"tiers", nullptr}});
    const auto nullNow = MonitorTierNow(Json{{"tiers", {{"now", nullptr}}}});
    assert(nullTierNow.is_object() && nullTierNow.empty() && nullNow.is_object() && nullNow.empty());
    assert(MonitorTierNow(Json(nullptr)).is_object());
    assert(MonitorTierShares(Json{{"engine", nullptr}, {"tiers", nullptr}})["total"] == 1.0);
    assert(MonitorAgeText(12.4) == L"12 s" && MonitorAgeText(89.7) == L"90 s" &&
           MonitorAgeText(90.0) == L"2 min" && MonitorAgeText(5399.0) == L"90 min" &&
           MonitorAgeText(5400.0) == L"1 h 30 min" && MonitorAgeText(std::numeric_limits<double>::quiet_NaN()).empty());
    assert(MonitorTierTitle(dsTierMetrics) == L"Where the experts live");
    Json helperTierMetrics = dsTierMetrics;
    helperTierMetrics["tiers"]["now"]["helper_held"] = 1;
    assert(MonitorTierTitle(helperTierMetrics) == L"Primary cache and RAM backing");
    assert(MonitorTierLegendLabel(helperTierMetrics, 0) == L"Primary VRAM" &&
           MonitorTierLegendLabel(helperTierMetrics, 1) == L"RAM");
    assert(MonitorTierLegendLabel(glmTierMetrics, 0) == L"VRAM" &&
           MonitorTierLegendLabel(glmTierMetrics, 1) == L"Pinned RAM" &&
           MonitorTierLegendLabel(glmTierMetrics, 2) == L"SSD only" &&
           MonitorTierLegendLabel(glmTierMetrics, 3).empty());
    const auto formattedGb = MonitorNumberLabel(8.5, 1);
    assert(MonitorNumberLabel(75.0) == L"75" && formattedGb.size() >= 3 &&
           formattedGb.find(L'8') != std::wstring::npos && formattedGb.back() == L'5');
    Json tierCaptionMetrics = dsTierMetrics;
    tierCaptionMetrics["tiers"]["now"]["time"] = 970.0;
    assert(MonitorTierCaption(tierCaptionMetrics, 1000.0) == L"live · 75 routed experts");
    assert(MonitorTierCaption(tierCaptionMetrics, 1000.5) == L"last reading 31 s ago, from the last answer");
    assert(MonitorTierCaption(tierCaptionMetrics, 1060.0) == L"last reading 2 min ago, from the last answer");
    tierCaptionMetrics["tiers"]["now"]["time"] = 0.0;
    assert(MonitorTierCaption(tierCaptionMetrics, 1060.0).empty());
    assert(MonitorReport(Json{{"tiers", nullptr}}).find(L"Time/token: — ms") != std::wstring::npos);
    assert(MonitorReportRefreshDue(10000, 0, false) &&
           !MonitorReportRefreshDue(12999, 10000, false) &&
           MonitorReportRefreshDue(13000, 10000, false) &&
           MonitorReportRefreshDue(10001, 10000, true) &&
           !MonitorReportRefreshDue(9999, 10000, false));
    assert(MonitorExpertReport(Json{{"tiers", nullptr}}).find(L"No engine STAT reading") != std::wstring::npos);
    assert(MonitorStatsColumns(640, false) == 4 && MonitorStatsColumns(488, false) == 3 &&
           MonitorStatsColumns(336, false) == 2 && MonitorStatsColumns(0, false) == 2 &&
           MonitorStatsColumns(220, true) == 3);
    assert(MonitorStatsColumnSpan(3, 3, true) == 3 && MonitorStatsColumnSpan(2, 3, true) == 1 &&
           MonitorStatsColumnSpan(3, 3, false) == 1);
    const auto wideMonitor = MonitorPageSectionsFor(150, 900);
    const auto stackedMonitor = MonitorPageSectionsFor(150, 835);
    const auto narrowBounds = MonitorPageBoundsFor(264, 988);
    const auto wideBounds = MonitorPageBoundsFor(264, 1628);
    assert(!wideMonitor.stacked && wideMonitor.rightNowTop == 150 && wideMonitor.contextTop == 518 &&
            wideMonitor.tiersTop == 774 && wideMonitor.gpuMapTop == 846);
    assert(stackedMonitor.stacked && stackedMonitor.rightNowTop == 518 && stackedMonitor.contextTop == 886 &&
            stackedMonitor.tiersTop == 1142 && stackedMonitor.gpuMapTop == 1214);
    assert(narrowBounds.left == 264 && narrowBounds.width == 988 &&
           wideBounds.left == 550 && wideBounds.width == MonitorPageContentMaxWidth);
    const auto monitorLayoutRefresh = MonitorPageLayoutRefreshRegionFor(
        {264, 988}, {272, 990}, 110, 900);
    assert(monitorLayoutRefresh.left == 264 && monitorLayoutRefresh.top == 110 &&
           monitorLayoutRefresh.right == 1262 && monitorLayoutRefresh.bottom == 900);
    const auto wideRefresh = MonitorPageRefreshRegionsFor(10, 150, 900, 1, false);
    const auto stackedRefresh = MonitorPageRefreshRegionsFor(10, 150, 835, 5, false);
    const auto wideGpuHover = MonitorGpuPanelRegionFor(10, 150, 900, 1, false);
    const auto stackedGpuHover = MonitorGpuPanelRegionFor(10, 150, 835, 5, true);
    const auto contextOnlyRefresh = MonitorPageRefreshRegionsFor(10, 150, 900, 1, false,
        false, false, true, false, false, false);
    assert(wideRefresh.size() == 6 && wideRefresh[0].left == 18 && wideRefresh[0].top == 184 &&
           wideRefresh[0].right == 564 && wideRefresh[0].bottom == 494 &&
           wideRefresh[1].left == 596 && wideRefresh[1].right == 902 &&
           wideRefresh[1].top == 184 && wideRefresh[1].bottom == 494 &&
            wideRefresh[2].top == 530 && wideRefresh[2].bottom == 750 && wideRefresh[3].top == 776 &&
            wideRefresh[3].bottom == 840 && wideRefresh[4].top == 846 && wideRefresh[4].bottom == 950 &&
            wideRefresh[5].top == 958 && wideRefresh[5].bottom == 1174);
    assert(wideGpuHover.left == 10 && wideGpuHover.top == 846 && wideGpuHover.right == 910 &&
           wideGpuHover.bottom == 950 && stackedGpuHover.top == 1214 && stackedGpuHover.bottom == 1620);
    assert(stackedRefresh.size() == 6 && stackedRefresh[0].top == 184 && stackedRefresh[0].bottom == 494 &&
           stackedRefresh[1].top == 552 && stackedRefresh[1].bottom == 862 &&
           stackedRefresh[2].top == 898 && stackedRefresh[2].bottom == 1118 &&
            stackedRefresh[3].top == 1144 && stackedRefresh[3].bottom == 1208 &&
            stackedRefresh[4].top == 1214 && stackedRefresh[4].bottom == 1574 &&
            stackedRefresh[5].top == 1582 && stackedRefresh[5].bottom == 1798);
    assert(contextOnlyRefresh.size() == 1 && contextOnlyRefresh[0].top == 530 &&
           contextOnlyRefresh[0].bottom == 750);
    const Json monitorRefreshBaseline{
        {"live", {{"state", "idle"}, {"active", 0}}},
        {"history", {{"tok_s", {1.0}}, {"prefill_tok_s_mean", {2.0}}, {"gpu_util", {3.0}}}},
        {"totals", {{"requests", 1}}}, {"tiers", {{"now", {{"ms_tok", 200.0}}}}},
        {"engine", {{"max_context", 272000}}}, {"requests", Json::array({Json{{"output_tokens", 10}}})},
        {"gpu_map", Json::array({Json{{"total", 16}}})}, {"hardware", {{"gpu_util", 18}}},
        {"hardware_static", {{"gpu_name", "GPU"}}}};
    const Json monitorRefreshHealth{{"max_context", 272000}};
    assert(MonitorPageOwnsBackground(true));
    assert(!MonitorPageOwnsBackground(false));
    const auto noMonitorChange = MonitorPageRefreshChangesFor(
        monitorRefreshBaseline, monitorRefreshBaseline, monitorRefreshHealth, monitorRefreshHealth);
    assert(!noMonitorChange.throughput && !noMonitorChange.rightNow && !noMonitorChange.context &&
           !noMonitorChange.tiers && !noMonitorChange.gpu && !noMonitorChange.hardware);
    auto gpuHistoryChanged = monitorRefreshBaseline;
    gpuHistoryChanged["history"]["gpu_util"][0] = 4.0;
    const auto gpuOnlyChange = MonitorPageRefreshChangesFor(
        monitorRefreshBaseline, gpuHistoryChanged, monitorRefreshHealth, monitorRefreshHealth);
    assert(!gpuOnlyChange.throughput && !gpuOnlyChange.rightNow && !gpuOnlyChange.context &&
           !gpuOnlyChange.tiers && !gpuOnlyChange.gpu && gpuOnlyChange.hardware);
    auto throughputChanged = monitorRefreshBaseline;
    throughputChanged["history"]["tok_s"][0] = 4.0;
    const auto throughputOnlyChange = MonitorPageRefreshChangesFor(
        monitorRefreshBaseline, throughputChanged, monitorRefreshHealth, monitorRefreshHealth);
    assert(throughputOnlyChange.throughput && !throughputOnlyChange.rightNow &&
           !throughputOnlyChange.context && !throughputOnlyChange.tiers && !throughputOnlyChange.gpu &&
           !throughputOnlyChange.hardware);
    auto unpaintedLiveFieldChanged = monitorRefreshBaseline;
    unpaintedLiveFieldChanged["live"]["tok_s_window_s"] = 2.0;
    const auto noPaintedValueChanged = MonitorPageRefreshChangesFor(
        monitorRefreshBaseline, unpaintedLiveFieldChanged, monitorRefreshHealth, monitorRefreshHealth);
    assert(!noPaintedValueChanged.throughput && !noPaintedValueChanged.rightNow &&
           !noPaintedValueChanged.context && !noPaintedValueChanged.tiers && !noPaintedValueChanged.gpu &&
           !noPaintedValueChanged.hardware);
    Json readingRefreshBaseline{{"live", {{"state", "reading"}, {"active", 1}, {"prompt_tokens", 100},
        {"prompt_read", 20}, {"prompt_total", 100}, {"prompt_reused", 10}, {"prefill_tok_s_mean", 700.0}}}};
    auto readingProgressChanged = readingRefreshBaseline;
    readingProgressChanged["live"]["prompt_read"] = 40;
    const auto readingProgressRefresh = MonitorPageRefreshChangesFor(
        readingRefreshBaseline, readingProgressChanged);
    assert(!readingProgressRefresh.throughput && readingProgressRefresh.rightNow &&
           !readingProgressRefresh.context && !readingProgressRefresh.tiers && !readingProgressRefresh.gpu &&
           !readingProgressRefresh.hardware);
    auto reusedTokensChanged = readingRefreshBaseline;
    reusedTokensChanged["live"]["prompt_reused"] = 30;
    const auto contextReuseRefresh = MonitorPageRefreshChangesFor(readingRefreshBaseline, reusedTokensChanged);
    assert(!contextReuseRefresh.throughput && !contextReuseRefresh.rightNow &&
           contextReuseRefresh.context && !contextReuseRefresh.tiers && !contextReuseRefresh.gpu &&
           !contextReuseRefresh.hardware);
    Json generationRefreshBaseline{{"live", {{"state", "generating"}, {"active", 1}, {"writing", 1},
        {"tok_s", 4.0}, {"phase", "Writing"}, {"generated", 1}, {"prompt_tokens", 100},
        {"elapsed_s", 1.0}, {"max_tokens", 100}}}};
    auto decodeSpeedChanged = generationRefreshBaseline;
    decodeSpeedChanged["live"]["tok_s"] = 5.0;
    const auto decodeSpeedRefresh = MonitorPageRefreshChangesFor(generationRefreshBaseline, decodeSpeedChanged);
    assert(decodeSpeedRefresh.throughput && !decodeSpeedRefresh.rightNow &&
           !decodeSpeedRefresh.context && !decodeSpeedRefresh.tiers && !decodeSpeedRefresh.gpu &&
           !decodeSpeedRefresh.hardware);
    assert(MonitorGpuMapTop(150, 900) == 846 && MonitorGpuActionsTop(150, 900, 1, false) == 1192 &&
           MonitorGpuRowAt(50, 890, 10, MonitorGpuMapTop(150, 900), 100, 3, -1) == 0);
    assert(MonitorPageContentBottom(150, 900, 1, false) == 1384 &&
           MonitorPageContentBottom(150, 900, 1, false, 12) == 2262 &&
           MonitorPageScrollLimit(150, 900, 1, false, 800) == 584 &&
           MonitorPageScrollLimit(150, 900, 1, false, 1020) == 364 &&
           MonitorPageScrollLimit(150, 835, 1, false, 800) == 952);
    const Json readingMetrics{{"live", {{"state", "reading"}, {"prompt_read", 40}, {"prompt_total", 80}}}};
    const Json writingMetrics{{"live", {{"state", "generating"}, {"generated", 120}, {"max_tokens", 100}}}};
    const Json readingCountMissing{{"live", {{"state", "reading"}, {"prompt_total", 80}}}};
    const Json readingCountNull{{"live", {{"state", "reading"}, {"prompt_read", nullptr}, {"prompt_total", 80}}}};
    const Json readingCountZero{{"live", {{"state", "reading"}, {"prompt_read", 0}, {"prompt_total", 80}}}};
    assert(MonitorRequestActivityFor(readingMetrics) == MonitorRequestActivity::reading &&
           MonitorRequestProgressPercent(readingMetrics, MonitorRequestActivity::reading) == 50);
    assert(MonitorRequestActivityFor(writingMetrics) == MonitorRequestActivity::generating &&
           MonitorRequestProgressPercent(writingMetrics, MonitorRequestActivity::generating) == 100);
    assert(MonitorRequestProgressPercent(readingCountMissing, MonitorRequestActivity::reading) == -1 &&
           MonitorRequestProgressPercent(readingCountNull, MonitorRequestActivity::reading) == -1 &&
           MonitorRequestProgressPercent(readingCountZero, MonitorRequestActivity::reading) == 0);
    assert(MonitorRequestActivityFor(Json{{"live", {{"state", "idle"}}}}) == MonitorRequestActivity::idle &&
           MonitorRequestProgressPercent(readingMetrics, MonitorRequestActivity::idle) == -1);
    assert(MonitorGpuRows(monitorMetrics).size() == 1 && MonitorGpuPanelHeight(1) == 104 &&
           MonitorGpuPanelHeight(0) == 0);
    assert(MonitorPanelGeometryChanged(0, false, 1, false));
    assert(!MonitorPanelGeometryChanged(1, false, 1, false));
    assert(MonitorPanelGeometryChanged(1, false, 1, true));
    assert(MonitorPanelGeometryChanged(5, false, 1, false));
    Json plainGpus = Json::array();
    for (int i = 0; i < 5; ++i)
        plainGpus.push_back({{"index", i}, {"mem_total", 8ULL * 1024 * 1024 * 1024},
                             {"mem_used", (uint64_t)(i + 1) * 1024 * 1024 * 1024}});
    const Json plainGpuMetrics{{"hardware", {{"gpus", plainGpus}}},
                               {"hardware_static", {{"gpu_name", "Card A + Card B + Card C + Card D + Card E"}}}};
    const auto plainRows = MonitorGpuRows(plainGpuMetrics);
    assert(plainRows.size() == 5 && plainRows[0].value("plain", false) && plainRows[0]["name"] == "Card A" &&
           plainRows[0]["parts"]["used"] == 1024ULL * 1024 * 1024 && plainRows[4]["free"] == 3ULL * 1024 * 1024 * 1024 &&
            MonitorGpuPanelHeight(plainRows.size()) == 360);
    Json fourGpuReadings = Json::array();
    for (int i = 0; i < 4; ++i)
        fourGpuReadings.push_back(plainGpus[i]);
    const Json fourGpuMetrics{{"hardware", {{"gpus", fourGpuReadings}}}};
    assert(MonitorGpuRows(fourGpuMetrics).empty());
    const auto spark = MonitorSparklinePoints(Json::array({0, 2, 1}), 100, 30);
    assert(spark.size() == 3 && spark.front().x == 0 && spark.front().y == 28 && spark[1].x == 49 &&
           spark[1].y == 4 && spark.back().x == 99 && spark.back().y == 16);
    const auto sparkArea = MonitorSparklineAreaPoints(spark, 30);
    assert(sparkArea.size() == 5 && sparkArea[0].x == 0 && sparkArea[0].y == 28 &&
           sparkArea[3].x == 99 && sparkArea[3].y == 29 && sparkArea[4].x == 0 && sparkArea[4].y == 29);
    assert(MonitorSparklineAreaPoints(spark, 0).empty());
    assert(MonitorSparklineAreaPoints({}, 30).empty());
    assert(MonitorSparklinePoints(Json::array({0, nullptr, 0}), 100, 30).empty());
    assert(MonitorSparklinePoints(Json::array({1, 2}), 1, 30).empty());
    assert(MonitorAllRequestsIncomplete({{"requests", Json::array({1, 2})}, {"requests_kept", 5}}, true));
    assert(!MonitorAllRequestsIncomplete({{"requests", Json::array({1, 2, 3})}, {"requests_kept", 3}}, true));
    assert(!MonitorAllRequestsIncomplete({{"requests", Json::array({1, 2})}, {"requests_kept", 5}}, false));
    assert(MonitorRequestsQueryAll(true, true));
    assert(!MonitorRequestsQueryAll(true, false));
    assert(!MonitorRequestsQueryAll(false, true));
    Json writingTools = Json::array();
    int writingLimit = 0;
    ApplyMcpEvent(writingTools, writingLimit, {{"event", "start"}, {"id", "writing-1"}, {"name", "weather"}}, 8, 4);
    assert(writingTools.size() == 1 && writingTools[0]["name"] == "weather" &&
           writingTools[0]["state"] == "writing" && writingTools[0]["at"] == 8 && writingTools[0]["rat"] == 4);
    assert(McpCompactText({{"event", "start"}, {"name", "weather"}}) == L"[Tool weather · Writing]\r\n");
    const auto compactToolArgs = McpCompactText({{"event", "call"}, {"name", "search"},
        {"arguments", {{"query", "alpha  beta"}}}});
    assert(compactToolArgs.find(L"{\"query\":\"alpha beta\"}") != std::wstring::npos);
    const std::wstring longToolResult(201, L'x');
    const auto compactToolResult = McpCompactText({{"event", "result"}, {"tool", "read"},
        {"text", Utf8(longToolResult)}, {"ok", true}, {"chars", 201}, {"ms", 4}});
    assert(compactToolResult.find(std::wstring(200, L'x')) != std::wstring::npos &&
           compactToolResult.find(longToolResult) == std::wstring::npos);
    assert(McpSavedText(writingTools[0]).find(L"(being written)") != std::wstring::npos);
    writingTools.push_back({{"state", "running"}});
    writingTools.push_back({{"state", "done"}});
    writingTools.push_back({{"state", "error"}});
    writingTools.push_back({{"state", "skipped"}});
    FinalizeMcpEvents(writingTools);
    assert(writingTools[0]["state"] == "skipped" && writingTools[0]["ms"].is_null());
    assert(writingTools[1]["state"] == "skipped" && writingTools[1]["ms"].is_null());
    assert(writingTools[2]["state"] == "done" && writingTools[3]["state"] == "error" &&
           writingTools[4]["state"] == "skipped");
    Json toolRecords = Json::array();
    int toolLimit = 0;
    ApplyMcpEvent(toolRecords, toolLimit,
                  {{"event", "call"}, {"id", "c1"}, {"name", "weather"}, {"server", "forecast"},
                   {"tool", "get_weather"}, {"arguments", {{"city", "Oradea"}}}, {"round", 1}},
                  6, 0);
    ApplyMcpEvent(toolRecords, toolLimit,
                  {{"event", "result"}, {"id", "c1"}, {"ok", true}, {"text", "Sunny"}, {"chars", 5},
                   {"truncated", false}, {"ms", 120}},
                  6, 0);
    const auto apiToolMessages = AssistantApiMessages("beforeafter", toolRecords);
    assert(apiToolMessages.size() == 3 && apiToolMessages[0]["content"] == "before" &&
           apiToolMessages[0]["tool_calls"][0]["function"]["arguments"] == "{\"city\":\"Oradea\"}" &&
           apiToolMessages[1]["role"] == "tool" && apiToolMessages[1]["content"] == "Sunny" &&
           apiToolMessages[2]["content"] == "after");
    for (const Json arguments : {Json(nullptr), Json(false), Json(0), Json("")})
    {
        const Json falsyArgumentTool = Json::array({{{"id", "falsy"}, {"name", "probe"}, {"arguments", arguments},
                                                     {"round", 0}, {"result", "ok"}, {"state", "done"}, {"at", 0}}});
        const auto messages = AssistantApiMessages("", falsyArgumentTool);
        assert(messages.size() == 2 && messages[0]["tool_calls"][0]["function"]["arguments"] == "{}" &&
               messages[1]["role"] == "tool");
    }
    const Json arrayArgumentTool = Json::array({{{"id", "array"}, {"name", "probe"}, {"arguments", Json::array()},
                                                  {"round", 0}, {"result", "ok"}, {"state", "done"}, {"at", 0}}});
    const auto arrayArgumentMessages = AssistantApiMessages("", arrayArgumentTool);
    assert(arrayArgumentMessages[0]["tool_calls"][0]["function"]["arguments"] == "[]");
    const auto stoppedEmpty = AssistantApiMessages("", Json::array(), true);
    assert(stoppedEmpty.size() == 1 && stoppedEmpty[0]["role"] == "assistant" && stoppedEmpty[0]["content"] == "");
    assert(AssistantApiMessages("partial answer", Json::array(), false, true).empty());
    assert(AssistantApiMessages("", Json::array()).empty());
    const Json nullResultTools = Json::array({{{"id", "null-result"}, {"round", 1}, {"result", nullptr},
                                               {"state", "done"}, {"at", 0}, {"name", "tool"}}});
    const auto omittedNullResult = AssistantApiMessages("plain answer", nullResultTools);
    assert(omittedNullResult.size() == 1 && omittedNullResult[0]["role"] == "assistant" &&
           omittedNullResult[0]["content"] == "plain answer");
    assert(TrimAssistantApiText(Utf8(L"\u00A0 \tbefore \r\n\uFEFF")) == "before");
    const Json trimTools = Json::array({{{"id", "trim"}, {"round", 1}, {"result", "ok"}, {"state", "done"},
                                         {"at", 10}, {"name", "tool"}}});
    const auto trimmedToolRound = AssistantApiMessages(" \tbefore \n  after\t ", trimTools);
    assert(trimmedToolRound.size() == 3 && trimmedToolRound[0]["content"] == "before" &&
           trimmedToolRound[2]["content"] == "after");
    const Json trailingTools = Json::array({{{"id", "tail"}, {"round", 1}, {"result", "ok"}, {"state", "done"},
                                             {"at", 4}, {"name", "tool"}}});
    assert(AssistantApiMessages("head   ", trailingTools).size() == 2);
    const auto qwenHistory = AssistantApiMessages("answer", Json::array(), false, false, "kept thought", true);
    assert(qwenHistory.size() == 1 && qwenHistory[0]["reasoning_content"] == "kept thought");
    const auto deepseekHistory = AssistantApiMessages("answer", Json::array(), false, false, "hidden thought", false);
    assert(deepseekHistory.size() == 1 && !deepseekHistory[0].contains("reasoning_content"));
    const Json qwenToolRecords = Json::array({{{"id", "call-1"}, {"name", "search"}, {"arguments", Json::object()},
                                               {"round", 0}, {"result", "found"}, {"state", "done"},
                                               {"at", 6}, {"rat", 4}}});
    const auto qwenToolHistory = AssistantApiMessages("beforeafter", qwenToolRecords, false, false, "plan next", true);
    assert(qwenToolHistory.size() == 3 && qwenToolHistory[0]["content"] == "before" &&
           qwenToolHistory[0]["reasoning_content"] == "plan" && qwenToolHistory[1]["role"] == "tool" &&
           qwenToolHistory[2]["content"] == "after" && qwenToolHistory[2]["reasoning_content"] == " next");
    assert(NativeMcpEnabled(true, {{"servers", Json::array({Json::object()})}, {"tools", 1}}));
    assert(!NativeMcpEnabled(true, {{"servers", Json::array({Json::object()})}, {"tools", 0}}));
    assert(!NativeMcpEnabled(false, {{"tools", 4}}));
    assert(!NativeMcpEnabled(true, Json::object()));
    assert(NativeMcpRefreshEligible(true, true, false));
    assert(!NativeMcpRefreshEligible(false, true, false));
    assert(!NativeMcpRefreshEligible(true, false, false));
    assert(!NativeMcpRefreshEligible(true, true, true));
    const Json toolRoundOffsets = Json::array({{{"at", 6}, {"rat", 3}}});
    assert(NativeToolRoundNeedsSeparator(toolRoundOffsets, 6, "at"));
    assert(NativeToolRoundNeedsSeparator(toolRoundOffsets, 3, "rat"));
    assert(!NativeToolRoundNeedsSeparator(toolRoundOffsets, 5, "at"));
    assert(!NativeToolRoundNeedsSeparator(toolRoundOffsets, 0, "at"));
    assert(!NativeToolRoundNeedsSeparator(Json::array(), 6, "at"));
    std::string toolRoundText = "before";
    assert(AppendNativeToolRoundText(toolRoundText, toolRoundOffsets, "at", "after"));
    assert(toolRoundText == "before\n\nafter");
    assert(!AppendNativeToolRoundText(toolRoundText, toolRoundOffsets, "at", "tail"));
    assert(toolRoundText == "before\n\naftertail");
    std::string reasoningChunks;
    const Json noReasoningRounds = Json::array();
    assert(!AppendNativeToolRoundText(reasoningChunks, noReasoningRounds, "rat", "Think"));
    assert(!AppendNativeToolRoundText(reasoningChunks, noReasoningRounds, "rat", "ing"));
    assert(!AppendNativeToolRoundText(reasoningChunks, noReasoningRounds, "rat", " clearly."));
    assert(reasoningChunks == "Thinking clearly.");
    const Json reasoningRoundOffsets = Json::array({{{"rat", 17}}});
    assert(AppendNativeToolRoundText(reasoningChunks, reasoningRoundOffsets, "rat", "next"));
    assert(reasoningChunks == "Thinking clearly.\n\nnext");
    assert(!AppendNativeToolRoundText(reasoningChunks, reasoningRoundOffsets, "rat", " token"));
    assert(reasoningChunks == "Thinking clearly.\n\nnext token");
    assert(McpSavedText(toolRecords[0]).find(L"Sunny") != std::wstring::npos);
    const auto mcpStatus = McpStatusReport({{"tools", 2}, {"servers", Json::array({
        {{"name", "weather"}, {"transport", "stdio"}, {"status", "ready"},
         {"info", {{"name", "Forecast MCP"}, {"version", "1.2"}}},
         {"tools", Json::array({{{"tool", "get_weather"}, {"description", "Current conditions by city"}},
                                {{"tool", "get_alerts"}}})}},
        {{"name", "broken"}, {"transport", "http"}, {"status", "failed"}, {"error", "connection refused"},
         {"tools", Json::array()}}})}});
    assert(mcpStatus.find("2 tools · 1 of 2 connected") != std::string::npos &&
           mcpStatus.find("weather · Connected · stdio · 2 tools") != std::string::npos &&
           mcpStatus.find("broken · Failed · http · 0 tools") != std::string::npos &&
           mcpStatus.find("get_weather") != std::string::npos &&
           mcpStatus.find("Current conditions by city") != std::string::npos &&
           mcpStatus.find("get_alerts") != std::string::npos &&
           mcpStatus.find("Error: connection refused") != std::string::npos &&
           McpStatusReport(Json::object()).find("No MCP servers are configured") != std::string::npos);
    assert(McpStatusLabel("starting") == "Starting" && McpStatusLabel("stopped") == "Stopped" &&
           McpStatusLabel("idle") == "Waiting" && McpStatusLabel("custom-state") == "custom-state");
    ApplyMcpEvent(toolRecords, toolLimit, {{"event", "limit"}, {"max_rounds", 4}}, 0, 0);
    assert(toolLimit == 4);
    const auto answerMeta = AnswerMetadata(
        {{"prompt_tokens", 200}, {"completion_tokens", 42}},
        {{"predicted_per_second", 2.5}, {"prompt_n", 50}, {"prompt_ms", 2000.0}, {"cache_n", 150}},
        20.0, true, Json::array({{{"state", "done"}}}), 4);
    assert(answerMeta == "42 tokens · 2.5 tok/s · read 50 new prompt tokens in 2.0 s (150 reused) · 1 tool call · "
                         "stopped at the limit of 4 tool rounds (mcp.max_rounds) · stopped");
    assert(AnswerMetadata({{"completion_tokens", 42}}, {{"predicted_per_second", 0}}, 20.0, false,
                          Json::array(), 0) == "42 tokens · 2.1 tok/s");
    const auto folders = ParseFolders(SerializeFolders({{"f1", "Projects", true}, {"f2", "Research", false}}));
    assert(folders.size() == 2 && folders[0].id == "f1" && folders[0].collapsed && !folders[1].collapsed);
    const Json successfulExportTool{{"server", "weather"}, {"tool", "forecast"},
                                    {"arguments", {{"city", "Oradea"}}}, {"result", "Sunny"}, {"ok", true}};
    SYSTEMTIME exportTime{};
    exportTime.wYear = 2026; exportTime.wMonth = 10; exportTime.wDay = 26;
    exportTime.wHour = 10; exportTime.wMinute = 9;
    assert(ChatExportFilename(exportTime) == L"maya-chat-2026-10-26-10-09.md");
    const Json failedExportTool{{"name", "read"}, {"arguments", Json::object()}, {"result", "Unavailable"},
                                {"ok", false}};
    const Json jsExportTool{{"server", ""}, {"tool", ""}, {"name", "fallback"}, {"arguments", false},
                            {"result", Json{{"state", "ok"}}}, {"ok", 1}};
    const Json jsExportArray{{"name", "array"}, {"arguments", Json::object()},
                             {"result", Json::array({"first", nullptr, 2, Json{{"key", "value"}},
                                                      1.0, 1e6, 1e-6, 1e-7})}};
    const Json jsArgumentTool{{"name", "numbers"},
                              {"arguments", Json::parse(R"({"decimal":1.0,"large":1e20,"values":[1e-6,1e-7]})")},
                              {"result", "ok"}, {"ok", true}};
    const auto markdown = ChatMarkdown("Test chat", "Be concise", "Maya model",
                                       {{"assistant", "answer", "reasoning",
                                         {successfulExportTool, failedExportTool, jsExportTool, jsExportArray,
                                          jsArgumentTool, {{"name", "pending"}}}}});
    assert(markdown.find("# Test chat") != std::string::npos && markdown.find("<summary>Thinking</summary>") !=
           std::string::npos && markdown.find("Attachments:") == std::string::npos &&
           markdown.find("image.png") == std::string::npos &&
           markdown.find("<summary>Tool weather / forecast</summary>") != std::string::npos &&
           markdown.find("<summary>Tool read (error)</summary>") != std::string::npos &&
           markdown.find("\"city\": \"Oradea\"") != std::string::npos &&
           markdown.find("Sunny") < markdown.find("answer") &&
           markdown.find("<summary>Tool pending</summary>") == std::string::npos);
    assert(markdown.find("Tool fallback</summary>") != std::string::npos);
    assert(markdown.find("Tool fallback (error)") == std::string::npos);
    assert(markdown.find("Tool array (error)</summary>") != std::string::npos);
    assert(markdown.find("[object Object]") != std::string::npos);
    assert(markdown.find("first,,2,[object Object],1,1000000,0.000001,1e-7") != std::string::npos);
    assert(markdown.find("\"decimal\": 1,\n  \"large\": 100000000000000000000,\n  \"values\": [\n    0.000001,\n    1e-7\n  ]") != std::string::npos);
    const auto errorExport = ChatMarkdown("Failed chat", "", "Maya",
                                           {{"assistant", "", "", {}, "HTTP 503: unavailable"}});
    assert(errorExport.find("HTTP 503: unavailable") != std::string::npos);
    assert(ChatMarkdown("T", "", "M", {{"user", "hi"}}) == "# T\n\n## You\n\nhi\n");
    assert(ChatMarkdown("T", "", "M", {{"user", "hi"}, {"assistant", "ok"}}) ==
           "# T\n\n## You\n\nhi\n\n## M\n\nok\n");
    const auto multilineInstructions = ChatMarkdown("Test chat", "First line\r\nSecond line\nThird line", "Maya", {});
    assert(multilineInstructions.find("> Instructions: First line Second line Third line\n\n") != std::string::npos);
    std::vector<Json> history;
    assert(ApplyChatHistoryRecord(history, {{"role", "user"}, {"text", "old"}}));
    assert(ApplyChatHistoryRecord(history, {{"role", "assistant"}, {"text", "old answer"}}));
    assert(ApplyChatHistoryRecord(history, ReplaceChatHistoryFrom(0, {{"role", "user"}, {"text", "edited"}})));
    assert(history.size() == 1 && history[0]["text"] == "edited");
    assert(ApplyChatHistoryRecord(history, {{"role", "assistant"}, {"text", "new answer"}}));
    assert(ApplyChatHistoryRecord(history, TruncateChatHistory(1)) && history.size() == 1);
    assert(!ApplyChatHistoryRecord(history, TruncateChatHistory(5)) && history.size() == 1);
    assert(!ApplyChatHistoryRecord(history, ReplaceChatHistoryFrom(0, Json())) && history.size() == 1);
    assert(PullStatusText({{"model", "test-model"}, {"state", "starting"}}) == "The setup is starting…");
    assert(PullStatusActive(Json{{"state", "starting"}}));
    assert(PullStatusActive(Json{{"state", "downloading"}}));
    assert(PullStatusNeedsModelRefresh(true, Json{{"state", "success"}}));
    assert(!PullStatusNeedsModelRefresh(false, Json{{"state", "success"}}));
    assert(!PullStatusNeedsModelRefresh(true, Json{{"state", "error"}}));
    assert(!PullStatusNeedsModelRefresh(true, Json(nullptr)));
    assert(!PullStatusActive(Json(nullptr)) && !PullStatusActive(Json::array()));
    assert(!PullStatusActive(Json{{"state", false}}));
    assert(PullStatusText(Json(nullptr)) == "No model download is active.");
    assert(PullStatusText(Json::array()) == "No model download is active.");
    assert(PullStatusText(Json{{"state", false}}) == "Download state unavailable.");
    assert(PullFormatBytes(999) == PullFormatNumber(999, 0) + " bytes" &&
           PullFormatBytes(1000000) == PullFormatNumber(1, 0) + " MB" &&
           PullFormatBytes(10000000000LL) == PullFormatNumber(10, 0) + " GB");
    const auto pullProgressText = PullStatusText({{"model", "test-model"}, {"state", "downloading"},
        {"completed", 5000000000LL}, {"total", 10000000000LL}, {"percent", 50}, {"bytes_per_s", 40000000.0}});
    assert(pullProgressText.find(PullFormatBytes(5000000000LL) + " of " + PullFormatBytes(10000000000LL) +
                                 " (50%) · " + PullFormatNumber(40, 0) + " MB/s") != std::string::npos);
    assert(PullStatusText({{"model", "test-model"}, {"state", "starting"}, {"message", ""}}) ==
           "The setup is starting…");
    assert(PullStatusText({{"model", "test-model"}, {"state", "downloading"}, {"completed", 5'000'000'000LL},
                           {"total", 10'000'000'000LL}, {"bytes_per_s", 40'000'000.0}})
               .find("50%") != std::string::npos);
    assert(PullStatusText({{"model", "test-model"}, {"state", "error"}, {"error", "fixture error"}})
               .find("fixture error") != std::string::npos);
    assert(PullCancelErrorText("fixture error").find("Download was not cancelled: fixture error") != std::string::npos);
    assert(PullStatusText({{"model", "test-model"}, {"state", "success"}}).find("complete") != std::string::npos);
    assert(McpCompactText(Json{{"event", "call"}, {"server", "files"}, {"tool", "read"}}).find(L"Running") != std::wstring::npos);
    assert(McpCompactText(Json{{"event", "result"}, {"server", "files"}, {"tool", "read"},
                               {"ok", false}, {"chars", 3}, {"ms", 7}}).find(L"Error") != std::wstring::npos);
    const Json unroutedTool{{"event", "call"}, {"id", "unknown-route"}, {"name", "unrouted_tool"},
                            {"server", nullptr}, {"tool", "unrouted_tool"}, {"arguments", Json::object()}};
    assert(McpProgressText(unroutedTool).find(L"unrouted_tool · Running") != std::wstring::npos);
    assert(McpCompactText(unroutedTool).find(L"unrouted_tool · Running") != std::wstring::npos);
    Json unroutedRecords = Json::array();
    int unroutedLimit = 0;
    ApplyMcpEvent(unroutedRecords, unroutedLimit, unroutedTool, 0, 0);
    assert(unroutedRecords[0]["server"].is_null() &&
           McpSavedText(unroutedRecords[0]).find(L"unrouted_tool") != std::wstring::npos);
    assert(McpSavedText(Json{{"server", "files"}, {"tool", "read"}, {"arguments", {{"path", "x"}}},
                             {"result", std::string(5001, 'r')}, {"ok", true}, {"chars", 5001}}).size() > 5001);
    {
        FixtureApi api;
        const auto result = SaveThinkingBudget(api, 2048, api.budget);
        assert(result["budget"] == 2048);
        assert(api.calls == std::vector<std::string>{"POST budget"});
        const std::vector<std::wstring> expectedPaths = {L"/api/thinking-budget"};
        const std::vector<Json> expectedBodies = {Json{{"budget", 2048}}};
        assert(api.postPaths == expectedPaths && api.postBodies == expectedBodies);
    }
    {
        FixtureApi api;
        const auto result = SaveThinkingBudget(api, 1024, api.budget);
        assert(result["budget"] == 1024 && api.calls.empty());
    }
    {
        FixtureApi api;
        MustThrow([&] { SaveThinkingBudget(api, 128, api.budget); }, "0 or 256 to 262144");
        assert(api.calls.empty());
    }
    {
        FixtureApi api;
        api.budget["supported"] = false;
        MustThrow([&] { SaveThinkingBudget(api, 2048, api.budget); }, "does not support");
        assert(api.calls.empty());
    }
    {
        FixtureApi api;
        api.budget["supported"] = nullptr;
        MustThrow([&] { ApplySettings(api, 65536, 8082, 2048); }, "does not support");
        assert((api.calls == std::vector<std::string>{"GET context", "GET port", "GET budget"}));
        assert(api.postPaths.empty() && api.postBodies.empty());
    }
    {
        FixtureApi api;
        const auto result = ApplySettings(api, 65536, 8182, 2048);
        assert(result["context"] == 65536 && result["port"] == 8182 && result["budget"] == 2048);
        const std::vector<std::string> expected = {"GET context", "GET port",    "GET budget", "POST context",
                                                   "GET context", "POST budget", "POST port"};
        assert(api.calls == expected);
        const std::vector<std::wstring> expectedPaths = {L"/api/context", L"/api/thinking-budget", L"/api/port"};
        const std::vector<Json> expectedBodies = {Json{{"max_context", 65536}}, Json{{"budget", 2048}},
                                                  Json{{"port", 8182}}};
        assert(api.postPaths == expectedPaths && api.postBodies == expectedBodies);
    }
    {
        FixtureApi api;
        const auto result = ApplySettings(api, 32768, 8082, 1024);
        assert(result["port_restarts"] == true);
        assert(api.calls.size() == 3); // unchanged values issue no POSTs
    }
    {
        FixtureApi api;
        api.failReload = true;
        MustThrow([&] { ApplySettings(api, 65536, 8182, 2048); }, "fixture reload failure");
        assert(api.calls.size() == 5); // no budget or port mutation after a failed reload
        const std::vector<std::wstring> expectedPaths = {L"/api/context"};
        const std::vector<Json> expectedBodies = {Json{{"max_context", 65536}}};
        assert(api.postPaths == expectedPaths && api.postBodies == expectedBodies);
    }
    {
        FixtureApi api;
        MustThrow([&] { ApplySettings(api, 65536, 80, 2048); }, "1024 to 65535");
        assert(api.calls.size() == 3); // invalid input is rejected before any mutation
        assert(api.postPaths.empty() && api.postBodies.empty());
    }
    {
        FixtureApi api;
        const auto defaults = SharedChatDefaults(Json{{"reasoning_effort", "max"}, {"temperature", 0.6},
                                                       {"top_p", 0.9}, {"max_tokens", 1200}});
        const auto saved = SaveSharedDefaults(api, defaults);
        assert(saved["shared"] == true && saved["defaults"] == defaults);
        assert(api.calls == std::vector<std::string>{"POST shared"});
        assert(api.postPaths == std::vector<std::wstring>{L"/settings"});
        assert((api.postBodies == std::vector<Json>{Json{{"defaults", defaults}}}));
    }
    {
        FixtureApi api;
        const auto saved = SaveSharedDefaults(api, Json(nullptr));
        assert(saved["shared"] == false && saved["defaults"].empty());
        assert(api.calls == std::vector<std::string>{"POST shared"});
        assert(api.postPaths == std::vector<std::wstring>{L"/settings"});
        assert((api.postBodies == std::vector<Json>{Json{{"defaults", nullptr}}}));
    }
    std::cout << "Settings flow fixtures passed\n";
}

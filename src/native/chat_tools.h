#pragma once
#include "http.h"
#include "chat_metadata.h"
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>
#include <utility>

inline bool NativeMcpEnabled(bool preference, const Json &status)
{
    return preference && status.is_object() && status.contains("tools") && status["tools"].is_number_integer() &&
           status["tools"].get<int>() > 0;
}

inline bool NativeMcpRefreshEligible(bool visiblePage, bool canRunAlongsideWork, bool inFlight)
{
    return visiblePage && canRunAlongsideWork && !inFlight;
}

inline bool NativeToolRoundNeedsSeparator(const Json &tools, size_t currentLength, const char *offsetKey)
{
    if (!currentLength || !tools.is_array() || tools.empty())
        return false;
    const auto &last = tools.back();
    return last.is_object() && last.contains(offsetKey) && last[offsetKey].is_number_integer() &&
           last.value(offsetKey, size_t(-1)) == currentLength;
}

inline bool AppendNativeToolRoundText(std::string &stream, const Json &tools, const char *offsetKey,
                                      const std::string &part)
{
    const bool separator = NativeToolRoundNeedsSeparator(tools, stream.size(), offsetKey);
    if (separator)
        stream += "\n\n";
    stream += part;
    return separator;
}

inline size_t ChatUtf8PrefixCodeUnits(const std::string &text, size_t byteOffset)
{
    return Wide(text.substr(0, std::min(byteOffset, text.size()))).size();
}

inline bool NativeChatCancelReady(bool requested, bool sent, bool hasJobId) noexcept
{
    return requested && !sent && hasJobId;
}

inline bool NativeChatStopped(bool requested, bool serverStopped) noexcept
{
    return requested || serverStopped;
}

inline std::string NativeChatFailureText(bool explicitlyStopped, const std::string &failure)
{
    return explicitlyStopped ? std::string() : failure;
}

inline std::string McpTextField(const Json &object, const char *key, std::string fallback = {})
{
    return object.is_object() && object.contains(key) && object[key].is_string()
               ? object[key].get<std::string>() : std::move(fallback);
}

inline void ApplyMcpEvent(Json &tools, int &limit, const Json &event, size_t textAt, size_t reasoningAt)
{
    if (!event.is_object())
        return;
    const auto kind = McpTextField(event, "event");
    if (kind == "limit")
    {
        if (event.contains("max_rounds") && event["max_rounds"].is_number_integer())
            limit = event["max_rounds"].get<int>();
        return;
    }
    if (kind != "start" && kind != "call" && kind != "result")
        return;
    if (!tools.is_array())
        tools = Json::array();
    const auto id = McpTextField(event, "id");
    auto found = std::find_if(tools.begin(), tools.end(), [&](const Json &tool) {
        return tool.is_object() && McpTextField(tool, "id") == id;
    });
    if (found == tools.end())
    {
        tools.push_back({{"id", id}, {"at", textAt}, {"rat", reasoningAt}, {"state", "writing"}});
        found = std::prev(tools.end());
    }
    if (kind == "start")
    {
        if (event.contains("name") && event["name"].is_string()) (*found)["name"] = event["name"];
        (*found)["state"] = "writing";
    }
    else if (kind == "call")
    {
        for (const auto key : {"name", "server", "tool", "arguments", "round"})
            if (event.contains(key))
                (*found)[key] = event[key];
        (*found)["at"] = textAt;
        (*found)["rat"] = reasoningAt;
        (*found)["state"] = "running";
    }
    else
    {
        (*found)["result"] = McpTextField(event, "text");
        (*found)["ok"] = event.value("ok", false);
        (*found)["chars"] = event.value("chars", 0);
        (*found)["truncated"] = event.value("truncated", false);
        (*found)["ms"] = event.value("ms", 0);
        (*found)["state"] = event.value("skipped", false) ? "skipped" : event.value("ok", false) ? "done" : "error";
    }
}

inline void FinalizeMcpEvents(Json &tools)
{
    if (!tools.is_array()) return;
    for (auto &tool : tools)
        if (tool.is_object() && (tool.value("state", std::string()) == "writing" ||
                                 tool.value("state", std::string()) == "running"))
        {
            tool["state"] = "skipped";
            tool["ms"] = nullptr;
        }
}

inline std::wstring McpProgressText(const Json &event)
{
    const auto kind = McpTextField(event, "event");
    if (kind == "limit")
        return L"[Tool calls stopped at the limit of " +
               std::to_wstring(event.value("max_rounds", 0)) + L" rounds]\r\n";
    const auto server = McpTextField(event, "server");
    const auto tool = McpTextField(event, "tool", McpTextField(event, "name", "tool"));
    const auto label = server.empty() ? tool : server + "/" + tool;
    if (kind == "call")
    {
        const auto args = event.value("arguments", Json::object());
        const auto json = args.dump(2);
        return L"[Tool " + std::wstring(label.begin(), label.end()) + L" · Running]\r\nArguments:\r\n" +
               std::wstring(json.begin(), json.end()) + L"\r\n";
    }
    if (kind == "result")
    {
        const auto state = event.value("skipped", false) ? L"Not run" : event.value("ok", false) ? L"Done" : L"Error";
        auto result = McpTextField(event, "text");
        const bool cut = result.size() > 4000;
        if (cut)
            result.resize(4000);
        auto line = L"[Tool " + std::wstring(label.begin(), label.end()) + L" · " + state + L" · " +
                    std::to_wstring(event.value("chars", 0)) + L" chars · " +
                    std::to_wstring(event.value("ms", 0)) + L" ms]\r\n" +
                    std::wstring(result.begin(), result.end());
        return line + (cut ? L"\r\n[Result preview shortened; full result is saved with this chat.]\r\n" : L"\r\n");
    }
    return {};
}

inline std::wstring McpCompactPreview(const Json &event)
{
    std::string value;
    if (McpTextField(event, "event") == "result")
        value = McpTextField(event, "text", McpTextField(event, "result"));
    else if (event.is_object() && event.contains("arguments") && !event["arguments"].is_null())
        value = event["arguments"].dump();
    auto preview = Wide(value);
    if (McpTextField(event, "event") != "result")
    {
        std::wstring compact;
        bool space = false;
        for (const wchar_t c : preview)
            if (std::iswspace(c)) space = !compact.empty();
            else
            {
                if (space) compact.push_back(L' ');
                compact.push_back(c);
                space = false;
            }
        preview = std::move(compact);
    }
    if (preview.size() > 200) preview.resize(200);
    return preview;
}

inline std::wstring McpCompactText(const Json &event)
{
    const auto kind = McpTextField(event, "event");
    if (kind == "limit") return McpProgressText(event);
    const auto server = McpTextField(event, "server");
    const auto tool = McpTextField(event, "tool", McpTextField(event, "name", "tool"));
    const auto label = server.empty() ? tool : server + "/" + tool;
    const auto preview = McpCompactPreview(event);
    if (kind == "start") return L"[Tool " + std::wstring(label.begin(), label.end()) + L" · Writing]" +
        (preview.empty() ? L"" : L" " + preview) + L"\r\n";
    if (kind == "call") return L"[Tool " + std::wstring(label.begin(), label.end()) + L" · Running]" +
        (preview.empty() ? L"" : L" " + preview) + L"\r\n";
    if (kind == "result")
    {
        const auto state = event.value("skipped", false) ? L"Not run" : event.value("ok", false) ? L"Done" : L"Error";
        return L"[Tool " + std::wstring(label.begin(), label.end()) + L" · " + state + L" · " +
               std::to_wstring(event.value("chars", 0)) + L" chars · " +
               std::to_wstring(event.value("ms", 0)) + L" ms]" +
               (preview.empty() ? L"" : L" " + preview) + L"\r\n";
    }
    return {};
}

inline std::wstring McpSavedText(const Json &tool)
{
    const auto server = McpTextField(tool, "server");
    const auto name = McpTextField(tool, "tool", McpTextField(tool, "name", "tool"));
    const auto label = server.empty() ? name : server + "/" + name;
    if (McpTextField(tool, "state") == "writing")
        return L"[Tool " + std::wstring(label.begin(), label.end()) + L" · Writing]\r\nArguments:\r\n(being written)\r\n";
    const auto args = tool.contains("arguments") ? tool["arguments"].dump(2) : std::string("{}");
    const auto state = McpTextField(tool, "state") == "running" ? L" · Running" : L"";
    auto out = L"[Tool " + std::wstring(label.begin(), label.end()) + state + L"]\r\nArguments:\r\n" +
               std::wstring(args.begin(), args.end()) + L"\r\n";
    if (tool.contains("result"))
    {
        const auto state = McpTextField(tool, "state") == "skipped" ? L"Not run" :
                           tool.value("ok", false) ? L"Result" : L"Error";
        const auto result = McpTextField(tool, "result");
        out += L"[" + std::wstring(state) + L" · " + std::to_wstring(tool.value("chars", 0)) + L" chars · " +
               std::to_wstring(tool.value("ms", 0)) + L" ms" +
               (tool.value("truncated", false) ? L" · truncated for the model" : L"") + L"]\r\n" +
               std::wstring(result.begin(), result.end()) + L"\r\n";
    }
    return out;
}

inline std::string TrimAssistantApiText(const std::string &text)
{
    const auto wide = Wide(text);
    return Utf8(TrimAndLimitName(wide, wide.size()));
}

inline Json AssistantToolArgumentsForApi(const Json &value)
{
    if (value.is_null() || (value.is_boolean() && !value.get<bool>()) ||
        (value.is_number() && value.get<double>() == 0.0) ||
        (value.is_string() && value.get_ref<const std::string &>().empty()))
        return Json::object();
    return value;
}

inline Json AssistantApiMessages(const std::string &text, const Json &tools, bool stopped = false, bool errored = false,
                                 const std::string &reasoning = {}, bool keepReasoning = false)
{
    Json out = Json::array();
    if (errored)
        return out;
    std::vector<int> rounds;
    if (tools.is_array())
        for (const auto &tool : tools)
            if (tool.is_object() && tool.contains("round") && tool["round"].is_number_integer() &&
                tool.contains("result") && !tool["result"].is_null() &&
                tool.value("state", std::string()) != "skipped")
            {
                const int round = tool["round"].get<int>();
                if (std::find(rounds.begin(), rounds.end(), round) == rounds.end())
                    rounds.push_back(round);
            }
    if (rounds.empty())
    {
        if (!text.empty() || stopped)
        {
            Json message = {{"role", "assistant"}, {"content", text}};
            if (keepReasoning && !text.empty() && !TrimAssistantApiText(reasoning).empty())
                message["reasoning_content"] = reasoning;
            out.push_back(std::move(message));
        }
        return out;
    }
    size_t pos = 0, reasoningPos = 0;
    for (const auto round : rounds)
    {
        Json calls = Json::array();
        std::vector<const Json *> called;
        size_t at = text.size();
        for (const auto &tool : tools)
            if (tool.value("round", -1) == round && tool.contains("result") && !tool["result"].is_null() &&
                tool.value("state", std::string()) != "skipped")
            {
                called.push_back(&tool);
                at = std::min(at, (size_t)tool.value("at", uint64_t(0)));
            }
        at = std::max(pos, std::min(at, text.size()));
        for (const auto *tool : called)
            calls.push_back({{"id", tool->value("id", std::string())}, {"type", "function"},
                             {"function", {{"name", tool->value("name", std::string("tool"))},
                                           {"arguments", AssistantToolArgumentsForApi(
                                               tool->value("arguments", Json::object())).dump()}}}});
        size_t rat = 0;
        if (!called.empty())
        {
            const auto value = called.front()->find("rat");
            if (value != called.front()->end() && value->is_number_unsigned())
                rat = static_cast<size_t>(std::min<uint64_t>(value->get<uint64_t>(), reasoning.size()));
            else if (value != called.front()->end() && value->is_number_integer())
            {
                const auto offset = value->get<int64_t>();
                if (offset > 0) rat = std::min<size_t>(static_cast<size_t>(offset), reasoning.size());
            }
        }
        rat = std::clamp(rat, reasoningPos, reasoning.size());
        Json message = {{"role", "assistant"}, {"content", TrimAssistantApiText(text.substr(pos, at - pos))},
                       {"tool_calls", calls}};
        const auto thought = reasoning.substr(reasoningPos, rat - reasoningPos);
        if (keepReasoning && !TrimAssistantApiText(thought).empty())
            message["reasoning_content"] = thought;
        out.push_back(std::move(message));
        for (const auto *tool : called)
            out.push_back({{"role", "tool"}, {"tool_call_id", tool->value("id", std::string())},
                           {"content", tool->value("result", std::string())}});
        pos = at;
        reasoningPos = rat;
    }
    const auto rest = TrimAssistantApiText(text.substr(pos));
    if (!rest.empty())
    {
        Json message = {{"role", "assistant"}, {"content", rest}};
        const auto thought = reasoning.substr(reasoningPos);
        if (keepReasoning && !TrimAssistantApiText(thought).empty())
            message["reasoning_content"] = thought;
        out.push_back(std::move(message));
    }
    return out;
}

inline std::string McpStatusLabel(const std::string &state)
{
    if (state == "ready") return "Connected";
    if (state == "starting") return "Starting";
    if (state == "failed") return "Failed";
    if (state == "stopped") return "Stopped";
    if (state == "idle") return "Waiting";
    return state;
}

inline std::string McpStatusReport(const Json &status)
{
    const auto servers = status.is_object() ? status.value("servers", Json::array()) : Json::array();
    if (!servers.is_array() || servers.empty())
        return "No MCP servers are configured. Add them to the model run config or start Maya with --mcp-config.";
    const auto count = status.is_object() && status.value("tools", Json()).is_number()
        ? status["tools"].get<int>() : 0;
    int ready = 0;
    for (const auto &server : servers)
        if (server.is_object() && (server.value("status", std::string()) == "ready" ||
                                   server.value("status", std::string()) == "stopped"))
            ++ready;
    std::ostringstream out;
    out << "MCP TOOL SERVERS · " << count << " tools · " << ready << " of " << servers.size() << " connected\r\n";
    for (const auto &server : servers)
    {
        if (!server.is_object()) continue;
        const auto name = server.value("name", std::string("MCP server"));
        const auto transport = server.value("transport", std::string("unknown"));
        const auto state = McpStatusLabel(server.value("status", std::string("unknown")));
        const auto tools = server.value("tools", Json::array());
        out << "\r\n" << name << " · " << state << " · " << transport << " · "
            << (tools.is_array() ? tools.size() : 0) << " tools\r\n";
        const auto info = server.value("info", Json::object());
        if (info.is_object() && info.contains("name") && info["name"].is_string())
        {
            const auto serverName = info["name"].get<std::string>();
            out << "  " << serverName;
            if (info.contains("version") && info["version"].is_string())
            {
                const auto version = info["version"].get<std::string>();
                out << " " << version;
            }
            out << "\r\n";
        }
        if (server.contains("error") && server["error"].is_string() && !server["error"].get<std::string>().empty())
        {
            const auto error = server["error"].get<std::string>();
            out << "  Error: " << error << "\r\n";
        }
        if (tools.is_array())
            for (const auto &tool : tools)
                if (tool.is_object() && tool.contains("tool") && tool["tool"].is_string())
                {
                    const auto toolName = tool["tool"].get<std::string>();
                    const auto description = tool.contains("description") && tool["description"].is_string()
                        ? tool["description"].get<std::string>() : std::string();
                    out << "  · " << toolName;
                    if (!description.empty()) out << " — " << description;
                    out << "\r\n";
                }
    }
    return out.str();
}

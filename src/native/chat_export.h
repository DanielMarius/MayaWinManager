#pragma once
#include "http.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

inline std::wstring ChatExportFilename(const SYSTEMTIME &updated)
{
    std::wostringstream out;
    out << L"maya-chat-" << std::setfill(L'0') << std::setw(4) << updated.wYear << L'-' << std::setw(2)
        << updated.wMonth << L'-' << std::setw(2) << updated.wDay << L'-' << std::setw(2) << updated.wHour << L'-'
        << std::setw(2) << updated.wMinute << L".md";
    return out.str();
}

struct ChatExportMessage
{
    std::string role, text, reasoning;
    std::vector<Json> tools;
    std::string error;
};

inline bool ChatExportTruthy(const Json &value)
{
    if (value.is_null()) return false;
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number()) return value.get<double>() != 0;
    if (value.is_string()) return !value.get_ref<const std::string &>().empty();
    return true;
}

inline std::string ChatExportNumberString(double value)
{
    if (value == 0) return "0";
    char buffer[128];
    const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general);
    if (converted.ec != std::errc()) return std::to_string(value);
    std::string text(buffer, converted.ptr);
    const auto e = text.find_first_of("eE");
    if (e == std::string::npos) return text;
    const int exponent = std::stoi(text.substr(e + 1));
    const bool negative = text[0] == '-';
    std::string mantissa = text.substr(negative ? 1 : 0, e - (negative ? 1 : 0));
    const auto dot = mantissa.find('.');
    const int decimal = (int)(dot == std::string::npos ? mantissa.size() : dot) + exponent;
    mantissa.erase(std::remove(mantissa.begin(), mantissa.end(), '.'), mantissa.end());
    if (exponent >= -6 && exponent < 21)
    {
        std::string fixed;
        if (decimal <= 0) fixed = "0." + std::string((size_t)-decimal, '0') + mantissa;
        else if (decimal >= (int)mantissa.size()) fixed = mantissa + std::string((size_t)(decimal - mantissa.size()), '0');
        else fixed = mantissa.substr(0, (size_t)decimal) + "." + mantissa.substr((size_t)decimal);
        return (negative ? "-" : "") + fixed;
    }
    return (negative ? "-" : "") + mantissa + "e" + (exponent >= 0 ? "+" : "") + std::to_string(exponent);
}

inline std::string ChatExportPrettyJson(const Json &value, size_t depth = 0)
{
    if (value.is_number()) return ChatExportNumberString(value.get<double>());
    if (value.is_primitive()) return value.dump();
    const bool array = value.is_array();
    const char open = array ? '[' : '{', close = array ? ']' : '}';
    if (value.empty()) return array ? "[]" : "{}";
    std::string out(1, open);
    for (auto it = value.begin(); it != value.end(); ++it)
    {
        out += "\n" + std::string((depth + 1) * 2, ' ');
        if (!array) out += Json(it.key()).dump() + ": ";
        out += ChatExportPrettyJson(it.value(), depth + 1);
        if (std::next(it) != value.end()) out += ',';
    }
    return out + "\n" + std::string(depth * 2, ' ') + close;
}

inline std::string ChatExportValueString(const Json &value)
{
    if (value.is_string()) return value.get<std::string>();
    if (value.is_number()) return ChatExportNumberString(value.get<double>());
    if (value.is_object()) return "[object Object]";
    if (value.is_array())
    {
        std::string out;
        for (size_t i = 0; i < value.size(); ++i)
        {
            if (i) out += ',';
            if (!value[i].is_null()) out += ChatExportValueString(value[i]);
        }
        return out;
    }
    return value.dump();
}

inline std::string ChatMarkdown(const std::string &title, const std::string &instructions, const std::string &model,
                                const std::vector<ChatExportMessage> &messages)
{
    std::string out = "# " + title + "\n\n";
    if (!instructions.empty())
    {
        std::string quoted;
        quoted.reserve(instructions.size());
        for (size_t i = 0; i < instructions.size(); ++i)
        {
            if (instructions[i] == '\r' || instructions[i] == '\n')
            {
                if (instructions[i] == '\r' && i + 1 < instructions.size() && instructions[i + 1] == '\n') ++i;
                quoted.push_back(' ');
            }
            else quoted.push_back(instructions[i]);
        }
        out += "> Instructions: " + quoted + "\n\n";
    }
    for (size_t i = 0; i < messages.size(); ++i)
    {
        const auto &message = messages[i];
        if (message.role == "user")
            out += "## You\n\n";
        else
        {
            out += "## " + model + "\n\n";
            if (!message.reasoning.empty())
                out += "<details><summary>Thinking</summary>\n\n" + message.reasoning + "\n\n</details>\n\n";
        }
        for (const auto &tool : message.tools)
        {
            if (!tool.is_object() || !tool.contains("result") || tool["result"].is_null())
                continue;
            const auto server = tool.contains("server") && ChatExportTruthy(tool["server"])
                                    ? ChatExportValueString(tool["server"]) : std::string();
            const auto name = tool.contains("tool") && ChatExportTruthy(tool["tool"])
                                  ? ChatExportValueString(tool["tool"])
                                  : tool.contains("name") && ChatExportTruthy(tool["name"])
                                        ? ChatExportValueString(tool["name"]) : std::string();
            const auto arguments = tool.contains("arguments") && ChatExportTruthy(tool["arguments"])
                                       ? tool["arguments"] : Json::object();
            const auto result = ChatExportValueString(tool["result"]);
            const bool ok = tool.contains("ok") && ChatExportTruthy(tool["ok"]);
            out += "<details><summary>Tool " + (server.empty() ? std::string() : server + " / ") + name +
                   (ok ? "" : " (error)") + "</summary>\n\n```json\n" + ChatExportPrettyJson(arguments) + "\n```\n\n```\n" +
                   result + "\n```\n\n</details>\n\n";
        }
        out += message.text.empty() ? message.error : message.text;
        out += i + 1 < messages.size() ? "\n\n" : "\n";
    }
    return out;
}

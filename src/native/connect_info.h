#pragma once
#include <array>
#include <string>
#include <utility>

struct ApiConnectInfo
{
    std::string openaiBase, anthropicBase, model, authentication;
};

inline std::string ApiConnectDisplayHost(const std::string &configuredHost, const std::string &lanHost)
{
    if (configuredHost != "0.0.0.0") return configuredHost;
    return lanHost.empty() ? "127.0.0.1" : lanHost;
}

inline std::string PowerShellLiteral(const std::string &value)
{
    std::string out = "'";
    for (const char c : value)
    {
        out.push_back(c);
        if (c == '\'') out.push_back('\'');
    }
    out.push_back('\'');
    return out;
}

inline ApiConnectInfo BuildApiConnectInfo(std::string host, unsigned short port, std::string model,
                                          bool keyRequired, bool keyConfigured)
{
    if (host == "0.0.0.0") host = "127.0.0.1";
    else if (host == "::") host = "::1";
    if (host.find(':') != std::string::npos && (host.empty() || host.front() != '[')) host = '[' + host + ']';
    const auto base = "http://" + host + ':' + std::to_string(port);
    return {base + "/v1", base, std::move(model),
            !keyRequired ? "No API key required" : keyConfigured ? "API key supplied by selected profile"
                                                                    : "API key required but missing from selected profile"};
}

inline std::array<std::string, 3> BuildApiConnectExamples(const ApiConnectInfo &info, bool keyRequired)
{
    const auto key = keyRequired ? "<your API key>" : "maya";
    const auto curlAuth = keyRequired ? " -H \"Authorization: Bearer <your API key>\"" : "";
    const auto claude = "$env:ANTHROPIC_BASE_URL = " + PowerShellLiteral(info.anthropicBase) + "\r\n" +
        "$env:ANTHROPIC_AUTH_TOKEN = " + PowerShellLiteral(key) + "\r\nclaude";
    const auto openai = "$env:OPENAI_BASE_URL = " + PowerShellLiteral(info.openaiBase) + "\r\n" +
        "$env:OPENAI_API_KEY = " + PowerShellLiteral(key) + "\r\n$env:MAYA_MODEL = " + PowerShellLiteral(info.model);
    const auto curl = "$body = @{ model = " + PowerShellLiteral(info.model) +
        "; messages = @(@{ role = 'user'; content = 'Hello!' }) } | ConvertTo-Json -Depth 5 -Compress\r\n" +
        "curl.exe " + PowerShellLiteral(info.openaiBase + "/chat/completions") +
        " -H 'Content-Type: application/json'" + curlAuth + " --data-raw $body";
    return {claude, openai, curl};
}

#pragma once
#include "chat_metadata.h"
#include <algorithm>
#include <cwctype>
#include <string>
#include <utility>

inline std::wstring MayaCodeLanguageToken(std::wstring language)
{
    language = TrimChatInput(language);
    const auto end = std::find_if(language.begin(), language.end(), ChatInputTrimWhitespace);
    language.erase(end, language.end());
    for (auto &c : language) c = (wchar_t)towlower(c);
    return language;
}

inline size_t MayaFirstCodeContent(const std::wstring &source)
{
    return (size_t)std::distance(source.begin(), std::find_if_not(source.begin(), source.end(), ChatInputTrimWhitespace));
}

inline bool MayaIsHtmlCode(const std::wstring &language, const std::wstring &source)
{
    if (language == L"html" || language == L"htm" || language == L"xhtml") return true;
    if (!language.empty()) return false;
    const auto first = MayaFirstCodeContent(source);
    if (first == source.size()) return false;
    auto lower = source;
    for (auto &c : lower) c = (wchar_t)towlower(c);
    return lower.compare(first, 14, L"<!doctype html") == 0 ||
           (lower.compare(first, 5, L"<html") == 0 && first + 5 < lower.size() &&
            (ChatInputTrimWhitespace(lower[first + 5]) || lower[first + 5] == L'>'));
}

inline std::wstring MayaCodeExtension(std::wstring language, const std::wstring &source)
{
    language = MayaCodeLanguageToken(std::move(language));
    if (language == L"python" || language == L"py") return L"py";
    if (language == L"javascript" || language == L"js") return L"js";
    if (language == L"jsx") return L"jsx";
    if (language == L"typescript" || language == L"ts") return L"ts";
    if (language == L"tsx") return L"tsx";
    if (language == L"html" || language == L"htm" || language == L"xhtml") return L"html";
    if (language == L"css") return L"css";
    if (language == L"scss") return L"scss";
    if (language == L"json") return L"json";
    if (language == L"bash" || language == L"sh" || language == L"shell" || language == L"zsh") return L"sh";
    if (language == L"c" || language == L"h") return language;
    if (language == L"cpp" || language == L"c++") return L"cpp";
    if (language == L"hpp") return L"hpp";
    if (language == L"cuda" || language == L"cu") return L"cu";
    if (language == L"rust" || language == L"rs") return L"rs";
    if (language == L"go") return L"go";
    if (language == L"java") return L"java";
    if (language == L"kotlin") return L"kt";
    if (language == L"swift") return L"swift";
    if (language == L"sql") return L"sql";
    if (language == L"yaml") return L"yaml";
    if (language == L"yml") return L"yml";
    if (language == L"toml") return L"toml";
    if (language == L"xml") return L"xml";
    if (language == L"svg") return L"svg";
    if (language == L"markdown" || language == L"md") return L"md";
    if (language == L"ps1" || language == L"powershell") return L"ps1";
    if (language == L"php") return L"php";
    if (language == L"ruby" || language == L"rb") return L"rb";
    if (language == L"lua") return L"lua";
    if (language == L"dockerfile") return L"Dockerfile";
    if (!language.empty()) return L"txt";
    return MayaIsHtmlCode(language, source) ? L"html" : L"txt";
}

inline std::wstring MayaCodeFilename(const std::wstring &extension)
{
    return extension == L"Dockerfile" ? extension : L"maya-code." + extension;
}

inline std::string MayaSandboxPreviewDocument(const std::string &source)
{
    std::string srcdoc;
    srcdoc.reserve(source.size() + source.size() / 8);
    for (const char c : source)
    {
        switch (c)
        {
        case '&': srcdoc += "&amp;"; break;
        case '"': srcdoc += "&quot;"; break;
        case '<': srcdoc += "&lt;"; break;
        case '>': srcdoc += "&gt;"; break;
        default: srcdoc += c; break;
        }
    }
    return "<!doctype html><html><head><meta charset=\"utf-8\"><title>Maya Preview</title>"
           "<style>html,body{height:100%;margin:0}body{font:14px Segoe UI,sans-serif;background:#0b0c16;color:#aab0d0}"
           "p{margin:0;padding:10px 14px}iframe{display:block;width:100%;height:calc(100% - 40px);border:0;background:white}"
           "</style></head><body><p>Runs in a sandbox, cut off from this page. Scripts it loads from the web come from the internet.</p>"
           "<iframe title=\"Preview\" sandbox=\"allow-scripts allow-pointer-lock allow-modals\" srcdoc=\"" +
           srcdoc + "\"></iframe></body></html>";
}

inline bool MayaHasHtmlPreview(bool complete, std::wstring language, const std::wstring &source)
{
    if (!complete) return false;
    language = MayaCodeLanguageToken(std::move(language));
    return MayaIsHtmlCode(language, source);
}

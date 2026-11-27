#include "chat_markdown.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static bool HasKeyword(const std::vector<ChatMarkdownSpan> &spans, const std::wstring &word)
{
    return std::any_of(spans.begin(), spans.end(), [&](const ChatMarkdownSpan &span) {
        return span.style == ChatMarkdownStyle::CODE_KEYWORD && span.text == word;
    });
}

static bool HasStyle(const std::vector<ChatMarkdownSpan> &spans, const std::wstring &word, ChatMarkdownStyle style)
{
    return std::any_of(spans.begin(), spans.end(), [&](const ChatMarkdownSpan &span) {
        return span.style == style && span.text == word;
    });
}

int main()
{
    const std::vector<std::wstring> shellWords = {
        L"if",L"then",L"else",L"elif",L"fi",L"for",L"while",L"until",L"do",L"done",L"case",L"esac",L"in",
        L"function",L"return",L"export",L"local",L"set",L"unset",L"echo",L"printf",L"cd",L"sudo",L"source",
        L"alias",L"exit",L"read",L"shift",L"trap",L"eval",L"exec"};
    for (const auto &word : shellWords)
        assert(HasKeyword(ChatSyntaxSpans(word, L"bash"), word));

    for (const auto &word : {L"then", L"echo", L"source", L"trap"})
        assert(!HasKeyword(ChatSyntaxSpans(word, L"cpp"), word));

    const auto lowercaseDirective = ChatSyntaxSpans(L"#include", L"cpp");
    assert(HasStyle(lowercaseDirective, L"#include", ChatMarkdownStyle::CODE_KEYWORD));
    const auto uppercaseDirective = ChatSyntaxSpans(L"#INCLUDE", L"cpp");
    assert(HasStyle(uppercaseDirective, L"#INCLUDE", ChatMarkdownStyle::CODE));
    assert(!HasStyle(uppercaseDirective, L"#INCLUDE", ChatMarkdownStyle::CODE_KEYWORD));

    assert(HasKeyword(ChatSyntaxSpans(L"echo", L"powershell"), L"echo"));
    const auto yamlWords = ChatSyntaxSpans(L"on-value off.value", L"yaml");
    assert(HasKeyword(yamlWords, L"on"));
    assert(HasKeyword(yamlWords, L"off"));
    assert(!HasKeyword(yamlWords, L"on-value"));
    const auto yamlKey = ChatSyntaxSpans(L"on-value: true", L"yaml");
    assert(HasStyle(yamlKey, L"on-value", ChatMarkdownStyle::CODE_ATTRIBUTE));
    assert(!HasKeyword(yamlKey, L"on"));
    const auto yamlWordDigit = ChatSyntaxSpans(L"on1", L"yaml");
    assert(HasKeyword(yamlWordDigit, L"on"));
    assert(HasStyle(yamlWordDigit, L"1", ChatMarkdownStyle::CODE));
    assert(!HasStyle(yamlWordDigit, L"1", ChatMarkdownStyle::CODE_NUMBER));
    assert(HasStyle(ChatSyntaxSpans(L"on-1", L"yaml"), L"1", ChatMarkdownStyle::CODE_NUMBER));
    const auto cssExtraPercent = ChatSyntaxSpans(L"1px% 1s% 33%", L"css");
    assert(HasStyle(cssExtraPercent, L"1px", ChatMarkdownStyle::CODE_NUMBER));
    assert(HasStyle(cssExtraPercent, L"1s", ChatMarkdownStyle::CODE_NUMBER));
    assert(HasStyle(cssExtraPercent, L"33%", ChatMarkdownStyle::CODE_NUMBER));
    assert(HasStyle(cssExtraPercent, L"% ", ChatMarkdownStyle::CODE));
    const auto jsonWordNumber = ChatSyntaxSpans(L"true1", L"json");
    assert(HasKeyword(jsonWordNumber, L"true"));
    assert(HasStyle(jsonWordNumber, L"1", ChatMarkdownStyle::CODE_NUMBER));
    const auto jsonPlainNumber = ChatSyntaxSpans(L"foo1", L"json");
    assert(HasStyle(jsonPlainNumber, L"foo", ChatMarkdownStyle::CODE));
    assert(HasStyle(jsonPlainNumber, L"1", ChatMarkdownStyle::CODE_NUMBER));
    const auto jsonUnderscore = ChatSyntaxSpans(L"_true1", L"json");
    assert(HasStyle(jsonUnderscore, L"_", ChatMarkdownStyle::CODE));
    assert(HasKeyword(jsonUnderscore, L"true"));
    std::cout << "v1.1.50 shell, YAML, JSON, and CSS syntax-highlighting parity passed.\n";
    return 0;
}

#pragma once
#include "chat_metadata.h"
#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <unordered_set>
#include <vector>
#include <cwctype>

enum class ChatMarkdownStyle { TEXT, EMPHASIS, ITALIC, STRIKETHROUGH, HEADING, SUBHEADING, CODE, CODE_COMMENT, CODE_STRING, CODE_KEYWORD,
    CODE_NUMBER, CODE_FUNCTION, CODE_ATTRIBUTE, CODE_VARIABLE, QUOTE, LINK, LIST, TABLE_HEADER, TABLE_ROW, HORIZONTAL_RULE };
struct ChatMarkdownSpan { std::wstring text, target; ChatMarkdownStyle style; bool fenced = false; bool tableCell = false; int listDepth = -1; ChatMarkdownStyle headingStyle = ChatMarkdownStyle::TEXT; int paragraphSpaceBeforeTwips = 0; bool blockQuote = false; };

inline std::vector<ChatMarkdownSpan> ChatSyntaxSpans(const std::wstring &code, std::wstring lang)
{
    for (auto &c : lang) c = (wchar_t)towlower(c);
    const bool python = lang == L"py" || lang == L"python" || lang == L"python3";
    const bool shell = lang == L"sh" || lang == L"bash" || lang == L"shell" || lang == L"zsh" ||
                       lang == L"ps1" || lang == L"powershell" || lang == L"bat" || lang == L"cmd" || lang == L"console";
    const bool json = lang == L"json" || lang == L"jsonc" || lang == L"json5";
    const bool sql = lang == L"sql";
    const bool html = lang == L"html" || lang == L"htm" || lang == L"xhtml" || lang == L"xml" || lang == L"svg" || lang == L"vue";
    const bool css = lang == L"css" || lang == L"scss" || lang == L"less";
    const bool yaml = lang == L"yaml" || lang == L"yml" || lang == L"toml" || lang == L"ini" ||
                      lang == L"cfg" || lang == L"conf" || lang == L"env" || lang == L"dockerfile";
    const bool known = python || shell || json || sql || yaml || html || css || lang == L"js" || lang == L"javascript" ||
        lang == L"jsx" || lang == L"mjs" || lang == L"cjs" || lang == L"ts" || lang == L"typescript" || lang == L"tsx" || lang == L"c" ||
        lang == L"h" || lang == L"cpp" || lang == L"cc" || lang == L"cxx" || lang == L"c++" || lang == L"hpp" || lang == L"java" ||
        lang == L"go" || lang == L"golang" || lang == L"rust" || lang == L"rs" || lang == L"cs" || lang == L"csharp" ||
        lang == L"swift" || lang == L"kotlin" || lang == L"kt" || lang == L"php" || lang == L"dart" ||
        lang == L"scala" || lang == L"cu" || lang == L"cuh" || lang == L"cuda" || lang == L"glsl" || lang == L"hlsl" ||
        lang == L"wgsl" || lang == L"zig";
    const bool cFamily = known && !(python || shell || json || sql || yaml || html || css);
    const auto asciiLetter = [](wchar_t c) { return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z'); };
    const auto asciiWord = [&](wchar_t c) { return asciiLetter(c) || (c >= L'0' && c <= L'9') || c == L'_'; };
    std::vector<ChatMarkdownSpan> out;
    auto add = [&](size_t at, size_t length, ChatMarkdownStyle style) {
        if (!length) return;
        auto text = code.substr(at, length);
        if (!out.empty() && out.back().style == style) out.back().text += text;
        else out.push_back({std::move(text), L"", style, false});
    };
    if (!known || code.size() > 200000) { out.push_back({code, L"", ChatMarkdownStyle::CODE, false}); return out; }
    size_t i = 0;
    const auto pyPrefix = [](wchar_t c) { return c == L'r' || c == L'b' || c == L'f' || c == L'u' || c == L'R' || c == L'B' || c == L'F' || c == L'U'; };
    auto matchesPythonStringPrefix = [&](size_t at) {
        size_t quoteAt = at;
        while (quoteAt < code.size() && quoteAt - at < 2 && pyPrefix(code[quoteAt])) ++quoteAt;
        return quoteAt > at && quoteAt < code.size() && (code[quoteAt] == L'\'' || code[quoteAt] == L'"');
    };
    auto consumeString = [&](size_t start, size_t quoteAt, ChatMarkdownStyle style) {
        const auto quote = code[quoteAt];
        i = quoteAt + 1;
        if (python && i + 1 < code.size() && code[i] == quote && code[i + 1] == quote)
        {
            i += 2;
            while (i + 2 < code.size() && !(code[i] == quote && code[i + 1] == quote && code[i + 2] == quote)) ++i;
            i = i + 2 < code.size() ? i + 3 : code.size();
        }
        else while (i < code.size())
        {
            if (code[i] == L'\\' && i + 1 < code.size()) { i += 2; continue; }
            if (quote != L'`' && code[i] == L'\n') break;
            if (code[i++] == quote) break;
        }
        add(start, i - start, style);
    };
    auto consumeNumber = [&](size_t start) {
        const auto digit = [](wchar_t c) { return c >= L'0' && c <= L'9'; };
        if (json)
        {
            if (code[i] == L'-') ++i;
            while (i < code.size() && digit(code[i])) ++i;
            if (i < code.size() && code[i] == L'.')
            {
                ++i;
                while (i < code.size() && digit(code[i])) ++i;
            }
            if (i < code.size() && (code[i] == L'e' || code[i] == L'E'))
            {
                const size_t exponent = i++;
                if (i < code.size() && (code[i] == L'+' || code[i] == L'-')) ++i;
                const size_t digits = i;
                while (i < code.size() && digit(code[i])) ++i;
                if (digits == i) i = exponent;
            }
        }
        else if (css)
        {
            if (code[i] == L'-') ++i;
            while (i < code.size() && digit(code[i])) ++i;
            if (i < code.size() && code[i] == L'.')
            {
                ++i;
                while (i < code.size() && digit(code[i])) ++i;
            }
            if (code.compare(i, 2, L"px") == 0 || code.compare(i, 2, L"em") == 0 || code.compare(i, 2, L"vh") == 0 ||
                code.compare(i, 2, L"vw") == 0 || code.compare(i, 2, L"ms") == 0 || code.compare(i, 2, L"fr") == 0) i += 2;
            else if (code.compare(i, 3, L"rem") == 0 || code.compare(i, 3, L"deg") == 0) i += 3;
            else if (i < code.size() && (code[i] == L'%' || code[i] == L's')) ++i;
        }
        else
        {
            bool prefixed = false;
            if (i + 2 < code.size() && code[i] == L'0' && code[i + 1] == L'x')
            {
                size_t end = i + 2;
                while (end < code.size() && ((code[end] >= L'0' && code[end] <= L'9') ||
                       (code[end] >= L'a' && code[end] <= L'f') || (code[end] >= L'A' && code[end] <= L'F') || code[end] == L'_')) ++end;
                if (end > i + 2) { i = end; prefixed = true; }
            }
            else if (i + 2 < code.size() && code[i] == L'0' && code[i + 1] == L'b')
            {
                size_t end = i + 2;
                while (end < code.size() && (code[end] == L'0' || code[end] == L'1' || code[end] == L'_')) ++end;
                if (end > i + 2) { i = end; prefixed = true; }
            }
            if (!prefixed)
            {
                while (i < code.size() && (digit(code[i]) || code[i] == L'_')) ++i;
                if (i < code.size() && code[i] == L'.')
                {
                    ++i;
                    while (i < code.size() && (digit(code[i]) || code[i] == L'_')) ++i;
                }
                if (i < code.size() && (code[i] == L'e' || code[i] == L'E'))
                {
                    const size_t exponent = i++;
                    if (i < code.size() && (code[i] == L'+' || code[i] == L'-')) ++i;
                    const size_t digits = i;
                    while (i < code.size() && digit(code[i])) ++i;
                    if (digits == i) i = exponent;
                }
            }
            while (i < code.size() && ((code[i] >= L'a' && code[i] <= L'z') || (code[i] >= L'A' && code[i] <= L'Z'))) ++i;
            if (i < code.size() && (digit(code[i]) || code[i] == L'_'))
            {
                i = start;
                return false;
            }
        }
        add(start, i - start, ChatMarkdownStyle::CODE_NUMBER);
        return true;
    };
    static const std::unordered_set<std::wstring> common = {
        L"abstract",L"as",L"async",L"await",L"break",L"case",L"catch",L"class",L"const",L"continue",
        L"default",L"delete",L"do",L"else",L"enum",L"export",L"extends",L"false",L"final",L"finally",
        L"fn",L"for",L"from",L"function",L"if",L"impl",L"import",L"in",L"interface",L"let",L"match",
        L"namespace",L"new",L"nil",L"null",L"nullptr",L"package",L"private",L"protected",L"public",
        L"return",L"self",L"sizeof",L"static",L"struct",L"super",L"switch",L"template",L"this",L"throw",
        L"true",L"try",L"type",L"typedef",L"typename",L"typeof",L"union",L"unsafe",L"use",L"using",
        L"var",L"virtual",L"void",L"volatile",L"while",L"yield",L"auto",L"bool",L"char",L"double",
        L"float",L"int",L"long",L"short",L"signed",L"unsigned",L"string",L"number",L"boolean",L"constexpr",L"debugger",
        L"defer",L"extern",L"func",L"get",L"go",L"goto",L"implements",L"inline",L"instanceof",L"loop",L"mod",
        L"module",L"move",L"mut",L"of",L"operator",L"override",L"pub",L"readonly",L"ref",L"register",L"throws",
        L"trait",L"where",L"with",L"any",L"never",L"unknown",L"Self",L"__global__",L"__device__",L"__host__",
        L"__shared__",L"__restrict__",L"i8",L"i16",L"i32",L"i64",L"u8",L"u16",L"u32",L"u64",L"f32",L"f64",
        L"usize",L"isize"};
    static const std::unordered_set<std::wstring> shellWords = {
        L"if",L"then",L"else",L"elif",L"fi",L"for",L"while",L"until",L"do",L"done",L"case",L"esac",L"in",
        L"function",L"return",L"export",L"local",L"set",L"unset",L"echo",L"printf",L"cd",L"sudo",L"source",
        L"alias",L"exit",L"read",L"shift",L"trap",L"eval",L"exec"};
    static const std::unordered_set<std::wstring> pyWords = {
        L"and",L"as",L"assert",L"async",L"await",L"break",L"class",L"continue",L"def",L"del",L"elif",
        L"else",L"except",L"finally",L"for",L"from",L"global",L"if",L"import",L"in",L"is",L"case",
        L"lambda",L"match",L"nonlocal",L"not",L"or",L"pass",L"raise",L"return",L"try",L"while",L"cls",
        L"with",L"yield",L"self",L"print",L"len",L"range"};
    static const std::unordered_set<std::wstring> pyLiterals = {L"False", L"None", L"True"};
    static const std::unordered_set<std::wstring> sqlWords = {
        L"select",L"from",L"where",L"and",L"or",L"not",L"insert",L"into",L"values",L"update",L"set",
        L"delete",L"create",L"table",L"index",L"view",L"add",L"drop",L"alter",L"join",L"left",L"right",L"inner",
        L"outer",L"on",L"group",L"by",L"order",L"having",L"limit",L"offset",L"as",L"distinct",
        L"union",L"all",L"null",L"is",L"in",L"like",L"between",L"case",L"when",L"then",L"else",L"end",
        L"primary",L"key",L"foreign",L"references",L"default",L"exists",L"with",L"returning",L"asc",L"desc",
        L"count",L"sum",L"avg",L"min",L"max"};
    static const std::unordered_set<std::wstring> yamlWords = {L"true",L"false",L"null",L"yes",L"no",L"on",L"off"};
    bool blockComment = false;
    for (; i < code.size();)
    {
        const size_t begin = i;
        if (blockComment)
        {
            auto end = code.find(L"*/", i);
            i = end == std::wstring::npos ? code.size() : end + 2;
            blockComment = end == std::wstring::npos;
            add(begin, i - begin, ChatMarkdownStyle::CODE_COMMENT);
        }
        else if (html && code.compare(i, 4, L"<!--") == 0)
        {
            auto end = code.find(L"-->", i + 4);
            i = end == std::wstring::npos ? code.size() : end + 3;
            add(begin, i - begin, ChatMarkdownStyle::CODE_COMMENT);
        }
        else if (html && code[i] == L'<')
        {
            size_t end = i + 1;
            if (end < code.size() && code[end] == L'/') ++end;
            if (end < code.size() && asciiLetter(code[end]))
            {
                ++end;
                while (end < code.size() && (asciiWord(code[end]) || code[end] == L':' || code[end] == L'-')) ++end;
                i = end;
                add(begin, i - begin, ChatMarkdownStyle::CODE_KEYWORD);
            }
            else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if (html && code[i] == L'&')
        {
            size_t end = i + 1;
            if (end < code.size() && code[end] == L'#') ++end;
            const size_t wordStart = end;
            while (end < code.size() && asciiWord(code[end])) ++end;
            if (end > wordStart && end < code.size() && code[end] == L';')
            {
                i = end + 1;
                add(begin, i - begin, ChatMarkdownStyle::CODE_NUMBER);
            }
            else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if (html && (code[i] == L'\'' || code[i] == L'"'))
        {
            const auto quote = code[i++];
            while (i < code.size() && code[i] != L'\n')
            {
                if (code[i] == L'\\' && i + 1 < code.size()) { i += 2; continue; }
                if (code[i++] == quote) break;
            }
            add(begin, i - begin, ChatMarkdownStyle::CODE_STRING);
        }
        else if (html && (asciiLetter(code[i]) || code[i] == L'_' || code[i] == L':'))
        {
            size_t end = i + 1;
            while (end < code.size() && (asciiWord(code[end]) || code[end] == L':' || code[end] == L'.' || code[end] == L'-')) ++end;
            i = end;
            add(begin, i - begin, i < code.size() && code[i] == L'=' ? ChatMarkdownStyle::CODE_ATTRIBUTE : ChatMarkdownStyle::CODE);
        }
        else if (html) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        else if (cFamily && code[i] == L'#')
        {
            size_t directiveEnd = i + 1;
            while (directiveEnd < code.size() && (code[directiveEnd] == L' ' || code[directiveEnd] == L'\t')) ++directiveEnd;
            const size_t wordStart = directiveEnd;
            while (directiveEnd < code.size() && code[directiveEnd] >= L'a' && code[directiveEnd] <= L'z') ++directiveEnd;
            if (directiveEnd == wordStart) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
            else { i = directiveEnd; add(begin, i - begin, ChatMarkdownStyle::CODE_KEYWORD); }
        }
        else if (css && code[i] == L'#')
        {
            auto hex = [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') || (c >= L'A' && c <= L'F'); };
            size_t end = i + 1;
            while (end < code.size() && hex(code[end])) ++end;
            if (end - i >= 4 && end - i <= 9 && (end == code.size() || !asciiWord(code[end])))
            {
                i = end;
                add(begin, i - begin, ChatMarkdownStyle::CODE_NUMBER);
            }
            else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if (css && code[i] == L'@')
        {
            size_t end = i + 1;
            while (end < code.size() && (asciiWord(code[end]) || code[end] == L'-')) ++end;
            if (end == i + 1) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
            else { i = end; add(begin, i - begin, ChatMarkdownStyle::CODE_KEYWORD); }
        }
        else if ((cFamily && code[i] == L'/' && i + 1 < code.size() && code[i + 1] == L'/') ||
                 (code[i] == L'#' && (python || yaml || (shell && (i == 0 || ChatInputTrimWhitespace(code[i - 1]))))) ||
                 (code[i] == L'-' && i + 1 < code.size() && code[i + 1] == L'-' && sql))
        {
            auto end = code.find(L'\n', i);
            i = end == std::wstring::npos ? code.size() : end;
            add(begin, i - begin, ChatMarkdownStyle::CODE_COMMENT);
        }
        else if ((cFamily || css || sql) && code[i] == L'/' && i + 1 < code.size() && code[i + 1] == L'*')
        {
            auto end = code.find(L"*/", i + 2);
            i = end == std::wstring::npos ? code.size() : end + 2;
            blockComment = end == std::wstring::npos;
            add(begin, i - begin, ChatMarkdownStyle::CODE_COMMENT);
        }
        else if (python && code[i] == L'@')
        {
            size_t end = i + 1;
            while (end < code.size() && (asciiWord(code[end]) || code[end] == L'.')) ++end;
            if (end == i + 1) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
            else { i = end; add(begin, i - begin, ChatMarkdownStyle::CODE_FUNCTION); }
        }
        else if (json && code[i] == L'"')
        {
            size_t end = i + 1;
            while (end < code.size() && code[end] != L'\n')
            {
                if (code[end] == L'\\' && end + 1 < code.size()) { end += 2; continue; }
                if (code[end++] == L'"') break;
            }
            size_t next = end;
            while (next < code.size() && ChatInputTrimWhitespace(code[next])) ++next;
            const auto style = end <= code.size() && end > i + 1 && code[end - 1] == L'"' &&
                next < code.size() && code[next] == L':' ? ChatMarkdownStyle::CODE_ATTRIBUTE : ChatMarkdownStyle::CODE_STRING;
            consumeString(begin, i, style);
        }
        else if (python && matchesPythonStringPrefix(i))
        {
            size_t quoteAt = i;
            while (quoteAt < code.size() && quoteAt - i < 2 && pyPrefix(code[quoteAt])) ++quoteAt;
            consumeString(begin, quoteAt, ChatMarkdownStyle::CODE_STRING);
        }
        else if (code[i] == L'\'' || code[i] == L'"' || (cFamily && code[i] == L'`'))
            consumeString(begin, i, ChatMarkdownStyle::CODE_STRING);
        else if (shell && code[i] == L'$')
        {
            size_t end = i + 1;
            if (end < code.size() && code[end] == L'{')
            {
                ++end;
                while (end < code.size() && code[end] != L'}' && code[end] != L'\n') ++end;
                if (end < code.size() && code[end] == L'}') ++end;
            }
            else
            {
                while (end < code.size() && (asciiWord(code[end]) || code[end] == L'@' || code[end] == L'#' || code[end] == L'?' ||
                       code[end] == L'*' || code[end] == L'!' || code[end] == L'$' || code[end] == L'-')) ++end;
                if (end == i + 1) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
            }
            if (end > i + 1) { i = end; add(begin, i - begin, ChatMarkdownStyle::CODE_VARIABLE); }
        }
        // Keep YAML keys whole before splitting ordinary words into value tokens.
        else if (yaml && (asciiLetter(code[i]) || code[i] == L'_' || code[i] == L'-' || code[i] == L'.' ||
                          (code[i] >= L'0' && code[i] <= L'9')))
        {
            size_t keyEnd = i;
            while (keyEnd < code.size() && (asciiWord(code[keyEnd]) || code[keyEnd] == L'.' || code[keyEnd] == L'-')) ++keyEnd;
            size_t separator = keyEnd;
            while (separator < code.size() && (code[separator] == L' ' || code[separator] == L'\t')) ++separator;
            if (separator < code.size() && (code[separator] == L':' || code[separator] == L'='))
            {
                i = keyEnd;
                add(begin, i - begin, ChatMarkdownStyle::CODE_ATTRIBUTE);
            }
            else if (code[i] >= L'0' && code[i] <= L'9')
            {
                if (i > 0 && asciiWord(code[i - 1])) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
                else if (!consumeNumber(begin)) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
            }
            else if (code[i] >= L'a' && code[i] <= L'z')
            {
                ++i;
                while (i < code.size() && code[i] >= L'a' && code[i] <= L'z') ++i;
                const auto word = code.substr(begin, i - begin);
                const auto style = yamlWords.find(word) != yamlWords.end() ? ChatMarkdownStyle::CODE_KEYWORD :
                    i < code.size() && code[i] == L'(' ? ChatMarkdownStyle::CODE_FUNCTION : ChatMarkdownStyle::CODE;
                add(begin, i - begin, style);
            }
            else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if ((code[i] >= L'0' && code[i] <= L'9') || ((json || css) && code[i] == L'-' && i + 1 < code.size() &&
                 code[i + 1] >= L'0' && code[i + 1] <= L'9'))
        {
            if (!consumeNumber(begin)) { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if (json && (asciiLetter(code[i]) || code[i] == L'_'))
        {
            if (code[i] >= L'a' && code[i] <= L'z')
            {
                ++i;
                while (i < code.size() && code[i] >= L'a' && code[i] <= L'z') ++i;
                const auto word = code.substr(begin, i - begin);
                const bool keyword = word == L"true" || word == L"false" || word == L"null";
                add(begin, i - begin, keyword ? ChatMarkdownStyle::CODE_KEYWORD : ChatMarkdownStyle::CODE);
            }
            else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
        }
        else if ((code[i] >= L'A' && code[i] <= L'Z') || (code[i] >= L'a' && code[i] <= L'z') || code[i] == L'_' ||
                 ((cFamily || python || sql) && code[i] == L'$') ||
                 (css && code[i] == L'-'))
        {
            ++i;
            while (i < code.size() && ((code[i] >= L'A' && code[i] <= L'Z') || (code[i] >= L'a' && code[i] <= L'z') ||
                   (code[i] >= L'0' && code[i] <= L'9') || code[i] == L'_' || ((cFamily || python || sql) && code[i] == L'$') ||
                   (shell && (code[i] == L'.' || code[i] == L'-')) ||
                   (css && code[i] == L'-'))) ++i;
            auto word = code.substr(begin, i - begin), lookup = word;
            if (sql) for (auto &c : lookup) c = (wchar_t)towlower(c);
            bool attribute = false;
            if (css)
            {
                size_t next = i;
                while (next < code.size() && iswspace(code[next])) ++next;
                if (next < code.size() && code[next] == L':')
                {
                    for (++next; next < code.size() && code[next] != L':' && code[next] != L'{' && code[next] != L';'; ++next)
                        if (code[next] == L'}') { attribute = true; break; }
                    if (next < code.size() && code[next] == L';') attribute = true;
                }
            }
            const bool keyword = json ? (lookup == L"true" || lookup == L"false" || lookup == L"null") :
                python ? (pyWords.find(word) != pyWords.end() || pyLiterals.find(word) != pyLiterals.end()) :
                sql ? sqlWords.find(lookup) != sqlWords.end() :
                shell ? shellWords.find(word) != shellWords.end() :
                !css && common.find(lookup) != common.end();
            if (attribute) add(begin, i - begin, ChatMarkdownStyle::CODE_ATTRIBUTE);
            else if (keyword) add(begin, i - begin, ChatMarkdownStyle::CODE_KEYWORD);
            else
            {
                const bool function = !css && !json && i < code.size() && code[i] == L'(';
                add(begin, i - begin, function ? ChatMarkdownStyle::CODE_FUNCTION : ChatMarkdownStyle::CODE);
            }
        }
        else { ++i; add(begin, 1, ChatMarkdownStyle::CODE); }
    }
    return out;
}

inline bool ChatMarkdownTableSeparator(const std::wstring &line)
{
    size_t first = line.find_first_not_of(L" \t\r\n"), last = line.find_last_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return false;
    size_t i = first;
    if (line[i] == L'|') ++i;
    const size_t cellStart = i;
    while (i <= last && (ChatInputTrimWhitespace(line[i]) || line[i] == L':' || line[i] == L'-')) ++i;
    if (i == cellStart || i > last || line[i] != L'|') return false;
    for (++i; i <= last; ++i)
        if (line[i] != L'|' && line[i] != L':' && line[i] != L'-' && !ChatInputTrimWhitespace(line[i])) return false;
    return true;
}
inline bool ChatMarkdownHorizontalRule(const std::wstring &line)
{
    size_t first = 0;
    while (first < line.size() && ChatInputTrimWhitespace(line[first])) ++first;
    if (first == line.size()) return false;
    const wchar_t marker = line[first];
    if (marker != L'-' && marker != L'*' && marker != L'_') return false;
    size_t count = 0;
    for (size_t i = first; i < line.size(); ++i)
    {
        if (line[i] == marker) ++count;
        else if (!ChatInputTrimWhitespace(line[i])) return false;
    }
    return count >= 3;
}
inline bool ChatMarkdownFenceOpener(const std::wstring &line, bool newline)
{
    return newline && line.rfind(L"```", 0) == 0 && line.find(L'`', 3) == std::wstring::npos;
}
inline bool ChatMarkdownFenceCloser(const std::wstring &line)
{
    return line.rfind(L"```", 0) == 0 && line.find_first_not_of(L" \t", 3) == std::wstring::npos;
}
inline bool ChatMarkdownHasUnclosedFence(const std::wstring &markdown)
{
    bool fenced = false;
    for (size_t start = 0; start < markdown.size();)
    {
        const auto end = markdown.find(L'\n', start);
        std::wstring line = markdown.substr(start, end == std::wstring::npos ? end : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (fenced ? ChatMarkdownFenceCloser(line) : ChatMarkdownFenceOpener(line, end != std::wstring::npos))
            fenced = !fenced;
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return fenced;
}
inline std::wstring ChatMarkdownNormalizeLists(const std::wstring &markdown)
{
    struct ListLevel { size_t indent; bool ordered; long long nextNumber; };
    std::wstring joined;
    std::vector<ListLevel> levels;
    bool fenced = false, previousListItem = false;
    for (size_t start = 0; start < markdown.size();)
    {
        const auto end = markdown.find(L'\n', start);
        std::wstring line = markdown.substr(start, end == std::wstring::npos ? end : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t first = 0;
        while (first < line.size() && ChatInputTrimWhitespace(line[first])) ++first;
        const auto trimmed = TrimChatInput(line);
        const bool fence = fenced ? ChatMarkdownFenceCloser(line) : ChatMarkdownFenceOpener(line, end != std::wstring::npos);
        bool listItem = false, orderedItem = false;
        size_t markerEnd = first;
        size_t numberEnd = first;
        long long itemStart = 1;
        if (!fenced && !fence && first < line.size())
        {
            if (line[first] == L'-' || line[first] == L'+' || line[first] == L'*')
            {
                markerEnd = first + 1;
            }
            else
            {
                while (numberEnd < line.size() && line[numberEnd] >= L'0' && line[numberEnd] <= L'9') ++numberEnd;
                orderedItem = numberEnd > first && numberEnd + 1 < line.size() &&
                    (line[numberEnd] == L'.' || line[numberEnd] == L')');
                listItem = orderedItem;
                if (orderedItem)
                {
                    itemStart = 0;
                    for (size_t i = first; i < numberEnd; ++i)
                    {
                        const auto digit = line[i] - L'0';
                        if (itemStart > (std::numeric_limits<long long>::max() - digit) / 10)
                        {
                            itemStart = std::numeric_limits<long long>::max();
                            break;
                        }
                        itemStart = itemStart * 10 + digit;
                    }
                    if (itemStart == 0) itemStart = 1;
                    markerEnd = numberEnd + 1;
                }
            }
        }
        size_t contentStart = markerEnd;
        if (markerEnd > first)
            while (contentStart < line.size() && ChatInputTrimWhitespace(line[contentStart])) ++contentStart;
        listItem = markerEnd > first && contentStart > markerEnd;
        const bool continuation = !fenced && !fence && previousListItem && !trimmed.empty() && first >= 2 && !listItem;
        if (listItem)
        {
            size_t indent = 0;
            for (size_t i = 0; i < first; ++i) indent += line[i] == L'\t' ? 4 : 1;
            while (!levels.empty() && levels.back().indent > indent) levels.pop_back();
            const bool newLevel = levels.empty() || levels.back().indent < indent;
            if (newLevel) levels.push_back({indent, orderedItem, itemStart});
            auto &level = levels.back();
            const auto number = level.nextNumber;
            if (level.ordered && level.nextNumber < std::numeric_limits<long long>::max()) ++level.nextNumber;
            const std::wstring marker = level.ordered ? std::to_wstring(number) + L". " : L"- ";
            line = std::wstring((levels.size() - 1) * 2, L' ') + marker + line.substr(contentStart);
            joined += line;
            previousListItem = true;
        }
        else if (continuation)
        {
            if (!joined.empty() && joined.back() == L'\n') joined.pop_back();
            joined.push_back(L' ');
            joined += trimmed;
        }
        else
        {
            joined += line;
            previousListItem = false;
            if (!fenced && !fence) levels.clear();
        }
        if (end != std::wstring::npos) joined.push_back(L'\n');
        if (fence)
        {
            fenced = !fenced;
            previousListItem = false;
            levels.clear();
        }
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return joined;
}
inline std::pair<int, int> ChatMarkdownListIndents(int depth, size_t markerDigits, int dpi)
{
    const auto scale = [dpi](long long twips) { return static_cast<int>((twips * dpi + 48) / 96); };
    const long long markerWidth = markerDigits ? static_cast<long long>(markerDigits) * 120 + 120 : 180;
    return {scale((static_cast<long long>(depth) + 1) * 360), -scale(markerWidth)};
}
inline bool ChatMarkdownWordChar(wchar_t c)
{
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
           (c >= L'0' && c <= L'9') || c == L'_';
}
inline std::vector<ChatMarkdownSpan> ChatMarkdownInlineLabelSpans(const std::wstring &line)
{
    std::vector<ChatMarkdownSpan> out;
    auto add = [&](std::wstring text, ChatMarkdownStyle style) {
        if (text.empty()) return;
        if (!out.empty() && out.back().style == style) out.back().text += text;
        else out.push_back({std::move(text), L"", style});
    };
    for (size_t i = 0; i < line.size();)
    {
        if (line[i] == L'`' || (line[i] == L'*' && i + 1 < line.size() && line[i + 1] == L'*'))
        {
            const bool code = line[i] == L'`';
            const size_t marker = code ? 1 : 2;
            const auto close = code ? line.find(L'`', i + marker) : line.find(L"**", i + marker);
            if (close != std::wstring::npos && close > i + marker)
            {
                add(line.substr(i + marker, close - i - marker), code ? ChatMarkdownStyle::CODE : ChatMarkdownStyle::EMPHASIS);
                i = close + marker;
                continue;
            }
        }
        if (line[i] == L'*' || (line[i] == L'~' && i + 1 < line.size() && line[i + 1] == L'~'))
        {
            const wchar_t marker = line[i];
            const size_t width = marker == L'~' ? 2 : 1;
            const auto close = line.find(std::wstring(width, marker), i + width);
            bool italicBoundaries = marker != L'*' ||
                (i == 0 || (!ChatMarkdownWordChar(line[i - 1]) && line[i - 1] != L'*'));
            if (marker == L'*' && close != std::wstring::npos && close + width < line.size())
                italicBoundaries = italicBoundaries && !ChatMarkdownWordChar(line[close + width]) &&
                    line[close + width] != L'*';
            if (close != std::wstring::npos && close > i + width && italicBoundaries)
            {
                add(line.substr(i + width, close - i - width), marker == L'~' ?
                    ChatMarkdownStyle::STRIKETHROUGH : ChatMarkdownStyle::ITALIC);
                i = close + width;
                continue;
            }
        }
        const auto next = line.find_first_of(L"`*~", i + 1);
        const auto count = next == std::wstring::npos ? line.size() - i : next - i;
        add(line.substr(i, count), ChatMarkdownStyle::TEXT);
        i += count;
    }
    return out;
}
inline std::vector<ChatMarkdownSpan> ChatMarkdownInlineSpans(const std::wstring &line)
{
    std::vector<ChatMarkdownSpan> out;
    auto add = [&](std::wstring text, ChatMarkdownStyle style, std::wstring target = L"") {
        if (text.empty()) return;
        if (!out.empty() && out.back().style == style && out.back().target == target) out.back().text += text;
        else out.push_back({std::move(text), std::move(target), style});
    };
    for (size_t i = 0; i < line.size();)
    {
        if (line[i] == L'[')
        {
            const auto labelEnd = line.find(L"](", i + 1);
            const auto urlEnd = labelEnd == std::wstring::npos ? labelEnd : line.find(L')', labelEnd + 2);
            if (labelEnd != std::wstring::npos && labelEnd > i + 1 && urlEnd != std::wstring::npos)
            {
                const auto url = line.substr(labelEnd + 2, urlEnd - labelEnd - 2);
                const bool https = url.rfind(L"https://", 0) == 0 && url.size() > 8;
                const bool http = url.rfind(L"http://", 0) == 0 && url.size() > 7;
                if ((https || http) && std::none_of(url.begin(), url.end(), ChatInputTrimWhitespace))
                {
                    for (auto part : ChatMarkdownInlineLabelSpans(line.substr(i + 1, labelEnd - i - 1)))
                    {
                        if (part.style == ChatMarkdownStyle::TEXT) part.style = ChatMarkdownStyle::LINK;
                        add(std::move(part.text), part.style, url);
                    }
                    i = urlEnd + 1;
                    continue;
                }
            }
        }
        if (line[i] == L'`' || (line[i] == L'*' && i + 1 < line.size() && line[i + 1] == L'*'))
        {
            const bool code = line[i] == L'`';
            const size_t marker = code ? 1 : 2;
            const auto close = code ? line.find(L'`', i + marker) : line.find(L"**", i + marker);
            if (close != std::wstring::npos && close > i + marker)
            {
                add(line.substr(i + marker, close - i - marker), code ? ChatMarkdownStyle::CODE : ChatMarkdownStyle::EMPHASIS);
                i = close + marker;
                continue;
            }
        }
        if (line[i] == L'*' || (line[i] == L'~' && i + 1 < line.size() && line[i + 1] == L'~'))
        {
            const wchar_t marker = line[i];
            const size_t width = marker == L'~' ? 2 : 1;
            const auto close = line.find(std::wstring(width, marker), i + width);
            bool italicBoundaries = marker != L'*' ||
                (i == 0 || (!ChatMarkdownWordChar(line[i - 1]) && line[i - 1] != L'*'));
            if (marker == L'*' && close != std::wstring::npos && close + width < line.size())
                italicBoundaries = italicBoundaries && !ChatMarkdownWordChar(line[close + width]) &&
                    line[close + width] != L'*';
            if (close != std::wstring::npos && close > i + width && italicBoundaries)
            {
                add(line.substr(i + width, close - i - width), marker == L'~' ?
                    ChatMarkdownStyle::STRIKETHROUGH : ChatMarkdownStyle::ITALIC);
                i = close + width;
                continue;
            }
        }
        const auto next = line.find_first_of(L"`*~[", i + 1);
        const auto count = next == std::wstring::npos ? line.size() - i : next - i;
        add(line.substr(i, count), ChatMarkdownStyle::TEXT);
        i += count;
    }
    return out;
}
inline std::vector<std::wstring> ChatMarkdownTableCells(std::wstring line)
{
    line = TrimChatInput(line);
    if (line.empty()) return {};
    if (!line.empty() && line.front() == L'|') line.erase(line.begin());
    if (!line.empty() && line.back() == L'|') line.pop_back();
    std::vector<std::wstring> cells;
    size_t start = 0;
    for (;;)
    {
        auto end = line.find(L'|', start);
        std::wstring cell = line.substr(start, end == std::wstring::npos ? end : end - start);
        cells.push_back(TrimChatInput(cell));
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return cells;
}

inline std::vector<ChatMarkdownSpan> ChatMarkdownSpans(const std::wstring &markdown, int initialSpaceBeforeTwips = 0)
{
    const auto normalized = ChatMarkdownNormalizeLists(markdown);
    std::vector<ChatMarkdownSpan> out;
    int currentListDepth = -1;
    int pendingSpaceBeforeTwips = initialSpaceBeforeTwips;
    bool hasContent = false, paragraphOpen = false;
    std::vector<bool> listItemsAtDepth;
    auto add = [&](std::wstring text, ChatMarkdownStyle style, std::wstring target = L"", bool fencedBlock = false,
                   bool tableCell = false, int listDepth = -2, ChatMarkdownStyle headingStyle = ChatMarkdownStyle::TEXT,
                   int paragraphSpaceBeforeTwips = 0, bool blockQuote = false) {
        if (text.empty()) return;
        paragraphSpaceBeforeTwips = std::max(paragraphSpaceBeforeTwips, pendingSpaceBeforeTwips);
        pendingSpaceBeforeTwips = 0;
        if (listDepth == -2) listDepth = currentListDepth;
        if (paragraphSpaceBeforeTwips == 0 && !out.empty() && out.back().paragraphSpaceBeforeTwips == 0 &&
            out.back().style == style && out.back().target == target && out.back().fenced == fencedBlock &&
            out.back().tableCell == tableCell && out.back().listDepth == listDepth && out.back().headingStyle == headingStyle && out.back().blockQuote == blockQuote)
            out.back().text += text;
        else out.push_back({std::move(text), std::move(target), style, fencedBlock, tableCell, listDepth, headingStyle, paragraphSpaceBeforeTwips, blockQuote});
        hasContent = true;
    };
    bool fenced = false;
    bool codeHasContent = false;
    std::wstring codeLang;
    size_t start = 0;
    while (start < normalized.size())
    {
        currentListDepth = -1;
        const auto end = normalized.find(L'\n', start);
        std::wstring line = normalized.substr(start, end == std::wstring::npos ? end : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t first = line.find_first_not_of(L" \t");
        const std::wstring trimmed = first == std::wstring::npos ? L"" : line.substr(first);
        if (!fenced && std::all_of(line.begin(), line.end(), ChatInputTrimWhitespace))
        {
            if (hasContent) pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 180);
            paragraphOpen = false;
            listItemsAtDepth.clear();
            if (end == std::wstring::npos) break;
            start = end + 1;
            continue;
        }
        size_t tableFirst = 0, tableLast = line.size();
        while (tableFirst < tableLast && ChatInputTrimWhitespace(line[tableFirst])) ++tableFirst;
        while (tableLast > tableFirst && ChatInputTrimWhitespace(line[tableLast - 1])) --tableLast;
        if (!fenced && end != std::wstring::npos && tableFirst < tableLast &&
            line[tableFirst] == L'|' && line[tableLast - 1] == L'|')
        {
            const auto separatorEnd = normalized.find(L'\n', end + 1);
            const auto separator = normalized.substr(end + 1, separatorEnd == std::wstring::npos
                ? separatorEnd : separatorEnd - end - 1);
            if (ChatMarkdownTableSeparator(separator))
            {
                if (hasContent) pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 180);
                paragraphOpen = false;
                listItemsAtDepth.clear();
                std::vector<std::vector<std::vector<ChatMarkdownSpan>>> rows;
                auto parseRow = [&](const std::wstring &row) {
                    std::vector<std::vector<ChatMarkdownSpan>> cells;
                    for (const auto &cell : ChatMarkdownTableCells(row))
                    {
                        auto parts = ChatMarkdownSpans(cell);
                        for (auto &part : parts)
                            while (!part.text.empty() && (part.text.back() == L'\r' || part.text.back() == L'\n'))
                                part.text.pop_back();
                        cells.push_back(std::move(parts));
                    }
                    rows.push_back(std::move(cells));
                };
                parseRow(line);
                size_t cursor = separatorEnd == std::wstring::npos ? normalized.size() : separatorEnd + 1;
                while (cursor < normalized.size())
                {
                    const auto rowEnd = normalized.find(L'\n', cursor);
                    const auto row = normalized.substr(cursor, rowEnd == std::wstring::npos ? rowEnd : rowEnd - cursor);
                    size_t first = 0, last = row.size();
                    while (first < last && ChatInputTrimWhitespace(row[first])) ++first;
                    while (last > first && ChatInputTrimWhitespace(row[last - 1])) --last;
                    if (first == last || row[first] != L'|' || row[last - 1] != L'|') break;
                    parseRow(row);
                    if (rowEnd == std::wstring::npos) { cursor = normalized.size(); break; }
                    cursor = rowEnd + 1;
                }
                size_t columnCount = 0;
                for (const auto &row : rows) columnCount = std::max(columnCount, row.size());
                std::vector<size_t> widths(columnCount);
                auto visibleWidth = [](const std::vector<ChatMarkdownSpan> &cell) {
                    size_t width = 0;
                    for (const auto &part : cell) width += part.text.size();
                    return width;
                };
                for (const auto &row : rows)
                    for (size_t i = 0; i < row.size(); ++i)
                        widths[i] = std::max(widths[i], visibleWidth(row[i]));
                for (size_t row = 0; row < rows.size(); ++row)
                {
                    const auto rowStyle = row == 0 ? ChatMarkdownStyle::TABLE_HEADER : ChatMarkdownStyle::TABLE_ROW;
                    for (size_t i = 0; i < rows[row].size(); ++i)
                    {
                        const auto cellWidth = visibleWidth(rows[row][i]);
                        for (auto part : rows[row][i])
                        {
                            if (part.style == ChatMarkdownStyle::TEXT) part.style = rowStyle;
                            add(std::move(part.text), part.style, std::move(part.target), false, true);
                        }
                        if (i + 1 < rows[row].size())
                            add(std::wstring(std::max<size_t>(2, widths[i] - cellWidth + 2), L' '),
                                ChatMarkdownStyle::TEXT, L"", false, true);
                    }
                    add(L"\r\n", ChatMarkdownStyle::TEXT, L"", false, true);
                }
                start = cursor;
                continue;
            }
        }
        const bool fence = fenced ? ChatMarkdownFenceCloser(line) : ChatMarkdownFenceOpener(line, end != std::wstring::npos);
        bool listItem = false;
        if (fence && !fenced)
        {
            pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 240);
            paragraphOpen = false;
            listItemsAtDepth.clear();
            codeHasContent = false;
        }
        if (fence)
        {
            if (fenced)
            {
                fenced = false;
                codeLang.clear();
                paragraphOpen = false;
                if (!codeHasContent) pendingSpaceBeforeTwips = 0;
            }
            else
            {
                fenced = true;
                codeLang = TrimChatInput(trimmed.substr(3));
                const auto tokenEnd = std::find_if(codeLang.begin(), codeLang.end(), ChatInputTrimWhitespace);
                codeLang.resize((size_t)(tokenEnd - codeLang.begin()));
            }
        }
        else if (fenced)
        {
            for (auto &span : ChatSyntaxSpans(line + L"\r\n", codeLang))
                add(std::move(span.text), span.style, codeLang, true);
            codeHasContent = true;
        }
        else
        {
            size_t prefix = 0;
            while (prefix < line.size() && prefix < 6 && line[prefix] == L'#') ++prefix;
            if (prefix && prefix < line.size() && ChatInputTrimWhitespace(line[prefix]))
            {
                paragraphOpen = false;
                size_t title = prefix;
                while (title < line.size() && ChatInputTrimWhitespace(line[title])) ++title;
                const auto headingStyle = prefix <= 2 ? ChatMarkdownStyle::HEADING : ChatMarkdownStyle::SUBHEADING;
                for (auto part : ChatMarkdownInlineSpans(line.substr(title)))
                    add(std::move(part.text), part.style, std::move(part.target), false, false, -2, headingStyle);
                add(L"\r\n", ChatMarkdownStyle::TEXT, L"", false, false, -2, headingStyle);
            }
            else if (ChatMarkdownHorizontalRule(trimmed))
            {
                paragraphOpen = false;
                pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 300);
                add(L"────────────────────────────────────────\r\n", ChatMarkdownStyle::HORIZONTAL_RULE);
                pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 300);
            }
            else if (!line.empty() && line[0] == L'>')
            {
                paragraphOpen = false;
                if (hasContent) pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 180);
                size_t contentStart = 1;
                if (contentStart < line.size() && ChatInputTrimWhitespace(line[contentStart])) ++contentStart;
                for (auto part : ChatMarkdownInlineSpans(line.substr(contentStart)))
                    add(std::move(part.text), part.style, std::move(part.target), false, false, -2, ChatMarkdownStyle::TEXT, 0, true);
                add(L"\r\n", ChatMarkdownStyle::QUOTE, L"", false, false, -2, ChatMarkdownStyle::TEXT, 0, true);
            }
            else
            {
                size_t contentStart = 0;
                const size_t indent = line.find_first_not_of(L" \t");
                if (indent != std::wstring::npos && indent < line.size())
                {
                    size_t markerEnd = indent;
                    if ((line[indent] == L'-' || line[indent] == L'+' || line[indent] == L'*') &&
                        indent + 1 < line.size() && (line[indent + 1] == L' ' || line[indent + 1] == L'\t'))
                        markerEnd = indent + 1;
                    else
                    {
                        while (markerEnd < line.size() && line[markerEnd] >= L'0' && line[markerEnd] <= L'9') ++markerEnd;
                        if (markerEnd == indent || markerEnd >= line.size() ||
                            (line[markerEnd] != L'.' && line[markerEnd] != L')') || markerEnd + 1 >= line.size() ||
                            (line[markerEnd + 1] != L' ' && line[markerEnd + 1] != L'\t'))
                            markerEnd = indent;
                        else
                            ++markerEnd;
                    }
                    if (markerEnd != indent)
                    {
                        currentListDepth = (int)(indent / 2);
                        listItem = true;
                        paragraphOpen = false;
                        const auto depth = static_cast<size_t>(currentListDepth);
                        if (listItemsAtDepth.empty() && hasContent)
                            pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 180);
                        const bool gapBefore = currentListDepth > 0 ||
                            (depth < listItemsAtDepth.size() && listItemsAtDepth[depth]);
                        if (listItemsAtDepth.size() > depth + 1) listItemsAtDepth.resize(depth + 1);
                        else if (listItemsAtDepth.size() <= depth) listItemsAtDepth.resize(depth + 1, false);
                        listItemsAtDepth[depth] = true;
                        const bool ordered = line[indent] >= L'0' && line[indent] <= L'9';
                        const auto marker = ordered ? line.substr(indent, markerEnd - indent) + L" " : L"• ";
                        add(marker, ChatMarkdownStyle::LIST, L"", false, false, -2, ChatMarkdownStyle::TEXT, gapBefore ? 90 : 0);
                        contentStart = markerEnd + 1;
                        line = line.substr(contentStart);
                    }
                }
                if (!listItem && !paragraphOpen && hasContent)
                    pendingSpaceBeforeTwips = std::max(pendingSpaceBeforeTwips, 180);
                if (!listItem) paragraphOpen = true;
                for (auto part : ChatMarkdownInlineSpans(line))
                    add(std::move(part.text), part.style, std::move(part.target));
                add(L"\r\n", ChatMarkdownStyle::TEXT);
            }
        }
        if (!listItem) listItemsAtDepth.clear();
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    return out;
}

inline size_t ChatMarkdownStreamCut(const std::wstring &text, size_t committed)
{
    if (text.size() < 6000 || text.size() < 3) return committed;
    auto cut = text.rfind(L"\n\n", text.size() - 3);
    while (cut != std::wstring::npos && cut > committed)
    {
        bool inFence = false;
        for (size_t line = 0; line < cut;)
        {
            const auto end = text.find(L'\n', line);
            const auto length = (end == std::wstring::npos || end > cut ? cut : end) - line;
            if (length >= 3 && text.compare(line, 3, L"```") == 0) inFence = !inFence;
            if (end == std::wstring::npos || end >= cut) break;
            line = end + 1;
        }
        if (!inFence) return cut;
        if (!cut) break;
        cut = text.rfind(L"\n\n", cut - 1);
    }
    return committed;
}

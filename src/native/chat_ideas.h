#pragma once
#include <array>
#include <string>

struct NativeChatIdea { const wchar_t *label; const wchar_t *prompt; };

inline const std::array<NativeChatIdea, 4> &NativeChatIdeas(bool turbo)
{
    static const std::array<NativeChatIdea, 4> normal{{
        {L"A three.js scene in one HTML file", L"Write a single-file HTML page with a three.js scene: a slowly rotating, softly lit crystal on a dark background, with orbit controls."},
        {L"Explain and improve some code", L"Explain step by step what this code does, then suggest two improvements:\n\n```\n\n```"},
        {L"Plan and write a small tool", L"Plan a small Python command-line tool that renames photos by the date they were taken. List the steps, then write it."},
        {L"Explain a topic clearly", L"Summarize the key ideas of mixture-of-experts language models for a curious engineer, in five short paragraphs."}}};
    static const std::array<NativeChatIdea, 4> fast{{
        {L"Write a Python function", L"Write a Python function that merges overlapping intervals in a list of (start, end) pairs, with a few tests."},
        normal[0], normal[1],
        {L"Fix a bug", L"This function should return the median of a list but is wrong for even lengths. Find the bug and fix it:\n\n```python\n\n```"}}};
    return turbo ? fast : normal;
}

inline std::wstring NativeGreetingModelCard(const std::wstring &model, unsigned long long context,
                                             bool images, bool ready, const wchar_t *connectionStatus = nullptr)
{
    std::wstring text = model.empty() ? L"No model loaded" : model;
    if (connectionStatus && *connectionStatus) text += L"  ·  " + std::wstring(connectionStatus);
    else text += ready ? L"  ·  Ready" : L"  ·  Unavailable";
    if (context) text += L"  ·  " + std::to_wstring((context + 500) / 1000) + L"K context";
    text += images ? L"  ·  Images on" : L"  ·  Images off";
    text += L"  ·  Switch model";
    return text;
}

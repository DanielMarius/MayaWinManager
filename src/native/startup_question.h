#pragma once
#include <cstddef>
#include <string>
#include <vector>

inline std::wstring NativeStartupQuestion(const std::vector<std::wstring> &args)
{
    for (std::size_t i = 0; i < args.size(); ++i)
    {
        if (args[i] == L"--ask")
            return i + 1 < args.size() ? args[i + 1] : L"";
        if (args[i].rfind(L"--ask=", 0) == 0)
            return args[i].substr(6);
    }
    return L"";
}

inline bool NativeStartupQuestionCanSend(const std::wstring &question, bool mayaReady, bool chatIdle,
                                         bool composerEmpty, bool attachmentsEmpty)
{
    return !question.empty() && mayaReady && chatIdle && composerEmpty && attachmentsEmpty;
}

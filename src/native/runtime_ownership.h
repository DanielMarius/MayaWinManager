#pragma once
#include "../config.h"
#include <algorithm>
#include <array>
#include <cwctype>
#include <shellapi.h>

struct Process {
    DWORD id{}, parent{};
    unsigned long long created{};
    std::wstring exe, command;
    bool server{};
};

inline std::array<std::filesystem::path, 2> MayaRuntimeProfiles(const std::filesystem::path &root)
{
    return {root / L"maya-ds4.json", root / L"maya-ds4-abliterated.json"};
}

inline bool MayaProcessRunsScript(const std::wstring &command, const std::filesystem::path &script,
                                 const std::filesystem::path *config = nullptr)
{
    int count = 0;
    auto args = CommandLineToArgvW(command.c_str(), &count);
    if (!args) return false;
    int i = 1;
    while (i < count && (wcscmp(args[i], L"-u") == 0 || wcscmp(args[i], L"-I") == 0)) ++i;
    bool match = i < count && SamePath(args[i], script.wstring());
    if (match && config)
    {
        int matches = 0, options = 0;
        for (++i; i < count; ++i) if (wcscmp(args[i], L"--config") == 0)
        {
            ++options;
            if (i + 1 < count && SamePath(args[i + 1], config->wstring())) ++matches;
        }
        match = matches == 1 && options == 1;
    }
    LocalFree(args);
    return match;
}

inline bool MayaRuntimeIsServer(const std::filesystem::path &root, const std::filesystem::path &python,
                                const Process &process)
{
    const auto venvPython = root / L".venv" / L"Scripts" / L"python.exe";
    const auto profiles = MayaRuntimeProfiles(root);
    return (SamePath(process.exe, venvPython.wstring()) || SamePath(process.exe, python.wstring())) &&
           std::any_of(profiles.begin(), profiles.end(), [&](const auto &config) {
               return MayaProcessRunsScript(process.command, root / L"serve" / L"server.py", &config);
           });
}

inline bool MayaRuntimeIsChild(const std::filesystem::path &root, const std::filesystem::path &python,
                               const Process &process)
{
    const auto venvPython = root / L".venv" / L"Scripts" / L"python.exe";
    return SamePath(process.exe, (root / L"build" / L"maya-ds4.exe").wstring()) ||
           ((SamePath(process.exe, python.wstring()) || SamePath(process.exe, venvPython.wstring())) &&
            MayaProcessRunsScript(process.command, root / L"serve" / L"telemetry.py"));
}

inline bool MayaRuntimeIsSupervisor(const std::filesystem::path &root, const std::filesystem::path &python,
                                    const Process &process)
{
    const auto venvPython = root / L".venv" / L"Scripts" / L"python.exe";
    const auto profiles = MayaRuntimeProfiles(root);
    return (SamePath(process.exe, python.wstring()) || SamePath(process.exe, venvPython.wstring())) &&
           std::any_of(profiles.begin(), profiles.end(), [&](const auto &config) {
               return MayaProcessRunsScript(process.command, root / L"maya.py", &config);
           });
}

inline bool MayaRuntimeProcessIdentityMatches(const Process &expected, bool observationValid,
                                              unsigned long long actualCreated, const wchar_t *actualExe)
{
    if (!observationValid) return false;
    const auto difference = actualCreated > expected.created ? actualCreated - expected.created
                                                              : expected.created - actualCreated;
    return difference < 10 && SamePath(actualExe, expected.exe);
}

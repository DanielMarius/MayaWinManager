#include "runtime_ownership.h"
#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

std::wstring Quote(const fs::path &path)
{
    return L"\"" + path.wstring() + L"\"";
}

std::wstring ScriptCommand(const fs::path &exe, const fs::path &script, const fs::path *config = nullptr)
{
    auto command = Quote(exe) + L" -u " + Quote(script);
    if (config) command += L" --config " + Quote(*config);
    return command;
}

Process FakeProcess(const fs::path &exe, std::wstring command)
{
    return {1, 0, 1, exe.wstring(), std::move(command), false};
}

int main()
{
    const fs::path root = L"C:\\Fixture\\Maya";
    const fs::path venvPython = root / L".venv" / L"Scripts" / L"python.exe";
    const fs::path basePython = L"C:\\Python311\\python.exe";
    const fs::path server = root / L"serve" / L"server.py";
    const fs::path supervisor = root / L"maya.py";
    const fs::path telemetry = root / L"serve" / L"telemetry.py";
    const fs::path standardProfile = root / L"maya-ds4.json";
    const fs::path abliteratedProfile = root / L"maya-ds4-abliterated.json";
    const fs::path otherProfile = root / L"other.json";

    assert(MayaRuntimeIsServer(root, basePython, FakeProcess(venvPython, ScriptCommand(venvPython, server, &standardProfile))));
    assert(MayaRuntimeIsServer(root, basePython, FakeProcess(basePython, ScriptCommand(basePython, server, &abliteratedProfile))));
    assert(!MayaRuntimeIsServer(root, basePython, FakeProcess(root / L"elsewhere" / L"python.exe",
                                                               ScriptCommand(venvPython, server, &standardProfile))));
    assert(!MayaRuntimeIsServer(root, basePython, FakeProcess(venvPython,
        ScriptCommand(venvPython, root / L"elsewhere" / L"server.py", &standardProfile))));
    auto duplicateConfig = ScriptCommand(venvPython, server, &standardProfile) + L" --config " + Quote(otherProfile);
    assert(!MayaRuntimeIsServer(root, basePython, FakeProcess(venvPython, duplicateConfig)));
    assert(!MayaRuntimeIsServer(root, basePython, FakeProcess(venvPython, ScriptCommand(venvPython, server, &otherProfile))));

    assert(MayaRuntimeIsSupervisor(root, basePython, FakeProcess(basePython, ScriptCommand(basePython, supervisor, &standardProfile))));
    assert(MayaRuntimeIsSupervisor(root, basePython,
        FakeProcess(venvPython, ScriptCommand(venvPython, supervisor, &abliteratedProfile))));
    assert(!MayaRuntimeIsSupervisor(root, basePython,
        FakeProcess(basePython, ScriptCommand(basePython, root / L"elsewhere" / L"maya.py", &standardProfile))));
    assert(!MayaRuntimeIsSupervisor(root, basePython,
        FakeProcess(basePython, ScriptCommand(basePython, supervisor, &otherProfile))));

    assert(MayaRuntimeIsChild(root, basePython, FakeProcess(root / L"build" / L"maya-ds4.exe", L"engine child")));
    assert(MayaRuntimeIsChild(root, basePython, FakeProcess(venvPython, ScriptCommand(venvPython, telemetry))));
    assert(MayaRuntimeIsChild(root, basePython, FakeProcess(basePython, ScriptCommand(basePython, telemetry))));
    assert(!MayaRuntimeIsChild(root, basePython,
        FakeProcess(root / L"elsewhere" / L"maya-ds4.exe", L"unrelated engine")));
    assert(!MayaRuntimeIsChild(root, basePython,
        FakeProcess(root / L"elsewhere" / L"python.exe", ScriptCommand(venvPython, telemetry))));

    const Process expected{7, 0, 100, L"C:\\Maya\\python.exe", L"", true};
    assert(MayaRuntimeProcessIdentityMatches(expected, true, 100, L"c:\\maya\\PYTHON.exe"));
    assert(MayaRuntimeProcessIdentityMatches(expected, true, 109, L"C:\\Maya\\python.exe"));
    assert(MayaRuntimeProcessIdentityMatches(expected, true, 91, L"C:\\Maya\\python.exe"));
    assert(!MayaRuntimeProcessIdentityMatches(expected, true, 110, L"C:\\Maya\\python.exe"));
    assert(!MayaRuntimeProcessIdentityMatches(expected, true, 90, L"C:\\Maya\\python.exe"));
    assert(!MayaRuntimeProcessIdentityMatches(expected, true, 100, L"C:\\Other\\python.exe"));
    assert(!MayaRuntimeProcessIdentityMatches(expected, false, 100, nullptr));

    std::cout << "Runtime ownership and identity guard fixtures passed\n";
}

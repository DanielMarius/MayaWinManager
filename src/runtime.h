#pragma once
#include <windows.h>
#include <filesystem>
#include <string>
#include <vector>
#include "config.h"
#include "native/runtime_ownership.h"
void InitializeCOM();
class MayaRuntime {
public:
    explicit MayaRuntime(std::filesystem::path directory, unsigned short port = 65535,
        std::filesystem::path profile=L"maya-ds4.json");
    std::vector<Process> Owned();
    std::wstring Start();
    std::wstring Stop();
    bool Ready();
    bool IsServer(const Process& p) const;
    bool IsChild(const Process& p) const;
    bool IsSupervisor(const Process& p) const;
private:
    std::filesystem::path root;
    std::filesystem::path configPath;
    std::wstring python;
    unsigned short port;
    std::wstring host;
};

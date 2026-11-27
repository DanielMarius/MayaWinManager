#include "runtime.h"
#include <wbemidl.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <winhttp.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <cwctype>
using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;
namespace {
void Check(HRESULT hr) { if (FAILED(hr)) throw std::runtime_error("Windows management operation failed."); }
struct BString {
    BSTR value;
    explicit BString(const wchar_t* s) : value(SysAllocString(s)) {}
    ~BString() { SysFreeString(value); }
    operator BSTR() const { return value; }
};
ComPtr<IWbemServices> Management() {
    ComPtr<IWbemLocator> locator;
    Check(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator)));
    ComPtr<IWbemServices> service;
    Check(locator->ConnectServer(BString(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &service));
    Check(CoSetProxyBlanket(service.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE));
    return service;
}
std::wstring Property(IWbemClassObject* object, const wchar_t* name) {
    VARIANT v; VariantInit(&v);
    Check(object->Get(name, 0, &v, nullptr, nullptr));
    std::wstring result;
    if (v.vt == VT_BSTR && v.bstrVal) result = v.bstrVal;
    else if (v.vt == VT_I4) result = std::to_wstring(v.lVal);
    else if (v.vt == VT_UI4) result = std::to_wstring(v.ulVal);
    VariantClear(&v); return result;
}
unsigned long long Creation(const std::wstring& date) {
    if (date.size() != 25 || date[14] != L'.') throw std::runtime_error("Cannot verify process creation time.");
    auto number = [&](size_t at, size_t n) { return static_cast<WORD>(std::stoul(date.substr(at, n))); };
    SYSTEMTIME s{};
    s.wYear=number(0,4); s.wMonth=number(4,2); s.wDay=number(6,2);
    s.wHour=number(8,2); s.wMinute=number(10,2); s.wSecond=number(12,2);
    auto micro=std::stoul(date.substr(15,6)); s.wMilliseconds=static_cast<WORD>(micro/1000);
    FILETIME f{}; if (!SystemTimeToFileTime(&s,&f)) throw std::runtime_error("Invalid process time.");
    ULARGE_INTEGER ticks{}; ticks.LowPart=f.dwLowDateTime; ticks.HighPart=f.dwHighDateTime;
    auto offset=static_cast<long long>(std::stol(date.substr(22,3)))*600000000LL;
    return ticks.QuadPart + (micro%1000)*10 - (date[21]==L'+' ? offset : -offset);
}
std::vector<Process> Snapshot() {
    auto service=Management(); ComPtr<IEnumWbemClassObject> entries;
    Check(service->ExecQuery(BString(L"WQL"), BString(L"SELECT ProcessId,ParentProcessId,ExecutablePath,CommandLine,CreationDate FROM Win32_Process WHERE Name='python.exe' OR Name='pythonw.exe' OR Name='maya-ds4.exe'"),
        WBEM_FLAG_FORWARD_ONLY|WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &entries));
    std::vector<Process> result;
    for (;;) {
        ComPtr<IWbemClassObject> item; ULONG count=0;
        Check(entries->Next(10000,1,&item,&count)); if (!count) break;
        auto exe=Property(item.Get(),L"ExecutablePath"); if (exe.empty()) continue;
        Process p; p.id=std::stoul(Property(item.Get(),L"ProcessId"));
        p.parent=std::stoul(Property(item.Get(),L"ParentProcessId")); p.exe=exe;
        p.command=Property(item.Get(),L"CommandLine"); p.created=Creation(Property(item.Get(),L"CreationDate"));
        result.push_back(p);
    }
    return result;
}
bool PortBusy(unsigned short port) {
    if (!port) return false;
    DWORD size=0; GetExtendedTcpTable(nullptr,&size,FALSE,AF_INET,TCP_TABLE_OWNER_PID_LISTENER,0);
    std::vector<BYTE> bytes(size);
    if (GetExtendedTcpTable(bytes.data(),&size,FALSE,AF_INET,TCP_TABLE_OWNER_PID_LISTENER,0)!=NO_ERROR)
        throw std::runtime_error("Cannot check Maya's port safely.");
    auto table=reinterpret_cast<MIB_TCPTABLE_OWNER_PID*>(bytes.data());
    for (DWORD i=0;i<table->dwNumEntries;++i) {
        DWORD p=table->table[i].dwLocalPort;
        if (static_cast<unsigned short>(((p&255)<<8)|((p>>8)&255))==port) return true;
    }
    return false;
}
struct Held {
    HANDLE handle{}; Process process;
    ~Held() { if(handle) CloseHandle(handle); }
};
HANDLE Verify(const Process& p) {
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_TERMINATE|SYNCHRONIZE,FALSE,p.id);
    if (!h) throw std::runtime_error("A Maya process changed or cannot be accessed. Nothing was stopped; retry.");
    FILETIME created{},exit{},kernel{},user{}; wchar_t exe[32768]{}; DWORD length=32768;
    bool valid=GetProcessTimes(h,&created,&exit,&kernel,&user) && QueryFullProcessImageNameW(h,0,exe,&length);
    ULARGE_INTEGER time{}; time.LowPart=created.dwLowDateTime; time.HighPart=created.dwHighDateTime;
    if (!MayaRuntimeProcessIdentityMatches(p, valid, time.QuadPart, exe)) {
        CloseHandle(h); throw std::runtime_error("Process identity changed. Nothing was stopped; retry.");
    }
    return h;
}
}
void InitializeCOM() {
    Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    HRESULT hr=CoInitializeSecurity(nullptr,-1,nullptr,nullptr,RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,nullptr,EOAC_NONE,nullptr);
    if (hr!=RPC_E_TOO_LATE) Check(hr);
}
MayaRuntime::MayaRuntime(fs::path directory,unsigned short port,fs::path profile)
    : root(std::move(directory)),configPath(profile.is_absolute()?std::move(profile):root/profile),port(port) {
    Configuration config(root,configPath); host=config.ConnectHost();
    if(this->port==65535) this->port=config.port;
    std::wifstream file(root/L".venv"/L"pyvenv.cfg"); std::wstring line;
    while (std::getline(file,line)) if (line.rfind(L"home = ",0)==0) {
        if (!line.empty() && line.back()==L'\r') line.pop_back();
        python=(fs::path(line.substr(7))/L"python.exe").wstring(); break;
    }
    if (python.empty()) throw std::runtime_error("Maya's Python environment is missing.");
}
bool MayaRuntime::IsServer(const Process& p) const {
    return MayaRuntimeIsServer(root, python, p);
}
bool MayaRuntime::IsChild(const Process& p) const {
    return MayaRuntimeIsChild(root, python, p);
}
bool MayaRuntime::IsSupervisor(const Process& p) const {
    return MayaRuntimeIsSupervisor(root, python, p);
}
std::vector<Process> MayaRuntime::Owned() {
    auto all=Snapshot(); std::vector<Process> owned;
    for (auto p:all) if (IsServer(p)) { p.server=true; owned.push_back(p); }
    for (auto p:all) if (IsSupervisor(p) &&
        std::none_of(owned.begin(),owned.end(),[&](auto& q){return q.id==p.id;})) owned.push_back(p);
    bool changed;
    do {
        changed=false;
        for (auto p:all) if (IsChild(p) &&
            std::none_of(owned.begin(),owned.end(),[&](auto& q){return q.id==p.id;}) &&
            std::any_of(owned.begin(),owned.end(),[&](auto& q){return q.id==p.parent && p.created>=q.created;})) {
            owned.push_back(p); changed=true;
        }
    } while(changed);
    return owned;
}
bool MayaRuntime::Ready() {
    if (!port) return false;
    HINTERNET session=WinHttpOpen(L"Maya Control",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0);
    if(!session) return false;
    WinHttpSetTimeouts(session,500,500,500,500);
    auto connection=WinHttpConnect(session,host.c_str(),port,0);
    auto request=connection ? WinHttpOpenRequest(connection,L"GET",L"/health",nullptr,nullptr,nullptr,0) : nullptr;
    DWORD status=0,size=sizeof(status);
    if(request && WinHttpSendRequest(request,nullptr,0,nullptr,0,0,0) && WinHttpReceiveResponse(request,nullptr))
        WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr);
    if(request) WinHttpCloseHandle(request); if(connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session); return status==200;
}
std::wstring MayaRuntime::Start() {
    if(!Owned().empty()) return Ready() ? L"Maya is running." : L"Maya is already starting.";
    for(auto& p:Snapshot()) if(SamePath(p.exe,(root/L"build"/L"maya-ds4.exe").wstring()))
        throw std::runtime_error("A Maya engine exists without a verified server. Inspect it before starting another.");
    if(PortBusy(port)) throw std::runtime_error("The configured port belongs to another process. It was left untouched.");
    const auto pythonExe=root/L".venv"/L"Scripts"/L"python.exe";
    const auto launcher=root/L"maya.py";
    if(!fs::exists(pythonExe) || !fs::exists(launcher)) throw std::runtime_error("Maya's private Python launcher is missing.");
    Configuration config(root,configPath);
    std::wstring command=L"\""+pythonExe.wstring()+L"\" -u \""+launcher.wstring()+L"\" --config \""+configPath.wstring()+L"\" --host \""+config.host+L"\" --port "+std::to_wstring(port);
    const auto compilerTemp=root/L"build"/L"manager-compiler-tmp";
    std::error_code tempError;
    fs::create_directories(compilerTemp,tempError);
    if(tempError || !fs::is_directory(compilerTemp)) throw std::runtime_error("Maya's compiler temp directory is unavailable.");
    if(!SetEnvironmentVariableW(L"TEMP",compilerTemp.c_str()) || !SetEnvironmentVariableW(L"TMP",compilerTemp.c_str()))
        throw std::runtime_error("Windows could not set Maya's compiler temp directory (error "+std::to_string(GetLastError())+").");
    SetEnvironmentVariableW(L"PYTHONIOENCODING",L"utf-8");
    STARTUPINFOW startup{}; startup.cb=sizeof(startup); startup.dwFlags=STARTF_USESHOWWINDOW; startup.wShowWindow=SW_HIDE;
    PROCESS_INFORMATION process{};
    if(!CreateProcessW(pythonExe.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process))
        throw std::runtime_error("Windows could not start Maya (CreateProcess error "+std::to_string(GetLastError())+").");
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    return L"Maya is starting hidden. Allow 1-2 minutes to load.";
}
std::wstring MayaRuntime::Stop() {
    auto owned=Owned(); if(owned.empty()) return L"Maya is not running.";
    if(owned.size()>MAXIMUM_WAIT_OBJECTS) throw std::runtime_error("Unexpected Maya process count; nothing was stopped.");
    std::vector<std::unique_ptr<Held>> handles;
    for(auto& p:owned) { auto h=std::make_unique<Held>(); h->process=p; h->handle=Verify(p); handles.push_back(std::move(h)); }
    // Hold verified handles before stopping anything; PID reuse cannot redirect a stop.
    for(auto& h:handles) if(h->process.server && WaitForSingleObject(h->handle,0)==WAIT_TIMEOUT)
        if(!TerminateProcess(h->handle,0)) throw std::runtime_error("Windows could not stop the Maya server.");
    std::vector<HANDLE> wait; for(auto& h:handles) wait.push_back(h->handle);
    WaitForMultipleObjects(static_cast<DWORD>(wait.size()),wait.data(),TRUE,8000);
    for(auto& h:handles) if(WaitForSingleObject(h->handle,0)==WAIT_TIMEOUT)
        if(!TerminateProcess(h->handle,0)) throw std::runtime_error("Windows could not stop a verified Maya child.");
    if(WaitForMultipleObjects(static_cast<DWORD>(wait.size()),wait.data(),TRUE,10000)!=WAIT_OBJECT_0)
        throw std::runtime_error("Maya has not finished stopping. Restart was cancelled.");
    return L"Maya stopped.";
}

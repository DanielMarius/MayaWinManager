#include "runtime.h"
#include "native/update_endpoint.h"
#include "native/update_preflight.h"
#include "native/update_preserve.h"
#include "native/github_repo.h"
#include <winhttp.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <algorithm>
#include <array>
#include <fstream>
#include <cctype>
#ifdef MAYA_WORKER_INSTALL_LAUNCH_TEST
extern "C" HINSTANCE WINAPI MayaWorkerInstallLaunchTestShellExecute(HWND, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, INT);
#define MAYA_WORKER_INSTALL_SHELL_EXECUTE MayaWorkerInstallLaunchTestShellExecute
#else
#define MAYA_WORKER_INSTALL_SHELL_EXECUTE ShellExecuteW
#endif
Json JsonEndpoint(const Configuration& config, const wchar_t *path) {
    HINTERNET session=WinHttpOpen(L"Maya Control",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0);
    if(!session) return {};
    WinHttpSetTimeouts(session,750,750,750,750);
    auto connection=WinHttpConnect(session,config.ConnectHost().c_str(),config.port,0);
    auto request=connection?WinHttpOpenRequest(connection,L"GET",path,nullptr,nullptr,nullptr,0):nullptr;
    std::string body; DWORD status=0,size=sizeof(status);
    if(request && WinHttpSendRequest(request,nullptr,0,nullptr,0,0,0) && WinHttpReceiveResponse(request,nullptr)) {
        WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr);
        char buffer[4096]; DWORD count=0;
        while(body.size()<65536 && WinHttpReadData(request,buffer,sizeof(buffer),&count) && count) body.append(buffer,count);
    }
    if(request) WinHttpCloseHandle(request); if(connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    if(status!=200) return {};
    return Json::parse(body,nullptr,false);
}
Json Health(const Configuration& config) { return JsonEndpoint(config,L"/health"); }
Json ServerStatus(const Configuration& config) { return JsonEndpoint(config,L"/status"); }
bool HealthHasModel(const Json& health) {
    return health.is_object() && health.contains("model") && health["model"].is_string() &&
        !health["model"].get<std::string>().empty();
}
bool HealthModelReady(const Json& health) {
    return HealthHasModel(health) && (!health.contains("unloaded") ||
        (health["unloaded"].is_boolean() && !health["unloaded"].get<bool>()));
}
void WaitForMayaIdle(const Configuration &config, DWORD timeoutMs=1800000) {
    const auto deadline=GetTickCount64()+timeoutMs;
    int idleSamples=0;
    while(GetTickCount64()<deadline) {
        const auto status=ServerStatus(config);
        const bool verified=status.is_object() && status.contains("busy") && status["busy"].is_boolean() &&
            !status["busy"].get<bool>() && status.contains("queued") && status["queued"].is_number_integer() &&
            status["queued"].get<int64_t>()==0;
        if(verified) {
            if(++idleSamples>=2) return;
        } else idleSamples=0;
        Sleep(1000);
    }
    throw std::runtime_error("Maya's request queue did not become verifiably idle within 30 minutes; the update was not started.");
}
bool WaitForHealthyModel(const Configuration &config, const std::string &version, DWORD timeoutMs) {
    const auto deadline=GetTickCount64()+timeoutMs;
    while(GetTickCount64()<deadline) {
        const auto health=Health(config);
        if(HealthModelReady(health) && health.value("version",std::string())==version) return true;
        Sleep(1000);
    }
    return false;
}
Json UpdateRequest(const Configuration& config,bool post,bool force=false) {
    HINTERNET session=WinHttpOpen(L"Maya Control",WINHTTP_ACCESS_TYPE_NO_PROXY,nullptr,nullptr,0);
    if(!session) throw std::runtime_error("Cannot open Maya's update endpoint.");
    WinHttpSetTimeouts(session,3000,3000,5000,5000);
    auto connection=WinHttpConnect(session,config.ConnectHost().c_str(),config.port,0);
    auto request=connection?WinHttpOpenRequest(connection,post?L"POST":L"GET",MayaUpdateEndpointPath(post,force),nullptr,nullptr,nullptr,0):nullptr;
    std::string body; DWORD status=0,size=sizeof(status);
    std::wstring headers=L"Content-Type: application/json\r\n";
    if(!config.apiKey.empty()) {
        if(config.apiKey.find_first_of(L"\r\n")!=std::wstring::npos) {
            if(request) WinHttpCloseHandle(request); if(connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
            throw std::runtime_error("The configured API key contains invalid header characters.");
        }
        headers+=L"Authorization: Bearer "+config.apiKey+L"\r\n";
    }
    BOOL sent=request && WinHttpAddRequestHeaders(request,headers.c_str(),static_cast<DWORD>(headers.size()),WINHTTP_ADDREQ_FLAG_ADD) &&
        WinHttpSendRequest(request,nullptr,0,post?const_cast<char*>("{}"):nullptr,post?2:0,post?2:0,0) &&
        WinHttpReceiveResponse(request,nullptr);
    if(sent) {
        WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&size,nullptr);
        char buffer[4096]; DWORD count=0;
        while(body.size()<65536 && WinHttpReadData(request,buffer,sizeof(buffer),&count) && count) body.append(buffer,count);
    }
    if(request) WinHttpCloseHandle(request); if(connection) WinHttpCloseHandle(connection); WinHttpCloseHandle(session);
    if(!sent) throw std::runtime_error("Maya's update endpoint could not be reached.");
    auto json=Json::parse(body,nullptr,false);
    if(json.is_discarded()) throw std::runtime_error("Maya returned an invalid update response.");
    if(status!=200 && status!=202) {
        std::string message="Maya rejected the update request.";
        if(json.contains("error") && json["error"].is_object() && json["error"].contains("message") && json["error"]["message"].is_string()) message=json["error"]["message"].get<std::string>();
        throw std::runtime_error(message);
    }
    return json;
}
std::wstring Quote(const std::wstring& value) {
    std::wstring out=L"\""; size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\') { ++slashes; continue; }
        if(c==L'\"') out.append(slashes*2+1,L'\\'); else out.append(slashes,L'\\');
        out+=c; slashes=0;
    }
    out.append(slashes*2,L'\\'); out+=L'\"'; return out;
}
std::string Utf8Output(const std::string& value) {
    if(value.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(size) return value;
    size=MultiByteToWideChar(CP_ACP,0,value.data(),static_cast<int>(value.size()),nullptr,0);
    if(!size) return "Dependency output could not be decoded using the Windows code page.";
    std::wstring wide(size,L' ');
    MultiByteToWideChar(CP_ACP,0,value.data(),static_cast<int>(value.size()),wide.data(),size);
    return Utf8(wide);
}
std::pair<bool,std::string> DependencyCheck(const std::filesystem::path& root,const std::filesystem::path& profile) {
    auto config=profile.is_absolute()?profile:root/profile;
    auto python=root/L".venv"/L"Scripts"/L"python.exe";
    if(!std::filesystem::exists(python)) return {false,"Maya's private Python environment is missing."};
    std::ifstream configFile(config, std::ios::binary);
    if (!configFile) return {false, "The selected Maya model config could not be read."};
    const auto modelConfig = Json::parse(configFile, nullptr, false);
    if (modelConfig.is_discarded() || !modelConfig.is_object())
        return {false, "The selected Maya model config is invalid JSON."};
    const auto model = MayaDependencyCheckModel(modelConfig);
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE}; HANDLE read=nullptr,write=nullptr;
    HANDLE nullFile=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
    if(nullFile==INVALID_HANDLE_VALUE || !CreatePipe(&read,&write,&security,0) || !SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)) {
        if(read) CloseHandle(read); if(write) CloseHandle(write); if(nullFile!=INVALID_HANDLE_VALUE) CloseHandle(nullFile);
        return {false,"Cannot start Maya's read-only dependency check."};
    }
    STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    startup.wShowWindow=SW_HIDE; startup.hStdOutput=write; startup.hStdError=write; startup.hStdInput=nullFile;
    PROCESS_INFORMATION process{};
    std::wstring command=Quote(python.wstring())+L" "+Quote((root/L"maya.py").wstring())+L" --check"+
        (model.empty() ? L"" : L" --model "+model)+L" --config "+Quote(config.wstring());
    BOOL created=CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process);
    CloseHandle(write); CloseHandle(nullFile); write=nullptr;
    if(!created) { CloseHandle(read); return {false,"Maya's read-only dependency check could not start."}; }
    std::string output; DWORD exitCode=STILL_ACTIVE; ULONGLONG began=GetTickCount64(); bool timeout=false;
    for(;;) {
        DWORD available=0;
        if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr) && available) {
            char buffer[2048]; DWORD count=0;
            if(ReadFile(read,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr) && count) {
                if(output.size()<30000) output.append(buffer,std::min<size_t>(count,30000-output.size()));
            }
        }
        if(WaitForSingleObject(process.hProcess,100)==WAIT_OBJECT_0) {
            DWORD available=0; if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr) && available) continue;
            GetExitCodeProcess(process.hProcess,&exitCode); break;
        }
        if(GetTickCount64()-began>120000) { timeout=true; TerminateProcess(process.hProcess,1); WaitForSingleObject(process.hProcess,2000); break; }
    }
    CloseHandle(read); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    bool ok=!timeout && exitCode==0 && output.find("This PC can run Maya.")!=std::string::npos;
    if(timeout) output="Dependency check timed out after 2 minutes.\n"+output;
    return {ok,Utf8Output(output)};
}
bool RunCheck(const std::filesystem::path& root,const std::filesystem::path& python,const std::wstring& prefix,
    std::string& output) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE}; HANDLE read=nullptr,write=nullptr;
    HANDLE nullFile=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
    if(nullFile==INVALID_HANDLE_VALUE || !CreatePipe(&read,&write,&security,0) || !SetHandleInformation(read,HANDLE_FLAG_INHERIT,0)) {
        if(read) CloseHandle(read); if(write) CloseHandle(write); if(nullFile!=INVALID_HANDLE_VALUE) CloseHandle(nullFile);
        output="Cannot start Maya's read-only dependency check."; return false;
    }
    STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
    startup.wShowWindow=SW_HIDE; startup.hStdOutput=write; startup.hStdError=write; startup.hStdInput=nullFile;
    PROCESS_INFORMATION process{};
    std::wstring command=Quote(python.wstring())+MayaPythonSelectorArgument(prefix)+L" "+Quote((root/L"maya.py").wstring())+
        L" --check";
    BOOL created=CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,root.c_str(),&startup,&process);
    CloseHandle(write); CloseHandle(nullFile);
    if(!created) { CloseHandle(read); output="Maya's read-only dependency check could not start."; return false; }
    DWORD exitCode=STILL_ACTIVE; ULONGLONG began=GetTickCount64(); bool timeout=false;
    for(;;) {
        DWORD available=0;
        if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr) && available) {
            char buffer[2048]; DWORD count=0;
            if(ReadFile(read,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr) && count && output.size()<30000)
                output.append(buffer,std::min<size_t>(count,30000-output.size()));
        }
        if(WaitForSingleObject(process.hProcess,100)==WAIT_OBJECT_0) {
            DWORD available=0; if(PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr) && available) continue;
            GetExitCodeProcess(process.hProcess,&exitCode); break;
        }
        if(GetTickCount64()-began>120000) { timeout=true; TerminateProcess(process.hProcess,1); WaitForSingleObject(process.hProcess,2000); break; }
    }
    CloseHandle(read); CloseHandle(process.hThread); CloseHandle(process.hProcess);
    if(timeout) output="Dependency check timed out after 2 minutes.\n"+output;
    return !timeout && exitCode==0 && output.find("This PC can run Maya.")!=std::string::npos;
}
std::string FreshCheck(const std::filesystem::path& root) {
    auto validPython=[](const std::filesystem::path& path,const std::wstring& prefix) {
        std::wstring args=prefix+L" -c \"import sys,venv,ensurepip; sys.exit(0 if sys.version_info >= (3,10) and sys.maxsize > 2**32 else 1)\"";
        SHELLEXECUTEINFOW info{sizeof(info)}; info.fMask=SEE_MASK_NOCLOSEPROCESS; info.lpVerb=L"open";
        info.lpFile=path.c_str(); info.lpParameters=args.c_str(); info.nShow=SW_HIDE;
        if(!ShellExecuteExW(&info) || !info.hProcess) return false;
        bool ok=WaitForSingleObject(info.hProcess,15000)==WAIT_OBJECT_0; DWORD code=1;
        if(ok) GetExitCodeProcess(info.hProcess,&code); CloseHandle(info.hProcess); return ok && code==0;
    };
    std::filesystem::path python; std::wstring prefix;
    std::array<std::pair<std::filesystem::path,std::wstring>,2> commands{{
        {L"py.exe",L"-3"},{L"python.exe",L""}}};
    for(auto& [name,args]:commands) {
        std::array<wchar_t,32768> found{}; DWORD n=SearchPathW(nullptr,name.c_str(),nullptr,static_cast<DWORD>(found.size()),found.data(),nullptr);
        if(n && n<found.size() && validPython(found.data(),args)) { python=found.data(); prefix=args; break; }
    }
    if(python.empty()) {
        wchar_t* local=nullptr; size_t length=0;
        if(_wdupenv_s(&local,&length,L"LOCALAPPDATA")==0 && local) {
            for(const wchar_t* version:{L"313",L"312",L"311",L"310"}) {
                auto candidate=std::filesystem::path(local)/L"Programs"/L"Python"/(L"Python"+std::wstring(version))/L"python.exe";
                if(std::filesystem::exists(candidate) && validPython(candidate,L"")) { python=candidate; break; }
            }
            free(local);
        }
    }
    std::string report="Fresh install preflight\r\n";
    if(python.empty()) return report+"\r\nSystem Python 3.10+ x64 with venv and ensurepip was not found.\r\nInstall Python, then check again. Maya setup will create its private .venv.";
    std::string check; bool ready=RunCheck(root,python,prefix,check);
    report+="\r\nSystem Python: "+Utf8(python.wstring())+"\r\n";
    report+=Utf8Output(check);
    return report+"\r\nFresh-install prerequisites: "+std::string(ready?"PASS":"BLOCKED")+
        "\r\nMaya setup will create the private .venv and ask about model download.";
}
std::pair<DWORD,std::string> RunHidden(const std::filesystem::path& exe,const std::wstring& args,const std::filesystem::path& cwd,DWORD timeout) {
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};HANDLE rd=nullptr,wr=nullptr;if(!CreatePipe(&rd,&wr,&sa,0)||!SetHandleInformation(rd,HANDLE_FLAG_INHERIT,0))throw std::runtime_error("Cannot create updater output pipe.");
    HANDLE nul=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);STARTUPINFOW si{sizeof(si)};si.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;si.wShowWindow=SW_HIDE;si.hStdOutput=wr;si.hStdError=wr;si.hStdInput=nul;
    PROCESS_INFORMATION pi{};std::wstring cmd=Quote(exe.wstring())+L" "+args;BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cwd.c_str(),&si,&pi);CloseHandle(wr);if(nul!=INVALID_HANDLE_VALUE)CloseHandle(nul);if(!ok){CloseHandle(rd);throw std::runtime_error("Could not start the Maya updater command.");}
    std::string out;ULONGLONG end=GetTickCount64()+timeout;char b[4096];while(WaitForSingleObject(pi.hProcess,50)!=WAIT_OBJECT_0&&GetTickCount64()<end){DWORD n=0;if(PeekNamedPipe(rd,nullptr,0,nullptr,&n,nullptr)&&n){DWORD got=0;if(ReadFile(rd,b,std::min<DWORD>(n,sizeof(b)),&got,nullptr)&&got&&out.size()<65536)out.append(b,std::min<size_t>(got,65536-out.size()));}}
    DWORD code=1;
    if(WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0){DWORD n=0;while(PeekNamedPipe(rd,nullptr,0,nullptr,&n,nullptr)&&n){DWORD got=0;if(!ReadFile(rd,b,std::min<DWORD>(n,sizeof(b)),&got,nullptr)||!got)break;if(out.size()<65536)out.append(b,std::min<size_t>(got,65536-out.size()));}GetExitCodeProcess(pi.hProcess,&code);}
    else {TerminateProcess(pi.hProcess,ERROR_TIMEOUT);WaitForSingleObject(pi.hProcess,5000);code=ERROR_TIMEOUT;}
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);CloseHandle(rd);return {code,out};
}
std::filesystem::path GitPath(){std::array<wchar_t,32768> p{};DWORD n=SearchPathW(nullptr,L"git.exe",nullptr,(DWORD)p.size(),p.data(),nullptr);if(!n||n>=p.size())throw std::runtime_error("Git for Windows is required for an offline upgrade.");return p.data();}
std::string Git(const std::filesystem::path& root,const std::wstring& args,DWORD timeout=60000){auto r=RunHidden(GitPath(),L"-C "+Quote(root.wstring())+L" "+args,root,timeout);if(r.first){const auto reason=r.first==ERROR_TIMEOUT?std::string("timed out"):"exited with code "+std::to_string(r.first);throw std::runtime_error("Git "+reason+" while running '"+Utf8(args)+"'. "+(r.second.empty()?std::string("Git returned no error text."):r.second));}return r.second;}
std::vector<int> VersionParts(std::string v){if(!v.empty()&&(v[0]=='v'||v[0]=='V'))v.erase(0,1);std::vector<int> p;size_t n=0;while(n<v.size()){size_t e=v.find('.',n);auto s=v.substr(n,e==std::string::npos?e:e-n);if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos)return {};p.push_back(std::stoi(s));if(e==std::string::npos)break;n=e+1;}return p.size()==3?p:std::vector<int>{};}
Json LatestRelease(const std::string& repo){auto s=WinHttpOpen(L"Maya Win Manager",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,nullptr,nullptr,0);if(!s)throw std::runtime_error("Cannot open a connection to GitHub.");WinHttpSetTimeouts(s,3000,3000,8000,8000);auto c=WinHttpConnect(s,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0);auto path=Wide("/repos/"+repo+"/releases/latest");auto q=c?WinHttpOpenRequest(c,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):nullptr;std::wstring h=L"Accept: application/vnd.github+json\r\nUser-Agent: Maya-Win-Manager\r\n";BOOL sent=q&&WinHttpAddRequestHeaders(q,h.c_str(),(DWORD)h.size(),WINHTTP_ADDREQ_FLAG_ADD)&&WinHttpSendRequest(q,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)&&WinHttpReceiveResponse(q,nullptr);DWORD status=0,z=sizeof(status);std::string body;if(sent){WinHttpQueryHeaders(q,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&status,&z,nullptr);char b[4096];DWORD n=0;while(body.size()<65536&&WinHttpReadData(q,b,sizeof(b),&n)&&n)body.append(b,n);}if(q)WinHttpCloseHandle(q);if(c)WinHttpCloseHandle(c);WinHttpCloseHandle(s);if(!sent||status!=200)throw std::runtime_error("GitHub release check failed (HTTP "+std::to_string(status)+").");auto j=Json::parse(body,nullptr,false);if(j.is_discarded())throw std::runtime_error("GitHub returned invalid release metadata.");return j;}
Json OfflineUpdate(const std::filesystem::path &root, const Configuration &cfg, const std::filesystem::path &profile,
                   bool allowRunning=false)
{
    if (!std::filesystem::exists(root / L".git")) throw std::runtime_error("Offline upgrade requires a Git checkout.");
    const auto repo = MayaGitHubRepo(Git(root, L"remote get-url origin", 10000));
    if (repo.empty()) throw std::runtime_error("The Maya origin is not a GitHub repository.");
    const auto release = LatestRelease(repo);
    const auto tag = release.value("tag_name", std::string());
    std::ifstream versionFile(root / L"VERSION");
    std::string current;
    std::getline(versionFile, current);
    const auto currentParts = VersionParts(current), latestParts = VersionParts(tag);
    if (currentParts.empty() || latestParts.empty()) throw std::runtime_error("Could not compare Maya release versions.");
    Json update = {{"current", current},
                   {"latest", {{"version", tag.size() && tag[0] == 'v' ? tag.substr(1) : tag},
                                {"tag", tag}, {"url", release.value("html_url", std::string())}}},
                   {"newer", latestParts > currentParts}, {"can_update", false}, {"blocker", nullptr}};
    if (!update["newer"].get<bool>())
    {
        update["blocker"] = "Maya is already at the latest release.";
        return update;
    }

    std::string blocker;
    InitializeCOM();
    try
    {
        MayaRuntime runtime(root, cfg.port, profile);
        if (!allowRunning && !runtime.Owned().empty()) blocker = "Maya is running or starting. Stop Maya before an offline upgrade.";
    }
    catch (...)
    {
        CoUninitialize();
        throw;
    }
    CoUninitialize();

    std::vector<std::string> tracked;
    if (blocker.empty())
    {
        Git(root, L"fetch --tags origin", 600000);
        tracked = MayaUpdatePathList(Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames HEAD", 20000));
        const auto additions = MayaUpdatePathList(
            Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames --diff-filter=A HEAD refs/tags/" + Wide(tag), 30000));
        // Check only paths the release adds. Enumerating every untracked path under the
        // Maya root can be enormous because it includes model/cache environments; the
        // filesystem check catches exact files and file/directory ancestor collisions.
        blocker = MayaUpdateExistingPathConflict(root, additions);
        if (blocker.empty())
        {
            if (!tracked.empty())
            {
                auto snapshot = Git(root, L"stash create", 120000);
                snapshot.erase(std::remove_if(snapshot.begin(), snapshot.end(), [](unsigned char c) { return std::isspace(c); }), snapshot.end());
                if (snapshot.empty())
                    blocker = "Git could not snapshot the tracked local changes for a three-way merge check.";
                else
                {
                    const auto simulated = RunHidden(GitPath(), L"-C " + Quote(root.wstring()) + L" merge-tree --write-tree " +
                        Quote(L"refs/tags/" + Wide(tag)) + L" " + Quote(Wide(snapshot)), root, 120000);
                    if (simulated.first != 0)
                        blocker = "Local tracked changes conflict with Maya " + tag + "; the source was not changed.";
                }
            }
        }
        if (blocker.empty())
        {
            std::string currentHead, mergeBase;
            try
            {
                currentHead = Git(root, L"rev-parse HEAD", 10000);
                mergeBase = Git(root, L"merge-base HEAD refs/tags/" + Wide(tag), 30000);
            }
            catch (const std::exception &)
            {
                // Fail closed when Git cannot prove a fast-forward update.
            }
            if (!MayaUpdateCanFastForward(currentHead, mergeBase))
                blocker = "The current Maya checkout cannot be fast-forwarded to this release.";
        }
    }

    const auto dependencies = DependencyCheck(root, profile);
    update["dependency_ok"] = dependencies.first;
    update["dependency_report"] = dependencies.second;
    update["preserve_tracked_changes"] = blocker.empty() && !tracked.empty();
    update["preserved_paths"] = tracked.size();
    if (blocker.empty() && !dependencies.first) blocker = "Maya's build/dependency preflight failed.";
    update["blocker"] = blocker.empty() ? Json(nullptr) : Json(blocker);
    update["can_update"] = blocker.empty();
    return update;
}

struct OfflineUpdateBackups
{
    std::filesystem::path bundle, patch;
    std::filesystem::path sourceFiles;
    size_t sourceFileCount=0;
    uint64_t sourceBytes=0;
};

bool ByteFilesEqual(const std::filesystem::path &left, const std::filesystem::path &right)
{
    std::error_code error;
    const auto leftSize=std::filesystem::file_size(left,error);
    if(error) return false;
    const auto rightSize=std::filesystem::file_size(right,error);
    if(error || leftSize!=rightSize) return false;
    std::ifstream a(left,std::ios::binary), b(right,std::ios::binary);
    if(!a || !b) return false;
    std::array<char,65536> x{},y{};
    for(;;) {
        a.read(x.data(),static_cast<std::streamsize>(x.size()));
        b.read(y.data(),static_cast<std::streamsize>(y.size()));
        const auto an=a.gcount(), bn=b.gcount();
        if(an!=bn || !std::equal(x.begin(),x.begin()+an,y.begin())) return false;
        if(an==0) return a.eof() && b.eof();
    }
}

std::pair<size_t,uint64_t> BackupSourceFiles(const std::filesystem::path &root,
                                            const std::vector<std::string> &tracked,
                                            const std::vector<std::string> &incoming,
                                            const std::filesystem::path &backupRoot)
{
    if(std::filesystem::exists(backupRoot))
        throw std::runtime_error("The timestamped source-file backup already exists; refusing to overwrite it.");
    std::vector<std::string> paths=incoming;
    for(const auto &name:tracked)
        if(std::find(paths.begin(),paths.end(),name)==paths.end()) paths.push_back(name);
    struct Copy { std::filesystem::path source, destination; };
    std::vector<Copy> copies;
    for(const auto &name:paths) {
        if(name.empty() || name.find('\\')!=std::string::npos || name.find(':')!=std::string::npos)
            throw std::runtime_error("A Maya source path is unsafe for a Windows backup: "+name);
        const auto relative=std::filesystem::u8path(name);
        if(relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
            throw std::runtime_error("A Maya source path is not relative: "+name);
        auto source=root;
        for(const auto &part:relative) {
            if(part==L"." || part==L"..") throw std::runtime_error("A Maya source path escapes the project folder: "+name);
            source/=part;
            const DWORD attributes=GetFileAttributesW(source.c_str());
            if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_REPARSE_POINT))
                throw std::runtime_error("A Maya source path crosses a reparse point; refusing the update: "+name);
        }
        std::error_code error;
        const auto status=std::filesystem::symlink_status(source,error);
        if(error==std::errc::no_such_file_or_directory || status.type()==std::filesystem::file_type::not_found) continue;
        if(error) throw std::runtime_error("Could not inspect a source file before backup: "+name);
        if(!std::filesystem::is_regular_file(status))
            throw std::runtime_error("A changed Maya source path is not a regular file: "+name);
        copies.push_back({source,backupRoot/relative});
    }
    std::filesystem::create_directories(backupRoot);
    uint64_t bytes=0;
    size_t copied=0;
    for(const auto &item:copies) {
        std::filesystem::create_directories(item.destination.parent_path());
        if(std::filesystem::exists(item.destination))
            throw std::runtime_error("A source-file backup already exists; refusing to overwrite it.");
        std::error_code error;
        if(!std::filesystem::copy_file(item.source,item.destination,std::filesystem::copy_options::none,error) || error)
            throw std::runtime_error("Could not make a byte-for-byte source backup for: "+Utf8(item.source.wstring()));
        if(!ByteFilesEqual(item.source,item.destination))
            throw std::runtime_error("Source backup verification failed; Maya source was not changed: "+Utf8(item.source.wstring()));
        const auto size=std::filesystem::file_size(item.destination,error);
        if(error) throw std::runtime_error("Could not verify the source backup size: "+Utf8(item.destination.wstring()));
        bytes+=size;
        ++copied;
    }
    return {copied,bytes};
}

OfflineUpdateBackups ApplyOfflineUpdate(const std::filesystem::path &root, const Configuration &cfg,
                                        const std::string &tag, const std::filesystem::path &profile)
{
    if (VersionParts(tag).empty()) throw std::runtime_error("Invalid release tag.");
    const auto check = OfflineUpdate(root, cfg, profile);
    if (!check.value("can_update", false))
        throw std::runtime_error(check.value("blocker", std::string("Offline update preflight failed.")));
    if (!MayaUpdateRecheckedTagMatches(check, tag))
        throw std::runtime_error("The latest Maya release changed after the update check; check again before applying it.");
    InitializeCOM();
    try
    {
        MayaRuntime runtime(root, cfg.port, profile);
        if (!runtime.Owned().empty()) throw std::runtime_error("Maya started during preflight; update cancelled.");
    }
    catch (...)
    {
        CoUninitialize();
        throw;
    }
    CoUninitialize();

    const auto tracked = MayaUpdatePathList(Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames HEAD", 20000));
    const auto incoming = MayaUpdatePathList(
        Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames HEAD refs/tags/" + Wide(tag), 30000));
    const auto additions = MayaUpdatePathList(
        Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames --diff-filter=A HEAD refs/tags/" + Wide(tag), 30000));
    // Match the check phase: inspect only incoming additions for untracked collisions.
    auto conflict = MayaUpdateExistingPathConflict(root, additions);
    if (!conflict.empty()) throw std::runtime_error(conflict);

    const auto base = root.parent_path() / L"Maya-XS-update-backups";
    std::filesystem::create_directories(base);
    SYSTEMTIME time{};
    GetLocalTime(&time);
    wchar_t stamp[64]{};
    swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u-%lu", time.wYear, time.wMonth, time.wDay,
               time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
    const auto backupName = L"maya-" + Wide(check["current"].get<std::string>()) + L"-to-" + Wide(tag) + L"-" + stamp;
    OfflineUpdateBackups backups{base / (backupName + L".bundle"), {}, base / (backupName + L"-source-files")};
    if (std::filesystem::exists(backups.bundle))
        throw std::runtime_error("The timestamped source backup already exists; refusing to overwrite it.");
    if (std::filesystem::exists(backups.sourceFiles))
        throw std::runtime_error("The timestamped source-file backup already exists; refusing to overwrite it.");
    if (!tracked.empty())
    {
        backups.patch = base / (backupName + L"-local-changes.patch");
        if (std::filesystem::exists(backups.patch))
            throw std::runtime_error("The timestamped local-changes backup already exists; refusing to overwrite it.");
    }

    Git(root, L"bundle create " + Quote(backups.bundle.wstring()) + L" HEAD", 180000);
    Git(root, L"bundle verify " + Quote(backups.bundle.wstring()), 60000);
    const auto sourceBackup=BackupSourceFiles(root,tracked,incoming,backups.sourceFiles);
    backups.sourceFileCount=sourceBackup.first;
    backups.sourceBytes=sourceBackup.second;
    std::string stashId;
    if (!tracked.empty())
    {
        Git(root, L"diff --binary HEAD --output=" + Quote(backups.patch.wstring()), 120000);
        std::error_code error;
        if (!std::filesystem::exists(backups.patch, error) || error || std::filesystem::file_size(backups.patch, error) == 0 || error)
            throw std::runtime_error("Could not create the local-changes patch backup; the Maya source was not changed.");
        Git(root, L"stash push --message " + Quote(L"Maya Manager pre-update " + std::wstring(stamp)), 120000);
        stashId = Git(root, L"rev-parse refs/stash", 10000);
        stashId.erase(std::remove_if(stashId.begin(), stashId.end(), [](unsigned char c) { return std::isspace(c); }), stashId.end());
        if (stashId.size() != 40 && stashId.size() != 64)
            throw std::runtime_error("Could not verify the Git stash containing local changes; patch backup: " + Utf8(backups.patch.wstring()));
        if (!Git(root, L"-c core.quotepath=false diff --name-only -z --no-renames HEAD", 20000).empty())
            throw std::runtime_error("Tracked changes remain after creating the stash; patch backup: " + Utf8(backups.patch.wstring()));
    }

    try
    {
        Git(root, L"merge --ff-only refs/tags/" + Wide(tag), 120000);
    }
    catch (const std::exception &error)
    {
        if (!stashId.empty())
        {
            try { Git(root, L"stash apply --index " + Wide(stashId), 120000); }
            catch (...) { throw std::runtime_error(std::string(error.what()) + "; local edits remain in Git stash and patch backup: " + Utf8(backups.patch.wstring())); }
        }
        throw;
    }
    if (!stashId.empty())
    {
        try { Git(root, L"stash apply --index " + Wide(stashId), 120000); }
        catch (const std::exception &error)
        {
            throw std::runtime_error(std::string("Maya source updated, but local edits need manual recovery; Maya remains stopped. ") +
                                     error.what() + ". Git stash and patch backup: " + Utf8(backups.patch.wstring()));
        }
    }
    if (!tracked.empty())
    {
        const auto restored=MayaUpdatePathList(
            Git(root,L"-c core.quotepath=false diff --name-only -z HEAD",20000));
        for(const auto &path:tracked)
            if(std::find(restored.begin(),restored.end(),path)==restored.end())
                throw std::runtime_error("Maya updated, but a tracked local edit could not be verified after reapplying it. "
                    "The Git stash and verified source copies remain at: "+Utf8(backups.sourceFiles.wstring()));
    }
    return backups;
}

struct StartupLogSnapshot
{
    std::filesystem::path path;
    uint64_t offset=0;
};

std::vector<StartupLogSnapshot> CaptureStartupLogs(const std::filesystem::path &root,
                                                   const std::filesystem::path &profile)
{
    std::vector<std::filesystem::path> paths;
    auto configPath=profile.is_absolute()?profile:root/profile;
    std::ifstream configFile(configPath);
    if(configFile) {
        Json data=Json::parse(configFile,nullptr,false);
        if(data.is_object() && data.contains("log") && data["log"].is_string() && !data["log"].get<std::string>().empty()) {
            auto log=std::filesystem::u8path(data["log"].get<std::string>());
            if(!log.is_absolute()) log=root/log;
            paths.push_back(log);
        }
    }
    paths.push_back(root/L"maya-setup.log");
    std::vector<StartupLogSnapshot> snapshots;
    for(const auto &path:paths) {
        if(std::find_if(snapshots.begin(),snapshots.end(),[&](const auto &s){return s.path==path;})!=snapshots.end()) continue;
        std::error_code error;
        const auto offset=std::filesystem::exists(path,error) && !error ? std::filesystem::file_size(path,error) : 0;
        snapshots.push_back({path,error?0:offset});
    }
    return snapshots;
}

std::string StartupFailureClass(const std::vector<StartupLogSnapshot> &logs)
{
    std::string combined;
    for(const auto &log:logs) {
        std::error_code error;
        if(!std::filesystem::is_regular_file(log.path,error) || error) continue;
        const auto size=std::filesystem::file_size(log.path,error);
        if(error) continue;
        std::ifstream file(log.path,std::ios::binary);
        if(!file) continue;
        const auto start=size<log.offset?uint64_t(0):log.offset;
        file.seekg(static_cast<std::streamoff>(start),std::ios::beg);
        std::string text(256*1024,'\0');
        file.read(text.data(),static_cast<std::streamsize>(text.size()));
        text.resize(static_cast<size_t>(file.gcount()));
        combined+=text;
        combined+='\n';
    }
    std::transform(combined.begin(),combined.end(),combined.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    auto has=[&](const char *text){return combined.find(text)!=std::string::npos;};
    if((has("cuda") || has("nvcc")) && (has("error") || has("failed") || has("fatal") || has("not found") || has("compile")))
        return "CUDA engine build failure";
    if(has("ninja: build stopped") || (has("compiler") && has("error")) || (has("cmake") && has("failed")))
        return "C++ engine build failure";
    if(has("modulenotfounderror") || has("no module named") || has("python failed"))
        return "Python dependency or startup failure";
    return {};
}

Json ManagedUpdate(const std::filesystem::path &root, const Configuration &config,
                   const std::filesystem::path &profile, const Json &update, bool wasOnline)
{
    if(!MayaUpdateCheckVerified(update) || !update.value("newer",false) || !update.value("can_update",false))
        throw std::runtime_error(update.value("blocker",std::string("Manager update preflight did not pass.")));
    const auto latest=update.value("latest",Json::object());
    const auto tag=latest.value("tag",std::string());
    const auto version=latest.value("version",std::string());
    if(tag.empty() || version.empty() || VersionParts(tag).empty())
        throw std::runtime_error("The verified Maya release metadata is incomplete.");
    if(wasOnline) WaitForMayaIdle(config);
    const auto initialHead=Git(root,L"rev-parse HEAD",10000);
    InitializeCOM();
    bool ownedBeforeStop=false;
    try {
        MayaRuntime runtime(root,config.port,profile);
        ownedBeforeStop=!runtime.Owned().empty();
        if(wasOnline && !ownedBeforeStop)
            throw std::runtime_error("Maya stopped after preflight; check its status and retry the update.");
        if(ownedBeforeStop) runtime.Stop();
        CoUninitialize();
    } catch(...) { CoUninitialize(); throw; }

    OfflineUpdateBackups backups;
    try {
        backups=ApplyOfflineUpdate(root,config,tag,profile);
    } catch(const std::exception &error) {
        std::string restore="";
        std::string currentHead;
        try { currentHead=Git(root,L"rev-parse HEAD",10000); } catch(...) {}
        if(ownedBeforeStop && currentHead==initialHead) {
            try {
                InitializeCOM(); MayaRuntime runtime(root,config.port,profile); runtime.Start(); CoUninitialize();
                if(!WaitForHealthyModel(config,update.value("current",std::string()),300000))
                    restore=" Maya was restarted on the previous source, but health did not return within five minutes.";
                else restore=" Maya was restarted on the previous source.";
            } catch(const std::exception &restartError) {
                CoUninitialize();
                restore=" Maya could not be restarted after the preflight failure: "+std::string(restartError.what());
            }
        }
        throw std::runtime_error("Maya source update was not completed: "+std::string(error.what())+restore);
    }

    const auto startupLogs=CaptureStartupLogs(root,profile);
    std::string startMessage;
    try {
        InitializeCOM(); MayaRuntime runtime(root,config.port,profile); startMessage=Utf8(runtime.Start()); CoUninitialize();
    } catch(const std::exception &error) {
        CoUninitialize();
        throw std::runtime_error("Maya v"+version+" was installed and local edits were restored, but Maya could not start: "+
            std::string(error.what())+". Verified source backups: "+Utf8(backups.sourceFiles.wstring()));
    }
    bool healthy=WaitForHealthyModel(config,version,1200000);
    auto failureClass=healthy?std::string():StartupFailureClass(startupLogs);
    if(!healthy && failureClass.empty()) {
        InitializeCOM();
        bool retry=false;
        try { MayaRuntime runtime(root,config.port,profile); retry=runtime.Owned().empty(); if(retry) runtime.Start(); CoUninitialize(); }
        catch(...) { CoUninitialize(); }
        if(retry) healthy=WaitForHealthyModel(config,version,600000);
        if(!healthy) failureClass=StartupFailureClass(startupLogs);
    }
    if(!healthy) {
        const auto kind=failureClass.empty()?std::string("Maya startup/model-load failure; no CUDA or compiler error was present in the captured logs"):failureClass;
        throw std::runtime_error("Maya source is v"+version+", but the model did not become healthy after restart ("+kind+"). "
            "The verified Git bundle and byte-for-byte source copies remain at "+Utf8(backups.sourceFiles.parent_path().wstring())+
            "; local edits were restored and the local-change patch is "+Utf8(backups.patch.wstring())+
            ". Check the Maya setup log and retry Start from Win-Manager.");
    }
    Json result={{"status","Maya v"+version+" is updated; local edits were restored and the model is loaded."},
                 {"online",wasOnline},{"managed_restart",true},{"managed_complete",true},{"version",version},
                 {"source_backup",Utf8(backups.bundle.wstring())},{"source_files_backup",Utf8(backups.sourceFiles.wstring())},
                 {"source_files_backed_up",backups.sourceFileCount},{"source_files_backup_bytes",backups.sourceBytes},
                 {"local_changes_backup",backups.patch.empty()?std::string():Utf8(backups.patch.wstring())},
                 {"start_message",startMessage}};
    return result;
}

int wmain(int argc,wchar_t** argv) {
    Json result={{"status","Cannot check Maya"},{"online",false},{"address",""},{"model",""},{"detail",""}};
    try {
        if(argc!=4) throw std::runtime_error("Invalid controller request.");
        std::filesystem::path root=argv[2],profile=argv[3]; std::wstring action=argv[1],message;
        bool installed=MayaHasInstalledModelConfig(root);
        if(!installed && (action==L"status" || action==L"update-check" || action==L"install-maya")) {
            result["installed"]=false; result["mode"]="fresh"; result["address"]="Not configured";
            result["model"]="Maya is not installed yet";
            if(action==L"status") { result["status"]="Maya is not installed yet"; result["detail"]="Open Settings → Maya updates to install."; }
            else {
                if(!std::filesystem::exists(root/L"maya.py") || !std::filesystem::exists(root/L"START-MAYA.bat"))
                    throw std::runtime_error("The Maya installer files are missing from this folder.");
                auto check=FreshCheck(root); bool ready=check.find("Fresh-install prerequisites: PASS")!=std::string::npos;
                result["dependency_ok"]=ready; result["dependency_report"]=check; result["ready"]=ready;
                result["status"]=ready?"Fresh-install prerequisites passed.":"Fresh-install prerequisites need attention.";
                if(action==L"install-maya") {
                    if(!ready) throw std::runtime_error("Fresh-install preflight is blocked. Resolve the reported prerequisites first.");
                    auto bat=(root/L"START-MAYA.bat").wstring();
                    if(reinterpret_cast<INT_PTR>(MAYA_WORKER_INSTALL_SHELL_EXECUTE(nullptr,L"open",bat.c_str(),nullptr,root.c_str(),SW_SHOWNORMAL))<=32)
                        throw std::runtime_error("Could not open START-MAYA.bat.");
                    result["status"]="Maya setup opened. Finish the prompts in its window; this controller will recognize the installation after refresh.";
                }
            }
            std::cout<<result.dump(); return 0;
        }
        Configuration config(root,profile);
        result["address"]=Utf8(config.Address()); result["model"]="Configured: "+Utf8(config.model);
        if(action==L"update-check" || action==L"update-start") {
            auto health=Health(config);bool online=HealthHasModel(health);
            result["online"]=online;
            if(online) {
                auto update=UpdateRequest(config,false,true);
                auto deps=DependencyCheck(root,profile);
                const bool apiReady=MayaUpdateCheckVerified(update) && update["newer"].get<bool>() &&
                    update.contains("can_update") && update["can_update"].is_boolean() &&
                    update["can_update"].get<bool>() && deps.first;
                const auto apiBlocker=update.contains("blocker") && update["blocker"].is_string()
                    ? update["blocker"].get<std::string>() : std::string();
                const bool localEditBlocker=apiBlocker.find("changed by hand")!=std::string::npos;
                if(action==L"update-start") {
                    if(apiReady) {
                        auto started=UpdateRequest(config,true);
                        result["status"]="Update started. Maya will finish active requests, update its source, rebuild changed engine files and restart.";
                        result["update"]=started.value("update",Json::object());
                    } else if(localEditBlocker && MayaUpdateCheckVerified(update) && update["newer"].get<bool>()) {
                        auto managed=OfflineUpdate(root,config,profile,true);
                        const auto apiLatest=update.value("latest",Json::object());
                        const auto managedLatest=managed.value("latest",Json::object());
                        if(apiLatest.value("tag",std::string())!=managedLatest.value("tag",std::string()))
                            throw std::runtime_error("Maya and GitHub reported different latest tags; no update was started. Check again.");
                        if(!managed.value("can_update",false))
                            throw std::runtime_error(managed.value("blocker",std::string("Manager-managed update preflight failed."))+
                                (managed.value("dependency_report",std::string()).empty()?std::string():"\n"+managed["dependency_report"].get<std::string>()));
                        auto completed=ManagedUpdate(root,config,profile,managed,true);
                        result.update(completed.begin(),completed.end());
                    } else {
                        std::string why=apiBlocker.empty()?"Update preflight did not pass.":apiBlocker;
                        if(!deps.first) why+="\nDependency check:\n"+deps.second;
                        throw std::runtime_error(why);
                    }
                } else {
                    bool ready=apiReady, managedRestart=false;
                    if(!apiReady && localEditBlocker && MayaUpdateCheckVerified(update) && update["newer"].get<bool>()) {
                        auto managed=OfflineUpdate(root,config,profile,true);
                        if(update.value("latest",Json::object()).value("tag",std::string())==
                           managed.value("latest",Json::object()).value("tag",std::string())) {
                            const auto apiLatest=update.value("latest",Json::object());
                            if(apiLatest.contains("summary") && apiLatest["summary"].is_string()) managed["latest"]["summary"]=apiLatest["summary"];
                            if(apiLatest.contains("body") && apiLatest["body"].is_string()) managed["latest"]["body"]=apiLatest["body"];
                            update=managed;
                            ready=managed.value("can_update",false);
                            managedRestart=ready;
                        } else {
                            update["blocker"]="Maya and GitHub reported different latest tags; check again.";
                        }
                    }
                    result["update"]=update;
                    result["dependency_ok"]=update.value("dependency_ok",deps.first);
                    result["dependency_report"]=update.value("dependency_report",deps.second);
                    result["ready"]=ready;
                    result["managed_restart"]=managedRestart;
                    result["status"]=MayaUpdateStatusText(result);
                }
            } else {
                auto update=OfflineUpdate(root,config,profile);
                bool ready=MayaUpdateCheckVerified(update) && update["newer"].get<bool>() &&
                    update.contains("can_update") && update["can_update"].is_boolean() && update["can_update"].get<bool>();
                if(action==L"update-start") {
                    if(!ready) throw std::runtime_error(update.value("blocker",std::string("Offline update preflight is blocked.")));
                    auto completed=ManagedUpdate(root,config,profile,update,false);
                    result.update(completed.begin(),completed.end());
                } else {
                    result["update"]=update;
                    result["dependency_ok"]=update.value("dependency_ok",false);
                    result["dependency_report"]=update.value("dependency_report",std::string());
                    result["ready"]=ready;
                    result["managed_restart"]=ready;
                    result["status"]=MayaUpdateStatusText(result);
                }
            }
            std::cout<<result.dump(); return 0;
        }
        if(action!=L"status") {
            InitializeCOM(); MayaRuntime runtime(root,65535,profile);
            if(action==L"start") message=runtime.Start();
            else if(action==L"stop") message=runtime.Stop();
            else if(action==L"restart") { runtime.Stop(); message=runtime.Start(); }
            else throw std::runtime_error("Unknown controller action.");
            CoUninitialize();
        }
        auto health=Health(config);
        if(action==L"restart") {
            auto deadline=std::chrono::steady_clock::now()+std::chrono::minutes(10);
            while(!HealthModelReady(health) &&
                std::chrono::steady_clock::now()<deadline) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                health=Health(config);
            }
            if(!HealthModelReady(health))
                throw std::runtime_error("Maya did not finish loading within 10 minutes.");
        }
        bool online=HealthHasModel(health);
        result["online"]=online;
        result["status"]=online?"Maya is running":message.empty()?"Maya is not responding":Utf8(message);
        if(online) {
            const std::string version=health.contains("version")&&health["version"].is_string()
                ?health["version"].get<std::string>():"unknown";
            const bool images=health.contains("images")&&health["images"].is_boolean()&&health["images"].get<bool>();
            const long long maxContext=health.contains("max_context")&&health["max_context"].is_number_integer()
                ?health["max_context"].get<long long>():0;
            result["version"]=version;
            try { auto update=UpdateRequest(config,false);result["update_available"]=MayaUpdateCheckVerified(update)&&update.value("newer",false); } catch(...) {}
            const bool unloaded=health.contains("unloaded")&&health["unloaded"].is_boolean()&&health["unloaded"].get<bool>();
            result["status"]=unloaded?"Server running - model unloaded":"Maya is running";
            result["model"]=(unloaded?"Model: ":"Running: ")+health["model"].get<std::string>();
            result["vision_on"]=images;
            result["detail"]="Strata Engine | Maya v"+result["version"].get<std::string>()+
                " | Vision "+std::string(images?"ON":"OFF")+
                " | "+std::to_string(maxContext/1000)+"K Context";
        } else {
            std::ifstream versionFile(root/L"VERSION"); std::string version;
            if(versionFile && std::getline(versionFile,version)) result["version"]=version;
            result["detail"]="Strata Engine / Maya v"+result["version"].get<std::string>()+" / Vision unavailable";
        }
        std::cout<<result.dump(); return 0;
    } catch(const std::exception& e) {
        result["status"]=e.what(); std::cout<<result.dump(); return 1;
    }
}

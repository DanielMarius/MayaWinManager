#pragma once
#include "config.h"
inline Json RunWorker(const std::filesystem::path& executable,const std::filesystem::path& root,const wchar_t* action,DWORD timeoutMs=45000,
    const std::filesystem::path& profile=L"maya-ds4.json") {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    HANDLE read=nullptr,write=nullptr,nullFile=INVALID_HANDLE_VALUE; PROCESS_INFORMATION process{};
    auto close=[&]{ if(read)CloseHandle(read); if(write)CloseHandle(write);
        if(nullFile!=INVALID_HANDLE_VALUE)CloseHandle(nullFile); if(process.hThread)CloseHandle(process.hThread); if(process.hProcess)CloseHandle(process.hProcess); };
    try {
        if(!CreatePipe(&read,&write,&security,0) || !SetHandleInformation(read,HANDLE_FLAG_INHERIT,0))
            throw std::runtime_error("Cannot open controller worker channel.");
        nullFile=CreateFileW(L"NUL",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr);
        STARTUPINFOW startup{sizeof(startup)}; startup.dwFlags=STARTF_USESTDHANDLES|STARTF_USESHOWWINDOW;
        startup.wShowWindow=SW_HIDE; startup.hStdOutput=write; startup.hStdError=nullFile; startup.hStdInput=nullFile;
        std::wstring command=L"\""+executable.wstring()+L"\" "+action+L" \""+root.wstring()+L"\" \""+(profile.is_absolute()?profile:root/profile).wstring()+L"\"";
        if(!CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,
            executable.parent_path().c_str(),&startup,&process)) throw std::runtime_error("MayaControlWorker.exe could not start.");
        CloseHandle(write); write=nullptr; std::string output; auto began=GetTickCount64();
        for(;;) {
            DWORD available=0;
            if(!PeekNamedPipe(read,nullptr,0,nullptr,&available,nullptr)) break;
            if(available) {
                char buffer[4096]; DWORD count=0;
                if(!ReadFile(read,buffer,std::min<DWORD>(available,sizeof(buffer)),&count,nullptr)) break;
                output.append(buffer,count); if(output.size()>131072) throw std::runtime_error("Controller reply is too large.");
            } else if(WaitForSingleObject(process.hProcess,50)==WAIT_OBJECT_0) {
                DWORD remaining=0;
                if(!PeekNamedPipe(read,nullptr,0,nullptr,&remaining,nullptr) || !remaining) break;
            }
            if(GetTickCount64()-began>timeoutMs) throw std::runtime_error("Maya action timed out. Check Maya before retrying.");
        }
        auto result=Json::parse(output); close(); return result;
    } catch(...) {
        if(process.hProcess && WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT) {
            TerminateProcess(process.hProcess,1); WaitForSingleObject(process.hProcess,1000);
        }
        close(); throw;
    }
}

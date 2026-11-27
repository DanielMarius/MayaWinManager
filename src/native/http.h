#pragma once
#include "../config.h"
#include "chat_stream_policy.h"
#include <winhttp.h>
#include <atomic>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

struct HttpReply { DWORD status{}; std::string body; };

class MayaHttpError : public std::runtime_error {
public:
    MayaHttpError(std::string message, unsigned status) : std::runtime_error(std::move(message)), status(status) {}
    unsigned status;
};

inline std::string NativeHttpApiErrorText(const std::string &body, unsigned status, bool stream = false)
{
    const auto fallback = "HTTP " + std::to_string(status);
    const auto value = Json::parse(body, nullptr, false);
    if (!value.is_object() || !value.contains("error")) return fallback;
    const auto &error = value["error"];
    if (error.is_object() && error.contains("message") && error["message"].is_string())
    {
        const auto message = error["message"].get<std::string>();
        if (!message.empty()) return message;
    }
    if (!stream && error.is_string() && !error.get<std::string>().empty())
        return error.get<std::string>();
    return fallback;
}

class MayaHttp {
public:
    explicit MayaHttp(Configuration config) : cfg(std::move(config)) {}
    HttpReply Request(const wchar_t* method,const wchar_t* path,const Json* body=nullptr) const {
        auto session=OpenSession(); auto conn=WinHttpConnect(session,cfg.ConnectHost().c_str(),cfg.port,0);
        auto req=conn?WinHttpOpenRequest(conn,method,path,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0):nullptr;
        if(!req){if(conn)WinHttpCloseHandle(conn);WinHttpCloseHandle(session);throw std::runtime_error("Maya API connection failed.");}
        auto headers=Headers(body!=nullptr);std::string data=body?body->dump(-1,' ',false,Json::error_handler_t::replace):std::string();
        BOOL sent=WinHttpAddRequestHeaders(req,headers.c_str(),(DWORD)headers.size(),WINHTTP_ADDREQ_FLAG_ADD)&&
            WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,data.empty()?WINHTTP_NO_REQUEST_DATA:(LPVOID)data.data(),(DWORD)data.size(),(DWORD)data.size(),0)&&WinHttpReceiveResponse(req,nullptr);
        HttpReply out;if(sent){DWORD n=sizeof(out.status);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&out.status,&n,nullptr);ReadAll(req,out.body,16*1024*1024);}
        WinHttpCloseHandle(req);WinHttpCloseHandle(conn);WinHttpCloseHandle(session);
        if(!sent)throw std::runtime_error("Maya API request failed.");
        return out;
    }
    Json Get(const wchar_t* path) const { auto r=Request(L"GET",path);if(r.status<200||r.status>=300)ApiError(r);return Parse(r.body); }
    Json Post(const wchar_t* path,const Json& body) const { auto r=Request(L"POST",path,&body);if(r.status<200||r.status>=300)ApiError(r);return Parse(r.body); }
    Json ExtractAttachment(const std::string& encodedName,const std::string& bytes,
                           std::atomic<HINTERNET>& active,const std::atomic_bool& cancelled) const {
        if(encodedName.find_first_of("\r\n")!=std::string::npos)throw std::runtime_error("Invalid attachment filename.");
        if(bytes.size()>64ULL*1024*1024)throw std::runtime_error("File is over 64 MB.");
        if(cancelled.load())throw std::runtime_error("Attachment processing was cancelled.");
        auto headers=Headers(false)+L"Content-Type: application/octet-stream\r\nX-File-Name: "+Wide(encodedName)+L"\r\n";
        auto session=OpenSession();if(!session)throw std::runtime_error("Maya API connection failed.");
        WinHttpSetTimeouts(session,5000,5000,120000,120000);
        auto conn=WinHttpConnect(session,cfg.ConnectHost().c_str(),cfg.port,0);
        if(!conn){WinHttpCloseHandle(session);throw std::runtime_error("Maya API connection failed.");}
        auto req=WinHttpOpenRequest(conn,L"POST",L"/attach",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);
        if(!req){WinHttpCloseHandle(conn);WinHttpCloseHandle(session);throw std::runtime_error("Maya API connection failed.");}
        active.store(req);
        if(cancelled.load()){Cancel(active);WinHttpCloseHandle(conn);WinHttpCloseHandle(session);throw std::runtime_error("Attachment processing was cancelled.");}
        const BOOL sent=WinHttpAddRequestHeaders(req,headers.c_str(),(DWORD)headers.size(),WINHTTP_ADDREQ_FLAG_ADD)&&
            WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,bytes.empty()?WINHTTP_NO_REQUEST_DATA:const_cast<char*>(bytes.data()),
                (DWORD)bytes.size(),(DWORD)bytes.size(),0)&&WinHttpReceiveResponse(req,nullptr);
        HttpReply out;
        if(sent){DWORD n=sizeof(out.status);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&out.status,&n,nullptr);ReadAll(req,out.body,32*1024*1024);}
        if(active.exchange(nullptr)==req)WinHttpCloseHandle(req);
        WinHttpCloseHandle(conn);WinHttpCloseHandle(session);
        if(cancelled.load())throw std::runtime_error("Attachment processing was cancelled.");
        if(!sent)throw std::runtime_error("Maya attachment upload failed.");
        if(out.status<200||out.status>=300)ApiError(out);
        return Parse(out.body);
    }
    template<class Callback> void Stream(const Json& body,std::atomic<HINTERNET>& active,Callback onEvent,
                                         const std::string& resumeJob={}) const {
        auto session=OpenSession();auto conn=WinHttpConnect(session,cfg.ConnectHost().c_str(),cfg.port,0);
        if(!conn){WinHttpCloseHandle(session);throw std::runtime_error("Maya chat connection failed.");}
        const auto headers=Headers(true,true);const auto data=body.dump(-1,' ',false,Json::error_handler_t::replace);
        std::string job=resumeJob;size_t received=0;bool finished=false,wasCancelled=false,apiError=false;DWORD lastStatus=0;
        auto read=[&](const wchar_t* method,const std::wstring& path,const std::string* payload)->bool {
            auto req=WinHttpOpenRequest(conn,method,path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);
            if(!req)throw std::runtime_error("Maya chat connection failed.");
            auto h=payload?headers:Headers(false,true);
            active.store(req);bool released=false;
            auto release=[&]{if(released)return;released=true;if(active.exchange(nullptr)==req)WinHttpCloseHandle(req);else wasCancelled=true;};
            try {
                lastStatus=0;
                BOOL sent=WinHttpAddRequestHeaders(req,h.c_str(),(DWORD)h.size(),WINHTTP_ADDREQ_FLAG_ADD)&&
                    WinHttpSendRequest(req,WINHTTP_NO_ADDITIONAL_HEADERS,0,payload?const_cast<char*>(payload->data()):WINHTTP_NO_REQUEST_DATA,
                        payload?(DWORD)payload->size():0,payload?(DWORD)payload->size():0,0)&&WinHttpReceiveResponse(req,nullptr);
                if(!sent)throw std::runtime_error("Maya chat stream disconnected.");
                DWORD n=sizeof(lastStatus);WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,nullptr,&lastStatus,&n,nullptr);
                if(lastStatus<200||lastStatus>=300){std::string err;ReadAll(req,err,2*1024*1024);if(NativeChatNeedsApiKeyHint(lastStatus,payload!=nullptr))throw std::runtime_error(NativeChatApiKeyRequiredMessage);throw std::runtime_error(NativeHttpApiErrorText(err,lastStatus,true));}
                std::string pending;char buf[8192];DWORD got=0;bool ended=false;
                for(;;){got=0;if(!WinHttpReadData(req,buf,sizeof(buf),&got)||!got)break;pending.append(buf,got);size_t p=0;
                    for(;;){auto e=pending.find('\n',p);if(e==std::string::npos){pending.erase(0,p);break;}std::string line=pending.substr(p,e-p);if(!line.empty()&&line.back()=='\r')line.pop_back();p=e+1;
                        if(line.rfind("data:",0)!=0)continue;auto s=line.substr(5);while(!s.empty()&&s.front()==' ')s.erase(s.begin());if(s=="[DONE]"){ended=true;continue;}auto j=Json::parse(s,nullptr,false);if(j.is_discarded())continue;
                        if(j.contains("id")&&j["id"].is_string()&&job.empty())job=j["id"].get<std::string>();
                        if(NativeStreamChunkHasError(j)){apiError=true;throw std::runtime_error(NativeStreamErrorMessage(j));}
                        ++received;onEvent(j);
                    }
                    if(ended)break;
                }
                release();return ended;
            } catch(...) {release();throw;}
        };
        try {
            if(job.empty()) {
                finished=read(L"POST",L"/v1/chat/completions",&data);
                if(!finished&&job.empty())throw std::runtime_error("Maya chat stream ended before a resumable job was received.");
            }
            const ULONGLONG until=GetTickCount64()+600000;
            while(!finished&&!wasCancelled) {
                if(GetTickCount64()>=until)throw std::runtime_error("Maya chat stream could not reconnect within 10 minutes.");
                const auto path=Wide(NativeChatResumePath(job,received));
                try { finished=read(L"GET",path,nullptr); }
                catch(const std::exception&) {
                    if(NativeChatResumeShouldStop(lastStatus))
                        throw std::runtime_error(NativeChatResumeMissingMessage);
                    if(apiError||wasCancelled)throw;
                }
                if(!finished&&!wasCancelled)Sleep(2000);
            }
        } catch(...) { WinHttpCloseHandle(conn);WinHttpCloseHandle(session);throw; }
        WinHttpCloseHandle(conn);WinHttpCloseHandle(session);
    }
    void Cancel(std::atomic<HINTERNET>& active) const {auto h=active.exchange(nullptr);if(h)WinHttpCloseHandle(h);}
private:
    Configuration cfg;
    static HINTERNET OpenSession(){auto s=WinHttpOpen(L"Maya Native Manager/1",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,nullptr,nullptr,0);if(s)WinHttpSetTimeouts(s,5000,5000,30000,30000);return s;}
    std::wstring Headers(bool json, bool stream=false) const {std::wstring h=L"Accept: "+std::wstring(stream?L"text/event-stream":L"application/json")+L"\r\n";if(json)h+=L"Content-Type: application/json\r\n";if(!cfg.apiKey.empty()){if(cfg.apiKey.find_first_of(L"\r\n")!=std::wstring::npos)throw std::runtime_error("Maya API key contains invalid characters.");h+=L"Authorization: Bearer "+cfg.apiKey+L"\r\n";}return h;}
    static void ReadAll(HINTERNET h,std::string& out,size_t limit){char b[8192];DWORD n=0;while(out.size()<limit&&WinHttpReadData(h,b,(DWORD)std::min<size_t>(sizeof(b),limit-out.size()),&n)&&n)out.append(b,n);}
    static Json Parse(const std::string& s){auto j=Json::parse(s,nullptr,false);if(j.is_discarded())throw std::runtime_error("Maya returned invalid JSON.");return j;}
    [[noreturn]] static void ApiError(const HttpReply& r){
        throw MayaHttpError(r.status==401?NativeChatApiKeyRequiredMessage:NativeHttpApiErrorText(r.body,r.status),r.status);
    }
};

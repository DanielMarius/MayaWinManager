#pragma once
#include <windows.h>
#include <wincred.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>
#include "third_party/nlohmann/json.hpp"
using Json=nlohmann::ordered_json;
inline std::wstring Wide(const std::string& text) {
    if(text.empty()) return {};
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);
    if(!size) throw std::runtime_error("Invalid UTF-8 in Maya settings.");
    std::wstring out(size,L' '); MultiByteToWideChar(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),size); return out;
}
inline std::string Utf8(const std::wstring& text) {
    int size=WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    std::string out(size,' '); WideCharToMultiByte(CP_UTF8,0,text.data(),static_cast<int>(text.size()),out.data(),size,nullptr,nullptr); return out;
}
inline const wchar_t* MayaApiKeyCredentialName() { return L"MayaXS.NativeManager.ApiKey"; }
inline bool MayaApiKeyTextValid(const std::wstring& key) {
    return !key.empty() && key.size()<=2048 && key.find_first_of(L"\r\n")==std::wstring::npos && key.find(L'\0')==std::wstring::npos;
}
inline std::wstring ReadMayaApiKeyOverride() {
    PCREDENTIALW credential=nullptr;
    if(!CredReadW(MayaApiKeyCredentialName(),CRED_TYPE_GENERIC,0,&credential)) return {};
    std::wstring key;
    if(credential->CredentialBlob && credential->CredentialBlobSize<=CRED_MAX_CREDENTIAL_BLOB_SIZE &&
       credential->CredentialBlobSize % sizeof(wchar_t)==0)
        key.assign(reinterpret_cast<const wchar_t*>(credential->CredentialBlob),credential->CredentialBlobSize/sizeof(wchar_t));
    CredFree(credential);
    while(!key.empty() && key.back()==L'\0') key.pop_back();
    return key;
}
inline bool SaveMayaApiKeyOverride(const std::wstring& key) {
    if(!MayaApiKeyTextValid(key)) return false;
    CREDENTIALW credential{};
    credential.Type=CRED_TYPE_GENERIC;
    credential.TargetName=const_cast<LPWSTR>(MayaApiKeyCredentialName());
    credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName=const_cast<LPWSTR>(L"Maya Native Manager");
    credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<wchar_t*>(key.c_str()));
    credential.CredentialBlobSize=static_cast<DWORD>(key.size()*sizeof(wchar_t));
    return CredWriteW(&credential,0)!=FALSE;
}
inline bool ClearMayaApiKeyOverride() {
    if(CredDeleteW(MayaApiKeyCredentialName(),CRED_TYPE_GENERIC,0)) return true;
    return GetLastError()==ERROR_NOT_FOUND;
}
struct Configuration {
    std::wstring host=L"127.0.0.1",model=L"Maya";
    std::wstring apiKey;
    bool apiKeyOverride=false;
    unsigned short port=8082;
    explicit Configuration(const std::filesystem::path& root,const std::filesystem::path& profile=L"maya-ds4.json") {
        auto config=profile.is_absolute()?profile:root/profile;
        auto name=config.filename().wstring();
        if(name!=L"maya-ds4.json" && name!=L"maya-ds4-abliterated.json") throw std::runtime_error("Unknown Maya profile.");
        std::ifstream file(config); if(!file) throw std::runtime_error("Cannot read the selected Maya profile.");
        Json data; file>>data;
        host=Wide(data.value("host",std::string("127.0.0.1")));
        model=Wide(data.value("model_name",std::string("Maya")));
        auto key=data.value("api_key",std::string());
        auto separator=key.find(','); if(separator!=std::string::npos) key.resize(separator);
        apiKey=ReadMayaApiKeyOverride();
        apiKeyOverride=!apiKey.empty();
        if(apiKey.empty()) apiKey=Wide(key);
        int p=data.value("port",8082);
        if(p<1 || p>65535) throw std::runtime_error("The configured port must be between 1 and 65535.");
        port=static_cast<unsigned short>(p);
        if(host.empty() || host.size()>253 || host.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.:-_[]")!=std::wstring::npos)
            throw std::runtime_error("The configured host is invalid.");
    }
    std::wstring ConnectHost() const { return host==L"0.0.0.0"?L"127.0.0.1":host==L"::"?L"::1":host; }
    std::wstring Address() const { return host+L":"+std::to_wstring(port); }
};
inline bool SamePath(std::wstring a,std::wstring b) {
    auto normalize=[](std::wstring& s){ for(auto& c:s) c=c==L'/'?L'\\':static_cast<wchar_t>(towlower(c)); };
    normalize(a); normalize(b); return a==b;
}

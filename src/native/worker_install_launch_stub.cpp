#include <windows.h>
#include <string>

extern "C" HINSTANCE WINAPI MayaWorkerInstallLaunchTestShellExecute(
    HWND, LPCWSTR verb, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, INT showCommand)
{
    if (!verb || !file || parameters || !directory || showCommand != SW_SHOWNORMAL || lstrcmpiW(verb, L"open") != 0)
        return nullptr;

    std::wstring expectedFile(directory);
    if (!expectedFile.empty() && expectedFile.back() != L'\\' && expectedFile.back() != L'/') expectedFile += L'\\';
    expectedFile += L"START-MAYA.bat";
    if (lstrcmpiW(file, expectedFile.c_str()) != 0) return nullptr;

    std::wstring recordPath(directory);
    if (!recordPath.empty() && recordPath.back() != L'\\' && recordPath.back() != L'/') recordPath += L'\\';
    recordPath += L"worker-install-launch.record";
    const std::wstring record = std::wstring(verb) + L"\r\n" + file + L"\r\n<null>\r\n" + directory +
                                L"\r\n" + std::to_wstring(showCommand);
    HANDLE output = CreateFileW(recordPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return nullptr;
    const DWORD byteCount = static_cast<DWORD>(record.size() * sizeof(wchar_t));
    DWORD written = 0;
    const BOOL ok = WriteFile(output, record.data(), byteCount, &written, nullptr);
    CloseHandle(output);
    if (!ok || written != byteCount) return nullptr;
    return reinterpret_cast<HINSTANCE>(static_cast<INT_PTR>(33));
}

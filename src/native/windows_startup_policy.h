#pragma once

namespace MayaWindowsStartup
{
// Keep Maya Manager's sign-in entry separate from Win-Control's existing Run value.
inline constexpr wchar_t ManagerStartupRunValue[] = L"MayaXSWinManager";

inline bool AutoStartRequiresManager(bool managerAtSignIn, bool startMayaAtSignIn)
{
    return managerAtSignIn && startMayaAtSignIn;
}

inline bool ShouldStartMaya(bool launchedAtSignIn, bool startMayaAtSignIn)
{
    return launchedAtSignIn && startMayaAtSignIn;
}
}

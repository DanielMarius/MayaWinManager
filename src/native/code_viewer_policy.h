#pragma once

#include <windows.h>

inline DWORD MayaCodeViewerEditStyle(bool wrapped)
{
    const DWORD base = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY;
    return wrapped ? base : base | WS_HSCROLL | ES_AUTOHSCROLL;
}

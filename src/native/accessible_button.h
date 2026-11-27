#pragma once

#include <windows.h>
#include <commctrl.h>
#include <oleacc.h>
#include <UIAutomation.h>
#include <atomic>
#include <new>
#include <string>

#pragma comment(lib, "UIAutomationCore.lib")

class MayaAccessibleButtonProvider final : public IRawElementProviderSimple, public IInvokeProvider
{
public:
    explicit MayaAccessibleButtonProvider(HWND hwnd) noexcept : hwnd_(hwnd) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override
    {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, __uuidof(IRawElementProviderSimple)))
            *object = static_cast<IRawElementProviderSimple *>(this);
        else if (IsEqualIID(iid, __uuidof(IInvokeProvider)))
            *object = static_cast<IInvokeProvider *>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }

    ULONG STDMETHODCALLTYPE AddRef() override
    {
        return references_.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = references_.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (!remaining) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE get_ProviderOptions(ProviderOptions *options) override
    {
        if (!options) return E_POINTER;
        *options = ProviderOptions_ServerSideProvider;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPatternProvider(PATTERNID patternId, IUnknown **provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        if (patternId != UIA_InvokePatternId) return S_OK;
        *provider = static_cast<IInvokeProvider *>(this);
        AddRef();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPropertyValue(PROPERTYID propertyId, VARIANT *value) override
    {
        if (!value) return E_POINTER;
        VariantInit(value);
        if (!IsWindow(hwnd_)) return UIA_E_ELEMENTNOTAVAILABLE;
        switch (propertyId)
        {
        case UIA_ControlTypePropertyId:
            value->vt = VT_I4;
            value->lVal = UIA_ButtonControlTypeId;
            break;
        case UIA_IsControlElementPropertyId:
        case UIA_IsContentElementPropertyId:
            value->vt = VT_BOOL;
            value->boolVal = VARIANT_TRUE;
            break;
        case UIA_IsEnabledPropertyId:
            value->vt = VT_BOOL;
            value->boolVal = IsWindowEnabled(hwnd_) ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        case UIA_IsKeyboardFocusablePropertyId:
            value->vt = VT_BOOL;
            value->boolVal = (GetWindowLongPtrW(hwnd_, GWL_STYLE) & WS_TABSTOP) ? VARIANT_TRUE : VARIANT_FALSE;
            break;
        case UIA_NamePropertyId:
        {
            std::wstring name((size_t)GetWindowTextLengthW(hwnd_) + 1, L'\0');
            const int length = GetWindowTextW(hwnd_, name.data(), (int)name.size());
            BSTR result = SysAllocStringLen(name.data(), (UINT)length);
            if (!result) return E_OUTOFMEMORY;
            value->vt = VT_BSTR;
            value->bstrVal = result;
            break;
        }
        default:
            break;
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE get_HostRawElementProvider(IRawElementProviderSimple **provider) override
    {
        if (!provider) return E_POINTER;
        *provider = nullptr;
        if (!IsWindow(hwnd_)) return UIA_E_ELEMENTNOTAVAILABLE;
        return UiaHostProviderFromHwnd(hwnd_, provider);
    }

    HRESULT STDMETHODCALLTYPE Invoke() override
    {
        if (!IsWindow(hwnd_) || !IsWindowVisible(hwnd_)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!IsWindowEnabled(hwnd_)) return UIA_E_ELEMENTNOTENABLED;
        SendMessageW(hwnd_, BM_CLICK, 0, 0);
        return S_OK;
    }

private:
    std::atomic<ULONG> references_{1};
    HWND hwnd_;
};

inline LRESULT CALLBACK MayaAccessibleButtonProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam,
                                                  UINT_PTR id, DWORD_PTR referenceData)
{
    auto *provider = reinterpret_cast<MayaAccessibleButtonProvider *>(referenceData);
    if (message == WM_GETOBJECT)
    {
        const DWORD objectId = static_cast<DWORD>(lParam);
        if (objectId == static_cast<DWORD>(UiaRootObjectId) && provider)
            return UiaReturnRawElementProvider(hwnd, wParam, lParam, provider);
        if (objectId == static_cast<DWORD>(OBJID_CLIENT))
        {
            IAccessible *accessible = nullptr;
            if (SUCCEEDED(CreateStdAccessibleObject(hwnd, OBJID_CLIENT, IID_IAccessible,
                                                   reinterpret_cast<void **>(&accessible))) && accessible)
            {
                const LRESULT result = LresultFromObject(IID_IAccessible, wParam, accessible);
                accessible->Release();
                if (result) return result;
            }
        }
    }
    if (message == WM_DESTROY) UiaReturnRawElementProvider(hwnd, 0, 0, nullptr);
    if (message == WM_NCDESTROY)
    {
        const LRESULT result = DefSubclassProc(hwnd, message, wParam, lParam);
        RemoveWindowSubclass(hwnd, MayaAccessibleButtonProc, id);
        if (provider) provider->Release();
        return result;
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

inline BOOL AttachMayaAccessibleButton(HWND hwnd)
{
    auto *provider = new (std::nothrow) MayaAccessibleButtonProvider(hwnd);
    if (!provider) return FALSE;
    if (!SetWindowSubclass(hwnd, MayaAccessibleButtonProc, 1, reinterpret_cast<DWORD_PTR>(provider)))
    {
        provider->Release();
        return FALSE;
    }
    return TRUE;
}

inline bool MayaShouldDispatchButtonCommand(UINT notification, HWND control) noexcept
{
    if (!control || notification == BN_CLICKED) return true;
    wchar_t className[7]{};
    return GetClassNameW(control, className, 7) != 6 || lstrcmpW(className, L"Button") != 0;
}

/*
Copyright 2016 Freddy Herzog

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#define INITGUID
#include <Windows.h>
#include <wincodec.h>
#include <shlobj.h> // SHChangeNotify

#include "util.h"
#include "plugin_guids.h"
#include "flifBitmapDecoder.h"
#include "flifPropertyHandler.h"
#include "flifPreviewHandler.h"
#include "ClassFactory.h"

/*!
* Global ref count of all object instances.
*/
ULONG g_dll_object_ref_count = 0;

/*!
* Call this function in the constructor of any class that may leave the library boundaries.
* @see DllCanUnloadNow
*/
void DllAddRef()
{
    InterlockedIncrement(&g_dll_object_ref_count);
}

/*!
* Call this function in the destructor of any class that may leave the library boundaries.
* @see DllCanUnloadNow
*/
void DllRelease()
{
    InterlockedDecrement(&g_dll_object_ref_count);
}

HINSTANCE g_module_handle = 0;

std::wstring getThisLibraryPath()
{
    std::vector<WCHAR> buffer;
    buffer.resize(256);

    for(DWORD n = 256; n <= 4096; n *= 2)
    {
        buffer.resize(n);
        DWORD ret = GetModuleFileNameW(g_module_handle, buffer.data(), n);
        if(ret == 0)
            throw std::bad_alloc();
        if(ret < n)
            return buffer.data();
    }

    throw std::bad_alloc();
}

HINSTANCE getInstanceHandle()
{
    return g_module_handle;
}

std::wstring to_wstring(const GUID& guid)
{
    OLECHAR buffer[256];
    if(StringFromGUID2(guid, buffer, 256) == 0)
        throw std::bad_alloc();

    return std::wstring(buffer);
}

void clearThumbnailCache()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}

STDAPI DllRegisterServer()
{
    // NOTE: This function is unnecessary for the installed version,
    // because the installer can write registry entries. So this is duplicate code.
    // Still, during development it helps to just call regsvr32 on the DLL
    
    CUSTOM_TRY

        RegistryManager reg;
        flifBitmapDecoder::registerClass(reg);
        flifPropertyHandler::registerClass(reg);
        flifPreviewHandler::registerClass(reg);

        if(!reg.getErrors().empty())
        {
            reg.showErrors();
            return reg.getErrors().front()._error_code;
        }

        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

STDAPI DllUnregisterServer()
{
    CUSTOM_TRY

        RegistryManager reg;
        flifBitmapDecoder::unregisterClass(reg);
        flifPropertyHandler::unregisterClass(reg);
        flifPreviewHandler::unregisterClass(reg);

        if(!reg.getErrors().empty())
        {
            reg.showErrors();
            return reg.getErrors().front()._error_code;
        }

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, LPVOID *ppv)
{
    CUSTOM_TRY

        if (ppv == 0)
            return E_INVALIDARG;

        if(IsEqualGUID(clsid, CLSID_flifBitmapDecoder))
        {
            ComPtr<ClassFactory<flifBitmapDecoder>> cf(new ClassFactory<flifBitmapDecoder>());
            return cf->QueryInterface(iid, ppv);
        }
        if(IsEqualGUID(clsid, CLSID_flifPropertyHandler))
        {
            ComPtr<ClassFactory<flifPropertyHandler>> cf(new ClassFactory<flifPropertyHandler>());
            return cf->QueryInterface(iid, ppv);
        }
        if (IsEqualGUID(clsid, CLSID_flifPreviewHandler))
        {
            ComPtr<ClassFactory<flifPreviewHandler>> cf(new ClassFactory<flifPreviewHandler>());
            return cf->QueryInterface(iid, ppv);
        }

        return CLASS_E_CLASSNOTAVAILABLE;

    CUSTOM_CATCH_RETURN_HRESULT
}

STDAPI DllCanUnloadNow()
{
    CUSTOM_TRY

        if(g_dll_object_ref_count == 0)
            return S_OK;
        else
            return S_FALSE;

    CUSTOM_CATCH_RETURN_HRESULT
}

BOOL WINAPI DllMain(__in  HINSTANCE hinstDLL, __in  DWORD fdwReason, __in  LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        g_module_handle = hinstDLL;
    }
    return TRUE;
}
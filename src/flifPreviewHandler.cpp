/*
Copyright 2017 Freddy Herzog

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

#include "flifPreviewHandler.h"
#include "flifBitmapDecoder.h" // streamReadAll()
#include "plugin_guids.h"

const WCHAR PREVIEW_WINDOW_CLASSNAME[] = L"flifPreviewHandler";

LRESULT CALLBACK WND_PROC_FLIF_PREVIEW_HANDLER(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

//=============================================================================

flifPreviewHandler::flifPreviewHandler()
    : _parent_window(0)
    , _parent_window_rect{0, 0, 0, 0}
    , _registered_class(0)
    , _preview_window(0)
{
    DllAddRef();
}

flifPreviewHandler::~flifPreviewHandler()
{
    DllRelease();
}

STDMETHODIMP flifPreviewHandler::QueryInterface(REFIID iid, void** ppvObject)
{
    if (ppvObject == 0)
        return E_INVALIDARG;

    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IPreviewHandler))
        *ppvObject = static_cast<IPreviewHandler*>(this);
    else if (IsEqualGUID(iid, IID_IInitializeWithStream))
        *ppvObject = static_cast<IInitializeWithStream*>(this);
    else
    {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::SetWindow(HWND hwnd, const RECT* prc)
{
    if (!(hwnd && prc))
        return E_INVALIDARG;

    _parent_window = hwnd;
    _parent_window_rect = *prc;

    if (_preview_window)
    {
        // Update preview window parent and rect information
        SetParent(_preview_window, _parent_window);
        SetWindowPos(_preview_window, NULL,
            _parent_window_rect.left,
            _parent_window_rect.top,
            _parent_window_rect.right - _parent_window_rect.left,
            _parent_window_rect.bottom - _parent_window_rect.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::SetRect(const RECT* prc)
{
    if (!prc)
        return E_INVALIDARG;

    _parent_window_rect = *prc;

    if (_preview_window)
    {
        // Preview window is already created, so set its size and position.
        SetWindowPos(_preview_window, NULL,
            _parent_window_rect.left,
            _parent_window_rect.top,
            (_parent_window_rect.right - _parent_window_rect.left),
            (_parent_window_rect.bottom - _parent_window_rect.top),
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    return S_OK;
}



HRESULT STDMETHODCALLTYPE flifPreviewHandler::DoPreview()
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WND_PROC_FLIF_PREVIEW_HANDLER;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = getInstanceHandle();
    wcex.hIcon = LoadIcon(getInstanceHandle(), MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = PREVIEW_WINDOW_CLASSNAME;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

    _registered_class = RegisterClassExW(&wcex);
    if (!_registered_class)
    {
        DWORD last_error = GetLastError();
        MessageBox(0, ("RegisterClass: " + to_string(last_error)).data(), 0, 0);
        return HRESULT_FROM_WIN32(last_error);
    }

    _preview_window = CreateWindowW(PREVIEW_WINDOW_CLASSNAME,
        L"",
        WS_CHILD,
        _parent_window_rect.left,
        _parent_window_rect.top,
        _parent_window_rect.right - _parent_window_rect.left,
        _parent_window_rect.bottom - _parent_window_rect.top,
        _parent_window,
        0,
        getInstanceHandle(),
        0);

    if (_preview_window == 0)
    {
        DWORD last_error = GetLastError();
        MessageBox(0, ("CreateWindow: " + to_string(last_error)).data(), 0, 0);
        return HRESULT_FROM_WIN32(last_error);
    }

    HWND button = CreateWindowW(L"BUTTON",
        L"Preview",
        WS_CHILD | WS_VISIBLE,
        10, 10,
        200,
        50,
        _preview_window,
        0,
        getInstanceHandle(),
        0);

    if (!button)
    {
        DWORD last_error = GetLastError();
        MessageBox(0, ("CreateWindow: " + to_string(last_error)).data(), 0, 0);
        return HRESULT_FROM_WIN32(last_error);
    }

    ShowWindow(_preview_window, SW_SHOW);

    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::Unload()
{
    if (_preview_window)
    {
        DestroyWindow(_preview_window);
        _preview_window = 0;
    }

    if (_registered_class)
    {
        UnregisterClassW(PREVIEW_WINDOW_CLASSNAME, getInstanceHandle());
        _registered_class = 0;
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::SetFocus()
{
    if (_preview_window)
    {
        ::SetFocus(_preview_window);
        return S_OK;
    }
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::QueryFocus(HWND* phwnd)
{
    if (!phwnd)
        return E_INVALIDARG;

    *phwnd = GetFocus();
    if (*phwnd)
        return S_OK;
    else
        return HRESULT_FROM_WIN32(GetLastError());
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::TranslateAccelerator(MSG* pmsg)
{
    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::Initialize(IStream *pstream, DWORD grfMode)
{
    if (_preview_window)
        return E_FAIL;

    return S_OK;
}

void flifPreviewHandler::registerClass(RegistryManager& reg)
{
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPreviewHandler));
        reg.writeString(k, L"", L"FLIF Preview Handler");
        reg.writeString(k, L"AppID", to_wstring(APPID_flifPreviewHandler));
    }

    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPreviewHandler) + L"\\InProcServer32");
        reg.writeString(k, L"", getThisLibraryPath());
        reg.writeString(k, L"ThreadingModel", L"Apartment");
    }

    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"AppID\\" + to_wstring(APPID_flifPreviewHandler));
        reg.writeExpandableString(k, L"DllSurrogate", L"%SystemRoot%\\system32\\prevhost.exe");
    }

    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}");
        reg.writeString(k, L"", to_wstring(CLSID_flifPreviewHandler));
    }

    {
        auto k = reg.key(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers");
        reg.writeString(k, to_wstring(CLSID_flifPreviewHandler), L"flifPreviewHandler");
    }
}

void flifPreviewHandler::unregisterClass(RegistryManager& reg)
{
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPreviewHandler)));
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L"AppID\\" + to_wstring(APPID_flifPreviewHandler)));

    // already unregistered by flifBitmapDecoder
    //reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L".flif\\shellex\\{8895b1c6-b41f-4c1c-a562-0d564250836f}"));
}
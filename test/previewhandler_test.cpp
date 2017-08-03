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

#define INITGUID

#include <Windows.h>
#include <comdef.h>
#include <ShObjIdl.h>
#include <Shlwapi.h>

#include "util.h"
#include "plugin_guids.h"

inline std::string formatHRESULT(HRESULT hr)
{
    _com_error e(hr);
    return e.ErrorMessage();
}

#define CHECK_HR(hr) \
    if(FAILED((hr))) { MessageBox(0, (std::string(__FILE__) + "(" + std::to_string(__LINE__) + "): " + formatHRESULT((hr))).data(), 0, 0); return 1; }

const WCHAR FLIF_PREVIEW_HANDLER_APP_CLASSNAME[] = L"flifPreviewHandler.app";

class AppData
{
public:
    AppData()
    {
        _instance = this;
    }

    ~AppData()
    {
        _instance = 0;
    }

    static AppData* getInstance()
    {
        return _instance;
    }

    ComPtr<IPreviewHandler> preview_handler;

private:
    static AppData* _instance;
};

AppData* AppData::_instance = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (AppData::getInstance()->preview_handler.get())
        {
            RECT r;
            GetClientRect(hWnd, &r);
            AppData::getInstance()->preview_handler->SetRect(&r);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int argc;
    LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (argc <= 1)
        return 1;

    std::wstring filename = args[1];

    AppData app;

    // create main window

    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = FLIF_PREVIEW_HANDLER_APP_CLASSNAME;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

    if (!RegisterClassExW(&wcex))
    {
        DWORD last_error = GetLastError();
        MessageBox(0, ("RegisterClass: " + std::to_string(last_error)).data(), 0, 0);
        return last_error;
    }

    HWND main = CreateWindowW(
        FLIF_PREVIEW_HANDLER_APP_CLASSNAME,
        L"flifPreviewHandler: Test",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        500, 500,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!main)
    {
        DWORD last_error = GetLastError();
        MessageBox(0, ("CreateWindow: " + std::to_string(last_error)).data(), 0, 0);
        return last_error;
    }

    ShowWindow(main, nCmdShow);
    UpdateWindow(main);

    RECT r;
    GetClientRect(main, &r);

    // create preview handler on the main window

    auto plugin = LoadLibraryW(L"flif_windows_plugin.dll");
    if (!plugin)
        return E_FAIL;

    typedef HRESULT(STDAPICALLTYPE *fpDllGetClassObject)(REFCLSID, REFIID, LPVOID);
    auto get_class_object = reinterpret_cast<fpDllGetClassObject>(GetProcAddress(plugin, "DllGetClassObject"));

    ComPtr<IClassFactory> class_factory_preview;
    HRESULT hr = get_class_object(CLSID_flifPreviewHandler, IID_IClassFactory, class_factory_preview.ptrptr());
    CHECK_HR(hr);

    hr = class_factory_preview->CreateInstance(0, IID_IPreviewHandler, (void**)app.preview_handler.ptrptr());
    CHECK_HR(hr);

    ComPtr<IInitializeWithStream> init_with_stream;
    hr = app.preview_handler->QueryInterface(IID_IInitializeWithStream, (void**)init_with_stream.ptrptr());
    CHECK_HR(hr);

    ComPtr<IStream> stream;
    hr = SHCreateStreamOnFileW(filename.data(), STGM_READ, stream.ptrptr());
    CHECK_HR(hr);

    hr = init_with_stream->Initialize(stream.get(), STGM_READ);
    CHECK_HR(hr);

    hr = app.preview_handler->SetWindow(main, &r);
    CHECK_HR(hr);

    hr = app.preview_handler->DoPreview();
    CHECK_HR(hr);

    // run message loop

    MSG msg;
    while (GetMessage(&msg, 0, 0, 0))
    {
        // for WS_TABSTOP
        if (!IsDialogMessage(main, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    LocalFree(args);

    return msg.wParam;
}
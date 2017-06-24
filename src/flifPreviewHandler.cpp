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

/**
* Boilerplate code for reacting to WM_HSCROLL/WM_VSCROLL events.
* @return Updated scrollbar position
*/
int updateScrollBar(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    int scrollCode = LOWORD(wp);
    int scrollPos = HIWORD(wp);
    HWND scrollBar = reinterpret_cast<HWND>(lp);

    HWND scrollBarHandle;
    int scrollBarType;
    if (scrollBar)
    {
        scrollBarHandle = scrollBar;
        scrollBarType = SB_CTL;
    }
    else
    {
        scrollBarHandle = hwnd;
        scrollBarType = message == WM_HSCROLL ? SB_HORZ : SB_VERT;
    }

    SCROLLINFO si;
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    GetScrollInfo(scrollBarHandle, scrollBarType, &si);

    switch (scrollCode) {
    case SB_LINELEFT:
        //case SB_LINEUP:
        si.nPos -= 1;
        break;
    case SB_LINERIGHT:
        //case SB_LINEDOWN:
        si.nPos += 1;
        break;
    case SB_PAGELEFT:
        //case SB_PAGEUP:
        if (si.nPage == 0)
            si.nPos -= 1;
        else
            si.nPos -= si.nPage;
        break;
    case SB_PAGERIGHT:
        //case SB_PAGEDOWN:
        if (si.nPage == 0)
            si.nPos += 1;
        else
            si.nPos += si.nPage;
        break;
    case SB_LEFT:
        //case SB_TOP:
        si.nPos = si.nMin;
        break;
    case SB_RIGHT:
        //case SB_BOTTOM:
        si.nPos = si.nMax;
        break;
    case SB_THUMBTRACK:
        si.nPos = scrollPos;
        break;
    case SB_THUMBPOSITION:
        si.nPos = scrollPos;
        break;
    default:
        break;
    }

    SetScrollPos(scrollBarHandle, scrollBarType, si.nPos, TRUE);

    return si.nPos;
}

LRESULT CALLBACK WND_PROC_FLIF_PREVIEW_HANDLER(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            flifPreviewHandler* handler = reinterpret_cast<flifPreviewHandler*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            handler->togglePlayState();
        }
        break;
    case WM_TIMER:
        if(wParam = 1)
        {
            flifPreviewHandler* handler = reinterpret_cast<flifPreviewHandler*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            handler->showNextFrame();
        }
        break;
    case WM_HSCROLL:
        {
            int scroll_pos = updateScrollBar(hWnd, message, wParam, lParam);

            flifPreviewHandler* handler = reinterpret_cast<flifPreviewHandler*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
            handler->showFrameFromScrollBar(scroll_pos);
        }
        break;
    case WM_CTLCOLORSTATIC:
        // no background color to avoid flickering during animation
        return reinterpret_cast<LRESULT>(GetStockObject(NULL_BRUSH));
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

//=============================================================================

flifPreviewHandler::flifPreviewHandler()
    : _parent_window(0)
    , _parent_window_rect{0, 0, 0, 0}
    , _registered_class(0)
    , _preview_window(0)
    , _image_window(0)
    , _play_button(0)
    , _frame_scrollbar(0)
    , _playing(false)
    , _frame_width(0)
    , _frame_height(0)
    , _current_frame(0)
{
    DllAddRef();
}

flifPreviewHandler::~flifPreviewHandler()
{
    destroyPreviewWindowData();
    DllRelease();
}

void flifPreviewHandler::destroyPreviewWindowData()
{
    if (_preview_window)
    {
        DestroyWindow(_preview_window);
        _preview_window = 0;
        _image_window = 0;
        _play_button = 0;
        _frame_scrollbar = 0;
    }

    if (_registered_class)
    {
        UnregisterClassW(PREVIEW_WINDOW_CLASSNAME, getInstanceHandle());
        _registered_class = 0;
    }

    _playing = false;
    _frame_width = 0;
    _frame_height = 0;
    _frame_bitmaps.clear();
    _current_frame = 0;
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
    CUSTOM_TRY
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
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::SetRect(const RECT* prc)
{
    CUSTOM_TRY
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

            updateLayout();
        }

        return S_OK;
    CUSTOM_CATCH_RETURN_HRESULT
}

HBITMAP createDibSectionFromFlifImage(FLIF_IMAGE* image)
{
    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);

    std::vector<uint8_t> line;
    line.resize(w * 4);

    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -static_cast<int>(h);
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biXPelsPerMeter = 0x0ec4; // this is a standard value
    bmi.bmiHeader.biYPelsPerMeter = 0x0ec4;

    // Create DIB
    HDC local_dc = GetDC(0);
    if (local_dc)
    {
        HBITMAP result = CreateDIBSection(local_dc, &bmi, DIB_RGB_COLORS, nullptr, NULL, 0);
        ReleaseDC(0, local_dc);

        if (result)
        {
            BITMAP bitmap_data;
            if (GetObject(result, sizeof(bitmap_data), &bitmap_data))
            {
                uint8_t* bits_start = reinterpret_cast<uint8_t*>(bitmap_data.bmBits);

                for (uint32_t y = 0; y < h; ++y)
                {
                    flif_image_read_row_RGBA8(image, y, line.data(), line.size());

                    uint8_t* line_start = bits_start + y * bitmap_data.bmWidthBytes;

                    for (uint32_t x = 0; x < w; ++x)
                    {
                        line_start[x * 3 + 2] = line[x*4];
                        line_start[x * 3 + 1] = line[x * 4 + 1];
                        line_start[x * 3] = line[x * 4 + 2];
                    }
                }
            }
        }

        return result;
    }

    return 0;
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::DoPreview()
{
    CUSTOM_TRY
        if (_preview_window)
            return E_FAIL; // called twice

        // deletes the incomplete preview window data if anything fails in this function (also in case of exceptions)
        PreviewWindowDataDeleter deleter(*this);

        vector<BYTE> bytes;
        HRESULT hr = flifBitmapDecoder::streamReadAll(_stream.get(), bytes);
        if (FAILED(hr))
            return hr;

        flifDecoder decoder;
        if (!decoder)
            return E_FAIL;

        if (!flif_decoder_decode_memory(decoder, bytes.data(), bytes.size()))
            return E_FAIL;

        for (size_t i = 0; i < flif_decoder_num_images(decoder); ++i)
        {
            FLIF_IMAGE* image = flif_decoder_get_image(decoder, i);
            if (!image)
                return E_FAIL;

            _frame_width = flif_image_get_width(image);
            _frame_height = flif_image_get_height(image);

            HBITMAP bitmap = createDibSectionFromFlifImage(image);
            _frame_bitmaps.push_back(bitmap);
        }

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

        SetWindowLongPtrW(_preview_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        _image_window = CreateWindowExW(WS_EX_CLIENTEDGE,
            L"STATIC",
            L"",
            WS_CHILD | WS_VISIBLE | SS_BITMAP | SS_REALSIZECONTROL,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            _preview_window,
            0,
            getInstanceHandle(),
            0);

        if (!_image_window)
        {
            DWORD last_error = GetLastError();
            MessageBox(0, ("CreateWindow: " + to_string(last_error)).data(), 0, 0);
            return HRESULT_FROM_WIN32(last_error);
        }

        if (_frame_bitmaps.size() > 1)
        {
            _play_button = CreateWindowW(L"BUTTON",
                L"Play",
                WS_CHILD | WS_VISIBLE,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                _preview_window,
                0,
                getInstanceHandle(),
                0);

            if (!_play_button)
            {
                DWORD last_error = GetLastError();
                MessageBox(0, ("CreateWindow: " + to_string(last_error)).data(), 0, 0);
                return HRESULT_FROM_WIN32(last_error);
            }

            _frame_scrollbar = CreateWindowW(L"SCROLLBAR",
                L"Play",
                WS_CHILD | WS_VISIBLE | SBS_HORZ,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                _preview_window,
                0,
                getInstanceHandle(),
                0);

            if (!_frame_scrollbar)
            {
                DWORD last_error = GetLastError();
                MessageBox(0, ("CreateWindow: " + to_string(last_error)).data(), 0, 0);
                return HRESULT_FROM_WIN32(last_error);
            }

            SetScrollRange(_frame_scrollbar, SB_CTL, 0, _frame_bitmaps.size(), FALSE /*redraw*/);
        }

        updateLayout();

        if (!_frame_bitmaps.empty())
            SendMessage(_image_window, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(_frame_bitmaps[0]._dib_section));

        ShowWindow(_preview_window, SW_SHOW);

        // everything successful, disarm deleter
        deleter.should_delete = false;

        // not needed anymore
        _stream.reset(0);

        return S_OK;
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::Unload()
{
    CUSTOM_TRY
        destroyPreviewWindowData();

        // throw away initialization parameters, too
        _parent_window = 0;
        _parent_window_rect = { 0, 0, 0, 0 };
        _stream.reset(0);

        return S_OK;
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::SetFocus()
{
    CUSTOM_TRY
        if (_preview_window)
        {
            ::SetFocus(_preview_window);
            return S_OK;
        }
        return S_FALSE;
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::QueryFocus(HWND* phwnd)
{
    CUSTOM_TRY
        if (!phwnd)
            return E_INVALIDARG;

        *phwnd = GetFocus();
        if (*phwnd)
            return S_OK;
        else
            return HRESULT_FROM_WIN32(GetLastError());
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::TranslateAccelerator(MSG* pmsg)
{
    CUSTOM_TRY
        return S_OK;
    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::Initialize(IStream *pstream, DWORD grfMode)
{
    CUSTOM_TRY
        if (_preview_window)
            return E_FAIL; // already initialized

        _stream.reset(pstream);
        return S_OK;
    CUSTOM_CATCH_RETURN_HRESULT
}

void flifPreviewHandler::togglePlayState()
{
    _playing = !_playing;
    if (_playing)
    {
        // TODO: use something better than timers (smooth animation)
        SetTimer(_preview_window, 1, 50, nullptr);
        SetWindowTextW(_play_button, L"Pause");
    }
    else
    {
        KillTimer(_preview_window, 1);
        SetWindowTextW(_play_button, L"Play");
    }
}

void flifPreviewHandler::showNextFrame()
{
    // TODO: calculate elapsed time and choose the right image

    if (!_frame_bitmaps.empty())
    {
        _current_frame = (_current_frame + 1) % _frame_bitmaps.size();
        SendMessage(_image_window, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(_frame_bitmaps[_current_frame]._dib_section));
        SetScrollPos(_frame_scrollbar, SB_CTL, _current_frame, TRUE /*redraw*/);
    }
}

void flifPreviewHandler::showFrameFromScrollBar(size_t frame)
{
    _playing = false;
    KillTimer(_preview_window, 1);
    SetWindowTextW(_play_button, L"Play");

    if (frame < _frame_bitmaps.size())
    {
        _current_frame = frame;
        SendMessage(_image_window, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(_frame_bitmaps[_current_frame]._dib_section));
    }
}

void flifPreviewHandler::updateLayout()
{
    const int total_w = _parent_window_rect.right - _parent_window_rect.left;
    const int total_h = _parent_window_rect.bottom - _parent_window_rect.top;

    const double total_aspect = double(total_w) / double(total_h);
    const double image_aspect = double(_frame_width) / double(_frame_height);

    const int control_height = 25;
    const int spacing = 10;

    int max_image_width = total_w - spacing * 2;
    int max_image_height = total_h - spacing * 2;

    if (_play_button)
        max_image_height -= control_height + spacing;

    // fit to image width
    int image_width = max_image_width;
    int image_height = int(double(image_width) / image_aspect);

    if (image_height > max_image_height)
    {
        // image to thin
        // fit to image height instead
        image_height = max_image_height;
        image_width = int(double(image_height) * image_aspect);
    }

    int image_x = (total_w - image_width) / 2;
    int image_y = _play_button ?
        (total_h - (image_height + control_height + spacing)) / 2 :
        (total_h - image_height) / 2;

    int control_y = image_y + spacing + image_height;

    SetWindowPos(_image_window, NULL,
        image_x,
        image_y,
        image_width,
        image_height,
        SWP_NOZORDER | SWP_NOACTIVATE);

    if (_play_button)
    {
        SetWindowPos(_play_button, NULL,
            image_x,
            control_y,
            100,
            control_height,
            SWP_NOZORDER | SWP_NOACTIVATE);

        SetWindowPos(_frame_scrollbar, NULL,
            image_x + 100 + spacing,
            control_y,
            image_width - 100 - spacing,
            control_height,
            SWP_NOZORDER | SWP_NOACTIVATE);
    }
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
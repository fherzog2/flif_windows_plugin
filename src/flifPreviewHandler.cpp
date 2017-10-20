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
#include <algorithm>
#include <limits>

const WCHAR PREVIEW_WINDOW_CLASSNAME[] = L"flifPreviewHandler";

/**
* Boilerplate code for reacting to WM_HSCROLL/WM_VSCROLL events.
* @return Updated scrollbar position
*/
static int updateScrollBar(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
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

static LRESULT CALLBACK WND_PROC_FLIF_PREVIEW_HANDLER(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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
        if(wParam == 1)
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
    , _frame_width(0)
    , _frame_height(0)
    , _num_loops(0)
    , _loop_time(0)
    , _play_state(PS_STOP)
    , _elapsed_millis_until_pause(0)
    , _current_frame(-1)
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

    _frame_width = 0;
    _frame_height = 0;
    _num_loops = 0;
    _frame_bitmaps.clear();
    _frame_delays.clear();
    _loop_time = std::chrono::milliseconds(0);

    _play_state = PS_STOP;
    _play_start_time = std::chrono::time_point<std::chrono::high_resolution_clock>();
    _current_frame = -1;
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

static HBITMAP createDibSectionFromFlifImage(FLIF_IMAGE* image)
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
                        uint8_t red   = line[x * 4];
                        uint8_t green = line[x * 4 + 1];
                        uint8_t blue  = line[x * 4 + 2];
                        uint8_t alpha = line[x * 4 + 3];

                        uint8_t bg_red   = 255;
                        uint8_t bg_green = 255;
                        uint8_t bg_blue  = 255;

                        // Currently the widget which displays the image doesn't support alpha.
                        // So blend the image with a background color.
                        // Otherwise the color values at translucent pixels will be random.

                        uint16_t red_blended   = red   * alpha + bg_red   * (255 - alpha);
                        uint16_t green_blended = green * alpha + bg_green * (255 - alpha);
                        uint16_t blue_blended  = blue  * alpha + bg_blue  * (255 - alpha);

                        line_start[x * 3 + 2] = red_blended / 256;
                        line_start[x * 3 + 1] = green_blended / 256;
                        line_start[x * 3]     = blue_blended / 256;
                    }
                }
            }
        }

        return result;
    }

    return 0;
}

/**
* Creates an icon programmatically.
*
* @param drawfunc Function object which takes a HDC as parameter.
*/
template<class DRAWFUNC>
HICON createIcon(int w, int h, DRAWFUNC drawfunc)
{
    win::ClientDC dc(NULL);
    if (dc)
    {
        win::Bitmap color = CreateCompatibleBitmap(dc, w, h);
        win::Bitmap mask = CreateBitmap(w, h, 1, 1, nullptr);

        if (color && mask)
        {
            win::MemoryDC mem_dc(dc);
            if (mem_dc)
            {
                win::Pen black_pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
                win::Brush black_brush = CreateSolidBrush(RGB(0, 0, 0));
                win::Brush white_brush = CreateSolidBrush(RGB(255, 255, 255));

                if (black_pen && black_brush && white_brush)
                {
                    {
                        win::SaveAndRestoreDC mem_dc_restore(mem_dc);

                        SelectObject(mem_dc, color);

                        RECT r = { 0, 0, w, h };
                        FillRect(mem_dc, &r, black_brush);

                        SelectObject(mem_dc, mask);

                        FillRect(mem_dc, &r, white_brush);

                        SelectObject(mem_dc, black_pen);
                        SelectObject(mem_dc, black_brush);

                        drawfunc(mem_dc);
                    }

                    ICONINFO ii;
                    ii.fIcon = TRUE;
                    ii.hbmMask = mask;
                    ii.hbmColor = color;
                    return CreateIconIndirect(&ii);
                }
            }
        }
    }

    return NULL;
}

static HICON createPlayIcon(int w, int h)
{
    return createIcon(w, h, [=](HDC dc) {
        POINT points[] = {
            { 0, 0 },
            { 0, h },
            { w, h / 2 }
        };
        Polygon(dc, points, 3);
    });
}

static HICON createPauseIcon(int w, int h)
{
    return createIcon(w, h, [=](HDC dc) {
        Rectangle(dc, w / 8, 0, w / 8 + w / 4, h);
        Rectangle(dc, w / 8 + w / 2, 0, w / 8 + w / 2 + w / 4, h);
    });
}

HRESULT STDMETHODCALLTYPE flifPreviewHandler::DoPreview()
{
    CUSTOM_TRY
        if (_preview_window)
            return E_FAIL; // called twice

        // deletes the incomplete preview window data if anything fails in this function (also in case of exceptions)
        PreviewWindowDataDeleter deleter(*this);

        std::vector<BYTE> bytes;
        HRESULT hr = flifBitmapDecoder::streamReadAll(_stream.get(), bytes);
        if (FAILED(hr))
            return hr;

        flifDecoder decoder;
        if (!decoder)
            return E_FAIL;

        if (!flif_decoder_decode_memory(decoder, bytes.data(), bytes.size()))
            return E_FAIL;

        _num_loops = flif_decoder_num_loops(decoder);

        if (flif_decoder_num_images(decoder) == 0)
            return E_FAIL;

        for (size_t i = 0, end = flif_decoder_num_images(decoder); i < end; ++i)
        {
            FLIF_IMAGE* image = flif_decoder_get_image(decoder, i);
            if (!image)
                return E_FAIL;

            _frame_width = flif_image_get_width(image);
            _frame_height = flif_image_get_height(image);
            _frame_delays.push_back(std::chrono::milliseconds(flif_image_get_frame_delay(image)));

            HBITMAP bitmap = createDibSectionFromFlifImage(image);
            _frame_bitmaps.push_back(bitmap);
        }

        _loop_time = std::chrono::milliseconds(0);
        for (auto delay : _frame_delays)
        {
            _loop_time += std::chrono::milliseconds(delay);
        }

        WNDCLASSEXW wcex;

        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WND_PROC_FLIF_PREVIEW_HANDLER;
        wcex.cbClsExtra = 0;
        wcex.cbWndExtra = 0;
        wcex.hInstance = getInstanceHandle();
        wcex.hIcon = LoadIcon(getInstanceHandle(), IDI_APPLICATION);
        wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszMenuName = NULL;
        wcex.lpszClassName = PREVIEW_WINDOW_CLASSNAME;
        wcex.hIconSm = LoadIcon(wcex.hInstance, IDI_APPLICATION);

        _registered_class = RegisterClassExW(&wcex);
        if (!_registered_class)
        {
            DWORD last_error = GetLastError();
            MessageBox(0, ("RegisterClass: " + std::to_string(last_error)).data(), 0, 0);
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
            MessageBox(0, ("CreateWindow: " + std::to_string(last_error)).data(), 0, 0);
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
            MessageBox(0, ("CreateWindow: " + std::to_string(last_error)).data(), 0, 0);
            return HRESULT_FROM_WIN32(last_error);
        }

        if (_frame_bitmaps.size() > 1)
        {
            _play_button = CreateWindowW(L"BUTTON",
                L"",
                WS_CHILD | WS_VISIBLE | BS_ICON,
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
                MessageBox(0, ("CreateWindow: " + std::to_string(last_error)).data(), 0, 0);
                return HRESULT_FROM_WIN32(last_error);
            }

            _play_icon = createPlayIcon(16, 16);
            _pause_icon = createPauseIcon(16, 16);

            if (!(_play_icon && _pause_icon))
            {
                DWORD last_error = GetLastError();
                MessageBox(0, ("Failed to create icons: " + std::to_string(last_error)).data(), 0, 0);
                return HRESULT_FROM_WIN32(last_error);
            }

            SendMessage(_play_button, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(_play_icon.get()));

            _frame_scrollbar = CreateWindowW(L"SCROLLBAR",
                L"",
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
                MessageBox(0, ("CreateWindow: " + std::to_string(last_error)).data(), 0, 0);
                return HRESULT_FROM_WIN32(last_error);
            }

            SetScrollRange(_frame_scrollbar, SB_CTL, 0, _frame_bitmaps.size(), FALSE /*redraw*/);
        }

        updateLayout();


        if (!_frame_bitmaps.empty())
            setCurrentFrame(0, true);

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

void flifPreviewHandler::setPlayState(PlayState state)
{
    if (_play_state == state)
        return;

    switch (state)
    {
    case PS_PLAY:
        _play_start_time = std::chrono::high_resolution_clock::now() - _elapsed_millis_until_pause;
        _elapsed_millis_until_pause = std::chrono::milliseconds(0);
        {
            // run the timer as fast as necessary, but keep in mind that it probably can't go faster than 50ms intervals

            std::chrono::milliseconds min_delay = _frame_delays.front();
            for (auto delay : _frame_delays)
                if (delay < min_delay)
                    min_delay = delay;

            // run the timer at half of the minimum frame delay (so no frame are missed)
            // clamp at UINT boundary (probably never happens, just to stay safe)
            UINT interval = static_cast<UINT>(std::min(min_delay.count() / 2,
                                                       static_cast<std::chrono::milliseconds::rep>(std::numeric_limits<UINT>::max())));
            interval = std::max(25u, interval);

            SetTimer(_preview_window, 1, interval, nullptr);
        }
        SendMessage(_play_button, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(_pause_icon.get()));
        break;
    case PS_PAUSE:
        KillTimer(_preview_window, 1);
        SendMessage(_play_button, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(_play_icon.get()));
        _elapsed_millis_until_pause = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _play_start_time);
        _play_start_time = std::chrono::time_point<std::chrono::high_resolution_clock>();
        break;
    case PS_STOP:
        KillTimer(_preview_window, 1);
        SendMessage(_play_button, BM_SETIMAGE, IMAGE_ICON, reinterpret_cast<LPARAM>(_play_icon.get()));
        _play_start_time = std::chrono::time_point<std::chrono::high_resolution_clock>();
        _elapsed_millis_until_pause = std::chrono::milliseconds(0);
        break;
    }

    _play_state = state;
}

void flifPreviewHandler::togglePlayState()
{
    setPlayState(_play_state != PS_PLAY ? PS_PLAY : PS_PAUSE);
}

void flifPreviewHandler::setCurrentFrame(size_t current_frame, bool update_scrollbar)
{
    if (_current_frame == current_frame)
        return;

    _current_frame = current_frame;

    SendMessage(_image_window, STM_SETIMAGE, IMAGE_BITMAP, reinterpret_cast<LPARAM>(_frame_bitmaps[_current_frame].get()));

    if(update_scrollbar)
        SetScrollPos(_frame_scrollbar, SB_CTL, _current_frame, TRUE /*redraw*/);
}

void flifPreviewHandler::showNextFrame()
{
    if (_play_state != PS_PLAY)
        return;

    if (!_frame_bitmaps.empty())
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed_millis = std::chrono::duration_cast<std::chrono::milliseconds>(now - _play_start_time);

        auto loops_done = elapsed_millis / _loop_time;

        if (_num_loops != 0 && loops_done >= _num_loops)
        {
            setPlayState(PS_STOP);
            setCurrentFrame(_frame_bitmaps.size() - 1, true);
        }
        else
        {
            elapsed_millis -= loops_done * _loop_time;

            std::chrono::milliseconds total_delay_of_previous_frame(0);
            for (auto it = _frame_delays.begin(), end = _frame_delays.end(); it != end; ++it)
            {
                if (elapsed_millis < *it + total_delay_of_previous_frame)
                {
                    setCurrentFrame(std::distance(_frame_delays.begin(), it), true);
                    break;
                }

                total_delay_of_previous_frame += *it;
            }
        }
    }
}

void flifPreviewHandler::showFrameFromScrollBar(size_t frame)
{
    if (frame < _frame_bitmaps.size())
    {
        setPlayState(PS_PAUSE);
        setCurrentFrame(frame, false);

        _elapsed_millis_until_pause = std::chrono::milliseconds(0);
        for (auto it = _frame_delays.begin(), end = it + _current_frame; it != end; ++it)
        {
            _elapsed_millis_until_pause += std::chrono::milliseconds(*it);
        }
    }
}

void flifPreviewHandler::updateLayout()
{
    const int total_w = _parent_window_rect.right - _parent_window_rect.left;
    const int total_h = _parent_window_rect.bottom - _parent_window_rect.top;

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
        const int button_width = 50;

        SetWindowPos(_play_button, NULL,
            image_x,
            control_y,
            button_width,
            control_height,
            SWP_NOZORDER | SWP_NOACTIVATE);

        SetWindowPos(_frame_scrollbar, NULL,
            image_x + button_width + spacing,
            control_y,
            image_width - button_width - spacing,
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
        auto k = reg.key(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers");
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
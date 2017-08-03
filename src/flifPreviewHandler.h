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

#pragma once

#include <Shobjidl.h>
#include <chrono>

#include "util.h"
#include "RegistryManager.h"

class DibSection
{
public:
    DibSection(HBITMAP dib_section = 0)
        : _dib_section(dib_section)
    {
    }

    ~DibSection()
    {
        if (_dib_section)
            DeleteObject(_dib_section);
    }

    DibSection(const DibSection& other) = delete;
    DibSection& operator=(const DibSection& other) = delete;

    DibSection(DibSection&& other)
        : _dib_section(other._dib_section)
    {
        other._dib_section = 0;
    }

    DibSection& operator=(DibSection&& other)
    {
        _dib_section = other._dib_section;
        other._dib_section = 0;
    }

    HBITMAP _dib_section;
};

class flifPreviewHandler : public IPreviewHandler, public IInitializeWithStream
{
public:
    flifPreviewHandler();
    ~flifPreviewHandler();

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override { return _ref_count.addRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() override { return _ref_count.releaseRef(this); }

    // IPreviewHandler methods
    virtual HRESULT STDMETHODCALLTYPE SetWindow(HWND hwnd, const RECT* prc) override;
    virtual HRESULT STDMETHODCALLTYPE SetRect(const RECT* prc) override;
    virtual HRESULT STDMETHODCALLTYPE DoPreview() override;
    virtual HRESULT STDMETHODCALLTYPE Unload() override;
    virtual HRESULT STDMETHODCALLTYPE SetFocus() override;
    virtual HRESULT STDMETHODCALLTYPE QueryFocus(HWND* phwnd) override;
    virtual HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG* pmsg) override;

    // IInitializeWithStream methods
    virtual HRESULT STDMETHODCALLTYPE Initialize(IStream *pstream, DWORD grfMode) override;

    enum PlayState
    {
        PS_PLAY,
        PS_PAUSE,
        PS_STOP
    };

    void setPlayState(PlayState state);
    void togglePlayState();
    void showNextFrame();
    void showFrameFromScrollBar(size_t frame);

    static void registerClass(RegistryManager& reg);
    static void unregisterClass(RegistryManager& reg);
private:
    void destroyPreviewWindowData();
    void updateLayout();
    void setCurrentFrame(size_t current_frame, bool update_scrollbar);

    ComRefCountImpl _ref_count;

    HWND _parent_window;
    RECT _parent_window_rect;

    ComPtr<IStream> _stream;

    // PREVIEW WINDOW DATA: only valid between DoPreview() and Unload()
    ATOM _registered_class;
    HWND _preview_window;
    HWND _image_window;    // owned by _preview_window
    HWND _play_button;     // owned by _preview_window
    HWND _frame_scrollbar; // owned by _preview_window

    int _frame_width;
    int _frame_height;
    int32_t _num_loops;
    vector<DibSection> _frame_bitmaps;
    vector<std::chrono::milliseconds> _frame_delays;
    std::chrono::milliseconds _loop_time;

    PlayState _play_state;
    std::chrono::time_point<std::chrono::high_resolution_clock> _play_start_time;
    std::chrono::milliseconds _elapsed_millis_until_pause; //<! remember the progress in case pause is called
    size_t _current_frame;
    // PREVIEW WINDOW DATA END

    /**
    * Cleanup helper used during DoPreview()
    */
    class PreviewWindowDataDeleter
    {
    public:
        PreviewWindowDataDeleter(flifPreviewHandler& parent)
            : _parent(parent)
            , should_delete(true)
        {
        }

        ~PreviewWindowDataDeleter()
        {
            if (should_delete)
                _parent.destroyPreviewWindowData();
        }

        flifPreviewHandler& _parent;
        bool should_delete;
    };
};
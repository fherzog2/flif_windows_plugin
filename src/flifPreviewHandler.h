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

#include "util.h"
#include "RegistryManager.h"

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

    static void registerClass(RegistryManager& reg);
    static void unregisterClass(RegistryManager& reg);
private:
    ComRefCountImpl _ref_count;

    HWND _parent_window;
    RECT _parent_window_rect;

    ATOM _registered_class;
    HWND _preview_window;
};
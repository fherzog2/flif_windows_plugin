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

#pragma once

#include <Propsys.h>

#include "util.h"
#include "RegistryManager.h"

class flifPropertyHandler : public IInitializeWithStream, public IPropertyStore
{
public:
    flifPropertyHandler();
    ~flifPropertyHandler();

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override { return _ref_count.addRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() override { return _ref_count.releaseRef(this); }

    // IPropertyStore methods
    virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD *cProps) override;
    virtual HRESULT STDMETHODCALLTYPE GetAt( DWORD iProp, PROPERTYKEY *pkey) override;
    virtual HRESULT STDMETHODCALLTYPE GetValue(REFPROPERTYKEY key, PROPVARIANT *pv) override;
    virtual HRESULT STDMETHODCALLTYPE SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar) override;
    virtual HRESULT STDMETHODCALLTYPE Commit(void) override;

    // IInitializeWithStream methods
    virtual HRESULT STDMETHODCALLTYPE Initialize(IStream *pstream, DWORD grfMode) override;

    static void registerClass(RegistryManager& reg);
    static void unregisterClass(RegistryManager& reg);

private:
    ComRefCountImpl _ref_count;

    CriticalSection _cs_init;
    bool _is_initialized;

    // properties
    uint32_t _width;
    uint32_t _height;
    uint8_t _bitdepth;
};
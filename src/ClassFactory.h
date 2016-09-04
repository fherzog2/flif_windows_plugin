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

#include <Unknwn.h>

#include "util.h"

/*!
* Generic class factory for all exported COM objects of this DLL
*/
template<class T>
class ClassFactory : public IClassFactory
{
public:
    ClassFactory()
    {
        DllAddRef();
    }

    ~ClassFactory()
    {
        DllRelease();
    }

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
    {
        if(ppvObject == 0)
            return E_INVALIDARG;

        if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IClassFactory))
            *ppvObject = static_cast<IClassFactory*>(this);
        else
        {
            *ppvObject = 0;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }
    virtual ULONG STDMETHODCALLTYPE AddRef() override { return _ref_count.addRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() override { return _ref_count.releaseRef(this); }

    // IClassFactory methods
    virtual HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID iid, void** ppvObject) override
    {
        CUSTOM_TRY

            if(outer != 0)
                return CLASS_E_NOAGGREGATION;

            if(ppvObject == 0)
                return E_INVALIDARG;

            ComPtr<T> thumbnail_provider(new T());
            if(thumbnail_provider.get() == 0)
                return E_OUTOFMEMORY;
        
        return thumbnail_provider->QueryInterface(iid, ppvObject);

        CUSTOM_CATCH_RETURN_HRESULT
    }
    virtual HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override
    {
        CUSTOM_TRY

            return E_NOTIMPL;

        CUSTOM_CATCH_RETURN_HRESULT
    }

    ComRefCountImpl _ref_count;
};
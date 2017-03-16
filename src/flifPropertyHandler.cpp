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

#include "flifPropertyHandler.h"
#include "plugin_guids.h"
#include "flifWrapper.h"

#include <Propkey.h>
#include <propvarutil.h>

flifPropertyHandler::flifPropertyHandler()
: _is_initialized(false)
, _width(0)
, _height(0)
, _bitdepth(0)
{
    DllAddRef();
}

flifPropertyHandler::~flifPropertyHandler()
{
    DllRelease();
}

STDMETHODIMP flifPropertyHandler::QueryInterface(REFIID iid, void** ppvObject)
{
    if(ppvObject == 0)
        return E_INVALIDARG;

    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IPropertyStore))
        *ppvObject = static_cast<IPropertyStore*>(this);
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

const PROPERTYKEY supported_flif_properties[] = {
    PKEY_Image_HorizontalSize,
    PKEY_Image_VerticalSize,
    PKEY_Image_Dimensions,
    PKEY_Image_BitDepth
};

HRESULT STDMETHODCALLTYPE flifPropertyHandler::GetCount(DWORD *cProps)
{
    CUSTOM_TRY

        if(_prop_cache.get() == nullptr)
            return E_ILLEGAL_METHOD_CALL;

        return _prop_cache->GetCount(cProps);

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::GetAt( DWORD iProp, PROPERTYKEY *pkey)
{
    CUSTOM_TRY

        if(_prop_cache.get() == nullptr)
            return E_ILLEGAL_METHOD_CALL;

        return _prop_cache->GetAt(iProp, pkey);

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT *pv)
{
    CUSTOM_TRY

        if(_prop_cache.get() == nullptr)
            return E_ILLEGAL_METHOD_CALL;

        return _prop_cache->GetValue(key, pv);

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::SetValue(REFPROPERTYKEY key, REFPROPVARIANT propvar)
{
    CUSTOM_TRY

        return STG_E_INVALIDPARAMETER;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::Commit(void)
{
    CUSTOM_TRY

        return STG_E_INVALIDPARAMETER;

    CUSTOM_CATCH_RETURN_HRESULT
}

/*!
* @param stream Input stream
* @param chunk_size The maximum amount of bytes that will be read from the stream
* @param buffer Keeps the read bytes for another run
* @param decoder Valid if the function returns S_OK
* @return S_OK if the decoder is ready.
*         S_FALSE if there is more data.
*         An error code if the reading or decoding failed.
*/
HRESULT readChunkAndTryDecoding(IStream *stream, size_t chunk_size, vector<BYTE>& buffer, flifDecoder& decoder)
{
    size_t previous_size = buffer.size();
    buffer.resize(buffer.size() + chunk_size);

    ULONG actually_read;
    HRESULT read_result = stream->Read(buffer.data() + previous_size, static_cast<ULONG>(chunk_size), &actually_read);
    if(FAILED(read_result))
        return read_result;

    bool end_of_stream = read_result == S_FALSE;

    buffer.resize(previous_size + actually_read);

    if (flif_decoder_decode_memory(decoder, buffer.data(), buffer.size()) != 0 &&
        flif_decoder_num_images(decoder) != 0)
    {
        return S_OK;
    }

    if(end_of_stream)
        return E_FAIL;
    else
        return S_FALSE;
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::Initialize(IStream *stream, DWORD grfMode)
{
    CUSTOM_TRY

        lock_guard<CriticalSection> lock(_cs_init);
        if(_is_initialized)
            return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

        HRESULT hr = PSCreateMemoryPropertyStore(IID_IPropertyStoreCache, (void**)_prop_cache.ptrptr());
        if(FAILED(hr))
            return hr;

        vector<BYTE> read_buffer;
        flifDecoder decoder;
        if(decoder == 0)
            return E_FAIL;

        while(true)
        {
            HRESULT try_decode_result = readChunkAndTryDecoding(stream, 2048, read_buffer, decoder);
            if(FAILED(try_decode_result))
                return try_decode_result;

            if(try_decode_result == S_FALSE)
                continue;

            FLIF_IMAGE* image = flif_decoder_get_image(decoder, 0);
            if(image == 0)
                return E_FAIL;

            _width = flif_image_get_width(image);
            _height = flif_image_get_height(image);
            _bitdepth = flif_image_get_depth(image) * flif_image_get_nb_channels(image);

            // fill prop cache

            PROPVARIANT value;

            HRESULT init_result = InitPropVariantFromInt32(_width, &value);
            if(SUCCEEDED(init_result))
            {
                _prop_cache->SetValueAndState(PKEY_Image_HorizontalSize, &value, PSC_NORMAL);
                PropVariantClear(&value);
            }

            init_result = InitPropVariantFromInt32(_height, &value);
            if(SUCCEEDED(init_result))
            {
                _prop_cache->SetValueAndState(PKEY_Image_VerticalSize, &value, PSC_NORMAL);
                PropVariantClear(&value);
            }

            init_result = InitPropVariantFromString((to_wstring(_width) + L" x " + to_wstring(_height)).data(), &value);
            if(SUCCEEDED(init_result))
            {
                _prop_cache->SetValueAndState(PKEY_Image_Dimensions, &value, PSC_NORMAL);
                PropVariantClear(&value);
            }

            init_result = InitPropVariantFromInt32(_bitdepth, &value);
            if(SUCCEEDED(init_result))
            {
                _prop_cache->SetValueAndState(PKEY_Image_BitDepth, &value, PSC_NORMAL);
                PropVariantClear(&value);
            }

            break;
        }

        _is_initialized = true;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

void flifPropertyHandler::registerClass(RegistryManager& reg)
{
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPropertyHandler));
        reg.writeString(k, L"", L"FLIF Property Handler");
        reg.writeDWORD(k, L"ManualSafeSave", 1);
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPropertyHandler) + L"\\InProcServer32");
        reg.writeString(k, L"", getThisLibraryPath());
        reg.writeString(k, L"ThreadingModel", L"Both");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"SystemFileAssociations\\.flif");
        reg.writeString(k, L"PreviewDetails", L"prop:System.DateModified;System.Image.Dimensions;System.Size;*System.OfflineAvailability;*System.OfflineStatus;System.DateCreated;*System.SharedWith");
        reg.writeString(k, L"FullDetails", L"prop:System.PropGroup.Image;System.Image.Dimensions;System.Image.HorizontalSize;System.Image.VerticalSize;System.Image.BitDepth;System.PropGroup.FileSystem;System.ItemNameDisplay;System.ItemType;System.ItemFolderPathDisplay;System.DateCreated;System.DateModified;System.Size;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;System.SharedWith;System.FileOwner;System.ComputerName");
        reg.writeString(k, L"InfoTip", L"prop:System.ItemType;System.Image.Dimensions;System.Size");
        reg.writeString(k, L"PreviewTitle", L"prop:System.Title;System.ItemType");
    }
    {
        auto k = reg.key(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertyHandlers\\.flif");
        reg.writeString(k, L"", to_wstring(CLSID_flifPropertyHandler));
    }
    {
        auto k = reg.key(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\KindMap");
        reg.writeString(k, L".flif", L"Picture");
    }
}

void flifPropertyHandler::unregisterClass(RegistryManager& reg)
{
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifPropertyHandler)));
}
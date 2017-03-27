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
#include "flifMetadataQueryReader.h"

#include <Propkey.h>
#include <propvarutil.h>

// only selected, predefined metadata entries are accessible through the property store

PROPERTYKEY SUPPORTED_METADATA_PROPERTIES[] = {
    PKEY_Photo_Aperture,
    PKEY_Photo_Brightness,
    PKEY_Photo_CameraManufacturer,
    PKEY_Photo_CameraModel,
    PKEY_Photo_CameraSerialNumber,
    PKEY_Photo_Contrast,
    PKEY_Photo_ContrastText,
    PKEY_Photo_DateTaken,
    PKEY_Photo_DigitalZoom,
    PKEY_Photo_Event,
    PKEY_Photo_EXIFVersion,
    PKEY_Photo_ExposureBias,
    PKEY_Photo_ExposureIndex,
    PKEY_Photo_ExposureProgram,
    PKEY_Photo_ExposureTime,
    PKEY_Photo_Flash,
    PKEY_Photo_FlashEnergy,
    PKEY_Photo_FlashManufacturer,
    PKEY_Photo_FlashModel,
    PKEY_Photo_FlashText,
    PKEY_Photo_FNumber,
    PKEY_Photo_FocalLength,
    PKEY_Photo_FocalLengthInFilm,
    PKEY_Photo_FocalPlaneXResolution,
    PKEY_Photo_FocalPlaneYResolution,
    PKEY_Photo_GainControl,
    PKEY_Photo_GainControlText,
    PKEY_Photo_ISOSpeed,
    PKEY_Photo_LensManufacturer,
    PKEY_Photo_LensModel,
    PKEY_Photo_LightSource,
    PKEY_Photo_MakerNote,
    PKEY_Photo_MakerNoteOffset,
    PKEY_Photo_MaxAperture,
    PKEY_Photo_MeteringMode,
    PKEY_Photo_MeteringModeText,
    PKEY_Photo_Orientation,
    PKEY_Photo_OrientationText,
    PKEY_Photo_PeopleNames,
    PKEY_Photo_PhotometricInterpretation,
    PKEY_Photo_PhotometricInterpretationText,
    PKEY_Photo_ProgramMode,
    PKEY_Photo_ProgramModeText,
    PKEY_Photo_RelatedSoundFile,
    PKEY_Photo_Saturation,
    PKEY_Photo_SaturationText,
    PKEY_Photo_Sharpness,
    PKEY_Photo_SharpnessText,
    PKEY_Photo_ShutterSpeed,
    PKEY_Photo_SubjectDistance,
    PKEY_Photo_TagViewAggregate,
    PKEY_Photo_TranscodedForSync,
    PKEY_Photo_WhiteBalance,
    PKEY_Photo_WhiteBalanceText,

    PKEY_Image_ImageID,
    PKEY_Image_Dimensions,
    PKEY_Image_HorizontalSize,
    PKEY_Image_VerticalSize,
    PKEY_Image_HorizontalResolution,
    PKEY_Image_VerticalResolution,
    PKEY_Image_BitDepth,
    PKEY_Image_Compression,
    PKEY_Image_ResolutionUnit,
    PKEY_Image_ColorSpace,
    PKEY_Image_CompressedBitsPerPixel,

    PKEY_ApplicationName,
    PKEY_Author,
    PKEY_Comment,
    PKEY_Copyright,
    PKEY_DateAcquired,
    PKEY_Keywords,
    PKEY_Rating,
    PKEY_Subject,
    PKEY_Title,

    PKEY_GPS_Altitude,
    PKEY_GPS_Latitude,
    PKEY_GPS_Longitude
};

//=============================================================================

/*! RAII class for PSGetNameFromPropertyKey
*/
class NameFromPropertyKey
{
public:
    NameFromPropertyKey(PROPERTYKEY key)
        : name(nullptr)
    {
        result = PSGetNameFromPropertyKey(key, &name);
    }

    ~NameFromPropertyKey()
    {
        if(name != nullptr)
            CoTaskMemFree(name);
    }

    HRESULT result;
    WCHAR* name;

private:
    NameFromPropertyKey(const NameFromPropertyKey& other);
    NameFromPropertyKey& operator=(const NameFromPropertyKey& other);
};

//=============================================================================

/*!
* RAII class for PROPVARIANT
*/
class ScopedPropVariant : public PROPVARIANT
{
public:
    ScopedPropVariant()
    {
        PropVariantInit(this);
    }
    ~ScopedPropVariant()
    {
        PropVariantClear(this);
    }

private:
    ScopedPropVariant(const ScopedPropVariant& other);
    ScopedPropVariant& operator=(const ScopedPropVariant& other);
};

//=============================================================================

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
    const size_t previous_size = buffer.size();
    buffer.resize(buffer.size() + chunk_size);

    ULONG actually_read;
    const HRESULT read_result = stream->Read(buffer.data() + previous_size, static_cast<ULONG>(chunk_size), &actually_read);
    if(FAILED(read_result))
        return read_result;

    const bool end_of_stream = read_result == S_FALSE;

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
            // read enough of the file to have a complete header (potentially with metadata)
            // since there is no API to read just the header, multiple tries are necessary to be safe for all images

            HRESULT try_decode_result = readChunkAndTryDecoding(stream, 20480, read_buffer, decoder);
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

            ScopedPropVariant prop_width;
            HRESULT init_result = InitPropVariantFromInt32(_width, &prop_width);
            if(SUCCEEDED(init_result))
                _prop_cache->SetValueAndState(PKEY_Image_HorizontalSize, &prop_width, PSC_NORMAL);

            ScopedPropVariant prop_height;
            init_result = InitPropVariantFromInt32(_height, &prop_height);
            if(SUCCEEDED(init_result))
                _prop_cache->SetValueAndState(PKEY_Image_VerticalSize, &prop_height, PSC_NORMAL);

            ScopedPropVariant prop_dimensions;
            init_result = InitPropVariantFromString((to_wstring(_width) + L" x " + to_wstring(_height)).data(), &prop_dimensions);
            if(SUCCEEDED(init_result))
                _prop_cache->SetValueAndState(PKEY_Image_Dimensions, &prop_dimensions, PSC_NORMAL);

            ScopedPropVariant prop_bitdepth;
            init_result = InitPropVariantFromInt32(_bitdepth, &prop_bitdepth);
            if(SUCCEEDED(init_result))
                _prop_cache->SetValueAndState(PKEY_Image_BitDepth, &prop_bitdepth, PSC_NORMAL);

            ScopedCoInitialize coinit;

            ComPtr<IWICMetadataQueryReader> query_reader;
            hr = createMetadataQueryReaderFromFLIF(image, query_reader);
            if(SUCCEEDED(hr))
            {
                for(const auto& prop : SUPPORTED_METADATA_PROPERTIES)
                {
                    NameFromPropertyKey canonical_name(prop);

                    if(SUCCEEDED(canonical_name.result))
                    {
                        ScopedPropVariant value;
                        hr = query_reader->GetMetadataByName(canonical_name.name, &value);

                        if(SUCCEEDED(hr) && value.vt != VT_EMPTY)
                            _prop_cache->SetValueAndState(prop, &value, PSC_NORMAL);
                    }
                }
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
        reg.writeString(k, L"PreviewDetails", L"prop:System.Photo.DateTaken;System.Keywords;*System.Photo.PeopleNames;System.Rating;*System.Image.Dimensions;*System.Size;System.Title;System.Author;System.Comment;*System.OfflineAvailability;*System.OfflineStatus;System.Photo.CameraManufacturer;System.Photo.CameraModel;System.Subject;*System.Photo.FNumber;*System.Photo.ExposureTime;*System.Photo.ISOSpeed;*System.Photo.ExposureBias;*System.Photo.FocalLength;*System.Photo.MaxAperture;*System.Photo.MeteringMode;*System.Photo.SubjectDistance;*System.Photo.Flash;*System.Photo.FlashEnergy;*System.Photo.FocalLengthInFilm;*System.DateCreated;*System.DateModified;*System.SharedWith");
        reg.writeString(k, L"FullDetails", L"prop:System.PropGroup.Description;System.Title;System.Subject;System.Rating;System.Keywords;*System.Photo.PeopleNames;System.Comment;System.PropGroup.Origin;System.Author;System.Photo.DateTaken;System.ApplicationName;System.DateAcquired;System.Copyright;System.PropGroup.Image;System.Image.ImageID;System.Image.Dimensions;System.Image.HorizontalSize;System.Image.VerticalSize;System.Image.HorizontalResolution;System.Image.VerticalResolution;System.Image.BitDepth;System.Image.Compression;System.Image.ResolutionUnit;System.Image.ColorSpace;System.Image.CompressedBitsPerPixel;System.PropGroup.Camera;System.Photo.CameraManufacturer;System.Photo.CameraModel;System.Photo.FNumber;System.Photo.ExposureTime;System.Photo.ISOSpeed;System.Photo.ExposureBias;System.Photo.FocalLength;System.Photo.MaxAperture;System.Photo.MeteringMode;System.Photo.SubjectDistance;System.Photo.Flash;System.Photo.FlashEnergy;System.Photo.FocalLengthInFilm;System.PropGroup.PhotoAdvanced;System.Photo.LensManufacturer;System.Photo.LensModel;System.Photo.FlashManufacturer;System.Photo.FlashModel;System.Photo.CameraSerialNumber;System.Photo.Contrast;System.Photo.Brightness;System.Photo.LightSource;System.Photo.ExposureProgram;System.Photo.Saturation;System.Photo.Sharpness;System.Photo.WhiteBalance;System.Photo.PhotometricInterpretation;System.Photo.DigitalZoom;System.Photo.EXIFVersion;System.PropGroup.GPS;*System.GPS.Latitude;*System.GPS.Longitude;*System.GPS.Altitude;System.PropGroup.FileSystem;System.ItemNameDisplay;System.ItemType;System.ItemFolderPathDisplay;System.DateCreated;System.DateModified;System.Size;System.FileAttributes;System.OfflineAvailability;System.OfflineStatus;System.SharedWith;System.FileOwner;System.ComputerName");
        reg.writeString(k, L"InfoTip", L"prop:System.ItemType;System.Photo.DateTaken;System.Keywords;*System.Photo.PeopleNames;System.Rating;*System.Image.Dimensions;*System.Size;System.Title");
        reg.writeString(k, L"ExtendedTileInfo", L"prop:System.ItemType;System.Photo.DateTaken;*System.Image.Dimensions");
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
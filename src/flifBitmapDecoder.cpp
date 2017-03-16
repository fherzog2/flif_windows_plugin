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

#include "flifBitmapDecoder.h"
#include "plugin_guids.h"

flifBitmapFrameDecode::flifBitmapFrameDecode()
{
    DllAddRef();
}

flifBitmapFrameDecode::~flifBitmapFrameDecode()
{
    DllRelease();
}

STDMETHODIMP flifBitmapFrameDecode::QueryInterface(REFIID iid, void** ppvObject)
{
    if(ppvObject == 0)
        return E_INVALIDARG;

    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IWICBitmapFrameDecode))
        *ppvObject = static_cast<IWICBitmapFrameDecode*>(this);
    else if (IsEqualGUID(iid, IID_IWICBitmapSource))
        *ppvObject = static_cast<IWICBitmapSource*>(this);
    else
    {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetSize(UINT* puiWidth, UINT* puiHeight)
{
    CUSTOM_TRY

        if(puiWidth == 0 || puiHeight == 0)
            return E_INVALIDARG;

        *puiWidth = _width;
        *puiHeight = _height;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetPixelFormat(WICPixelFormatGUID* pPixelFormat)
{
    CUSTOM_TRY

        if(pPixelFormat == 0)
            return E_INVALIDARG;

        *pPixelFormat = GUID_WICPixelFormat32bppRGBA;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetResolution(double* pDpiX, double* pDpiY)
{
    CUSTOM_TRY

        if(pDpiX == 0 || pDpiY == 0)
            return E_INVALIDARG;

        *pDpiX = 96;
        *pDpiY = 96;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::CopyPalette(IWICPalette* pIPalette)
{
    CUSTOM_TRY

        return WINCODEC_ERR_PALETTEUNAVAILABLE;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::CopyPixels(const WICRect* rect_to_copy, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer)
{
    CUSTOM_TRY

        UINT copy_x = 0;
        UINT copy_y = 0;
        UINT copy_width = _width;
        UINT copy_height = _height;
        if(rect_to_copy != 0)
        {
            // negative numbers?
            if (rect_to_copy->X < 0 ||
                rect_to_copy->Y < 0 ||
                rect_to_copy->Width < 0 ||
                rect_to_copy->Height < 0)
            {
                return E_INVALIDARG;
            }

            copy_x = rect_to_copy->X;
            copy_y = rect_to_copy->Y;
            copy_width = rect_to_copy->Width;
            copy_height = rect_to_copy->Height;
        }

        // copy rect out of bounds?

        if (copy_x + copy_width > _width ||
            copy_y + copy_height > _height)
            return E_INVALIDARG;

        UINT bytesPerPixel = 4;

        // stride too small?
        if(cbStride / bytesPerPixel < copy_width)
            return E_INVALIDARG;

        // buffer too small?
        if(cbBufferSize / cbStride < copy_height)
            return WINCODEC_ERR_INSUFFICIENTBUFFER;

        UINT stride = _width * bytesPerPixel;

        // all parameters are ok, copy without further checks
        for(UINT y = 0; y < copy_height; ++y)
        {
            flifRGBA* src_line_start = _pixels.data() + (y + copy_y) * _width;
            BYTE* dst_line_start = pbBuffer + y * cbStride;

            memcpy(dst_line_start, src_line_start, copy_width * bytesPerPixel);
        }

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

// IWICBitmapFrameDecode methods
HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetMetadataQueryReader( IWICMetadataQueryReader** metadata_query_reader)
{
    CUSTOM_TRY

        return WINCODEC_ERR_UNSUPPORTEDOPERATION;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetColorContexts(UINT count, IWICColorContext** color_contexts, UINT* actual_count)
{
    CUSTOM_TRY

        if(actual_count == 0)
            return E_INVALIDARG;

        *actual_count = 0;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapFrameDecode::GetThumbnail( IWICBitmapSource** thumbnail)
{
    CUSTOM_TRY

        return WINCODEC_ERR_CODECNOTHUMBNAIL;

    CUSTOM_CATCH_RETURN_HRESULT
}

/*!
* Init function. Call directly after construction, and before the interface is handed over to other modules.
*/
void flifBitmapFrameDecode::extractFrame(const flifDecoder& decoder, int index)
{
    // this function is the only place where the members are changed
    // and it is only called immediately after construction
    // therefore, the frame data is immutable and needs need locks for multithread access

    FLIF_IMAGE* image = flif_decoder_get_image(decoder, index);
    if(image == 0)
        return;

    uint32_t w = flif_image_get_width(image);
    uint32_t h = flif_image_get_height(image);

    _width = w;
    _height = h;
    _pixels.resize(w*h);

    for(uint32_t y = 0; y < h; ++y)
        flif_image_read_row_RGBA8(image, y, _pixels.data() + y*w, w*4);
}

//=============================================================================

flifBitmapDecoder::flifBitmapDecoder()
    : _initialized(false)
{
    DllAddRef();
}

flifBitmapDecoder::~flifBitmapDecoder()
{
    DllRelease();
}

STDMETHODIMP flifBitmapDecoder::QueryInterface(REFIID iid, void** ppvObject)
{
    if(ppvObject == 0)
        return E_INVALIDARG;

    if (IsEqualGUID(iid, IID_IUnknown) || IsEqualGUID(iid, IID_IWICBitmapDecoder))
        *ppvObject = static_cast<IWICBitmapDecoder*>(this);
    else
    {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::QueryCapability(IStream* stream, DWORD* capability)
{
    CUSTOM_TRY

        if(stream == 0 || capability == 0)
            return E_INVALIDARG;

        HRESULT hr = checkStreamIsFLIF(stream);
        if(FAILED(hr))
            return hr;

        *capability =
            WICBitmapDecoderCapabilityCanDecodeAllImages |
            WICBitmapDecoderCapabilityCanDecodeSomeImages;

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::Initialize(IStream* stream, WICDecodeOptions cacheOptions)
{
    CUSTOM_TRY

        lock_guard<CriticalSection> lock(_cs_init_data);

        if(_decoder == 0)
            return E_FAIL;

        // already initialized?
        if(_initialized)
            return E_FAIL;
        
        vector<BYTE> bytes;
        HRESULT hr = streamReadAll(stream, bytes);
        if(FAILED(hr))
            return hr;

        if(flif_decoder_decode_memory(_decoder, bytes.data(), bytes.size()) == 0)
            return E_FAIL;

        _initialized = true;

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetContainerFormat(GUID* container_format)
{
    CUSTOM_TRY

        *container_format = GUID_ContainerFormatFLIF;
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetDecoderInfo(IWICBitmapDecoderInfo** decoder_info)
{
    CUSTOM_TRY

        ComPtr<IWICImagingFactory> factory;
        HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, reinterpret_cast<LPVOID*>(factory.ptrptr()));
        if(FAILED(result))
            return result;

        ComPtr<IWICComponentInfo> compInfo;
        result = factory->CreateComponentInfo(CLSID_flifBitmapDecoder, compInfo.ptrptr());
        if (FAILED(result))
            return result;

        result = compInfo->QueryInterface(IID_IWICBitmapDecoderInfo, reinterpret_cast<void**>(decoder_info));
        if (FAILED(result))
            return result;

        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::CopyPalette(IWICPalette* palette)
{
    CUSTOM_TRY

        return WINCODEC_ERR_PALETTEUNAVAILABLE;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetMetadataQueryReader(IWICMetadataQueryReader** metadata_query_reader)
{
    CUSTOM_TRY

        return WINCODEC_ERR_UNSUPPORTEDOPERATION;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetPreview(IWICBitmapSource** bitmap_source)
{
    CUSTOM_TRY

        return WINCODEC_ERR_UNSUPPORTEDOPERATION;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetColorContexts(UINT count, IWICColorContext** color_contexts, UINT* actual_count)
{
    CUSTOM_TRY

        if(actual_count == 0)
            return E_INVALIDARG;

        *actual_count = 0;
        return WINCODEC_ERR_UNSUPPORTEDOPERATION;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetThumbnail(IWICBitmapSource** thumbnail)
{
    CUSTOM_TRY

        return WINCODEC_ERR_CODECNOTHUMBNAIL;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetFrameCount(UINT* count)
{
    CUSTOM_TRY

        lock_guard<CriticalSection> lock(_cs_init_data);

        // check limits before truncating value
        size_t n = flif_decoder_num_images(_decoder);
        if(n > UINT_MAX)
            return E_FAIL;

        *count = static_cast<UINT>(n);
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifBitmapDecoder::GetFrame(UINT index, IWICBitmapFrameDecode** bitmap_frame)
{
    CUSTOM_TRY

        lock_guard<CriticalSection> lock(_cs_init_data);

        if(index >= flif_decoder_num_images(_decoder))
            return WINCODEC_ERR_FRAMEMISSING;

        if(_frames.size() < flif_decoder_num_images(_decoder))
            _frames.resize(flif_decoder_num_images(_decoder));

        if(_frames[index].get() == 0)
        {
            // lazy init for each requested frame

            ComPtr<flifBitmapFrameDecode> frame(new flifBitmapFrameDecode());
            frame->extractFrame(_decoder, index);

            _frames[index] = move(frame);
        }

        _frames[index]->QueryInterface(IID_IWICBitmapFrameDecode, reinterpret_cast<void**>(bitmap_frame));
        return S_OK;

    CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT flifBitmapDecoder::checkStreamIsFLIF(IStream* stream)
{
    char buffer[4];
    ULONG bytes_read;
    HRESULT hr = stream->Read(buffer, 4, &bytes_read);
    if(FAILED(hr))
        return hr;

    if(bytes_read < 4)
        return WINCODEC_ERR_BADHEADER;

    if(memcpy(buffer, "FLIF", 4) != 0)
        return WINCODEC_ERR_BADHEADER;

    return S_OK;
}

HRESULT flifBitmapDecoder::streamReadAll(IStream* stream, vector<BYTE>& bytes)
{
    const size_t GROWTH_STEP = 10000;

    size_t bytes_filled_counter = 0;

    while(true)
    {
        bytes.resize(bytes.size() + GROWTH_STEP);

        ULONG actually_read;
        HRESULT hr = stream->Read(bytes.data() + bytes_filled_counter, GROWTH_STEP, &actually_read);
        if(FAILED(hr))
            return hr;

        // exit condition: read less than what was requested
        if(actually_read < GROWTH_STEP)
        {
            // not handling funky return codes, just the documented ones
            if(hr != S_OK && hr != S_FALSE)
                return E_UNEXPECTED;

            // remove unused bytes
            bytes.resize(bytes_filled_counter + actually_read);
            return S_OK;
        }

        bytes_filled_counter += actually_read;
    }

    // For completeness. Because both success and fail are handled in the while-loop, this line can never be reached.
    return E_UNEXPECTED;
}

void flifBitmapDecoder::registerClass(RegistryManager& reg)
{
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifBitmapDecoder));
        reg.writeString(k, L"", L"FLIF Decoder");
        reg.writeString(k, L"ContainerFormat", to_wstring(GUID_ContainerFormatFLIF));
        reg.writeString(k, L"FileExtensions", L".flif");
        reg.writeString(k, L"FriendlyName", L"FLIF Decoder");
        reg.writeString(k, L"MimeTypes", L"image/flif");
        reg.writeString(k, L"VendorGUID", L"{F0967481-BDCE-4850-88A9-47303C90B96C}");
        reg.writeDWORD(k, L"SupportsAnimation", 1);
        //reg.writeDWORD(k, L"SupportChromakey", 0);
        reg.writeDWORD(k, L"SupportLossless ", 1);
        reg.writeDWORD(k, L"SupportMultiframe", 0);
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifBitmapDecoder) + L"\\Formats\\" + to_wstring(GUID_WICPixelFormat32bppRGBA));
        reg.writeString(k, L"", L"");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifBitmapDecoder) + L"\\InprocServer32");
        reg.writeString(k, L"", getThisLibraryPath());
        reg.writeString(k, L"ThreadingModel", L"Both");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifBitmapDecoder) + L"\\Patterns\\0");
        reg.writeDWORD(k, L"Position", 0);
        reg.writeDWORD(k, L"Length", 4);
        BYTE pattern[] = { 'F', 'L', 'I', 'F' };
        BYTE mask[] = { 255, 255, 255, 255 };
        reg.writeData(k, L"Pattern", pattern, sizeof(pattern));
        reg.writeData(k, L"Mask", mask, sizeof(mask));
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"CLSID\\{7ED96837-96F0-4812-B211-F13C24117ED3}\\Instance\\" + to_wstring(CLSID_flifBitmapDecoder));
        reg.writeString(k, L"CLSID", to_wstring(CLSID_flifBitmapDecoder));
        reg.writeString(k, L"FriendlyName", L"FLIF Decoder");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif");
        reg.writeString(k, L"", L"fliffile");
        reg.writeString(k, L"Content Type", L"image/flif");
        reg.writeString(k, L"PerceivedType", L"image");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\OpenWithList\\PhotoViewer.dll");
        reg.writeString(k, L"", L"");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\OpenWithProgids");
        reg.writeString(k, L"fliffile", L"");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\ShellEx\\ContextMenuHandlers\\ShellImagePreview");
        reg.writeString(k, L"", L"{FFE2A43C-56B9-4bf5-9A79-CC6D4285608A}");
    }

    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}");
        reg.writeString(k, L"", L"{C7657C4A-9F68-40fa-A4DF-96BC08EB3551}");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L".flif\\ShellEx\\ContextMenuHandlers\\ShellImagePreview");
        reg.writeString(k, L"", L"{FFE2A43C-56B9-4bf5-9A79-CC6D4285608A}");
    }

    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"fliffile");
        reg.writeString(k, L"", L"FLIF Image");
        //reg.writeString(k, L"FriendlyTypeName", L"@" + getThisLibraryPath() + L",-1025");
        reg.writeString(k, L"FriendlyTypeName", L"FLIF Image");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"fliffile\\DefaultIcon");
        reg.writeExpandableString(k, L"", L"%SystemRoot%\\System32\\imageres.dll,-72");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"fliffile\\shell\\open");
        reg.writeExpandableString(k, L"MuiVerb", L"@%PROGRAMFILES%\\Windows Photo Viewer\\photoviewer.dll,-3043");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"fliffile\\shell\\open\\command");
        reg.writeExpandableString(k, L"", L"%SystemRoot%\\System32\\rundll32.exe \"\"%ProgramFiles%\\Windows Photo Viewer\\PhotoViewer.dll\"\", ImageView_Fullscreen %1");
    }
    {
        auto k = reg.key(HKEY_CLASSES_ROOT, L"fliffile\\shell\\printto\\command");
        reg.writeExpandableString(k, L"", L"%SystemRoot%\\System32\\rundll32.exe \"%SystemRoot%\\System32\\shimgvw.dll\", ImageView_PrintTo /pt \"%1\" \"%2\" \"%3\" \"%4");
    }
}

void flifBitmapDecoder::unregisterClass(RegistryManager& reg)
{
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L".flif"));
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L"fliffile"));
    reg.removeTree(reg.key(HKEY_CLASSES_ROOT, L"CLSID\\" + to_wstring(CLSID_flifBitmapDecoder)));
}
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

#include <wincodec.h>

#include "util.h"
#include "RegistryManager.h"
#include "flifWrapper.h"

class flifBitmapFrameDecode : public IWICBitmapFrameDecode
{
public:
    flifBitmapFrameDecode();
    virtual ~flifBitmapFrameDecode();

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override { return _ref_count.addRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() override { return _ref_count.releaseRef(this); }

    // IWICBitmapSource methods
    virtual HRESULT STDMETHODCALLTYPE GetSize(UINT* puiWidth, UINT* puiHeight) override;
    virtual HRESULT STDMETHODCALLTYPE GetPixelFormat(WICPixelFormatGUID* pPixelFormat) override;
    virtual HRESULT STDMETHODCALLTYPE GetResolution(double* pDpiX, double* pDpiY) override;
    virtual HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette* pIPalette) override;
    virtual HRESULT STDMETHODCALLTYPE CopyPixels(const WICRect* prc, UINT cbStride, UINT cbBufferSize, BYTE* pbBuffer) override;

    // IWICBitmapFrameDecode methods
    virtual HRESULT STDMETHODCALLTYPE GetMetadataQueryReader( IWICMetadataQueryReader** metadata_query_reader) override;
    virtual HRESULT STDMETHODCALLTYPE GetColorContexts(UINT cCount, IWICColorContext** color_contexts, UINT* actual_count) override;
    virtual HRESULT STDMETHODCALLTYPE GetThumbnail( IWICBitmapSource** thumbnail) override;

    void extractFrame(const flifDecoder& decoder, int index);

private:
    ComRefCountImpl _ref_count;

    uint32_t _width;
    uint32_t _height;
    std::vector<flifRGBA> _pixels;
};

/*!
* Bitmap Decoder for FLIF files.
* Makes it possible to view FLIF files in Windows Explorer or the Photo Viewer.
*/
class flifBitmapDecoder : public IWICBitmapDecoder
{
public:
    flifBitmapDecoder();
    virtual ~flifBitmapDecoder();

    // IUnknown methods
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override;
    virtual ULONG STDMETHODCALLTYPE AddRef() override { return _ref_count.addRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() override { return _ref_count.releaseRef(this); }

    // IWICBitmapDecoder methods
    virtual HRESULT STDMETHODCALLTYPE QueryCapability(IStream* stream, DWORD* capability) override;
    virtual HRESULT STDMETHODCALLTYPE Initialize(IStream* stream, WICDecodeOptions cacheOptions) override;
    virtual HRESULT STDMETHODCALLTYPE GetContainerFormat(GUID* container_format) override;
    virtual HRESULT STDMETHODCALLTYPE GetDecoderInfo(IWICBitmapDecoderInfo** decoder_info) override;
    virtual HRESULT STDMETHODCALLTYPE CopyPalette(IWICPalette* palette) override;
    virtual HRESULT STDMETHODCALLTYPE GetMetadataQueryReader(IWICMetadataQueryReader** metadata_query_reader) override;
    virtual HRESULT STDMETHODCALLTYPE GetPreview(IWICBitmapSource** bitmap_source) override;
    virtual HRESULT STDMETHODCALLTYPE GetColorContexts(UINT count, IWICColorContext** color_contexts, UINT* actual_count) override;
    virtual HRESULT STDMETHODCALLTYPE GetThumbnail(IWICBitmapSource** thumbnail) override;
    virtual HRESULT STDMETHODCALLTYPE GetFrameCount(UINT* count) override;
    virtual HRESULT STDMETHODCALLTYPE GetFrame(UINT index, IWICBitmapFrameDecode** bitmap_frame) override;

    static void registerClass(RegistryManager& reg);
    static void unregisterClass(RegistryManager& reg);

    static HRESULT checkStreamIsFLIF(IStream* stream);
    static HRESULT streamReadAll(IStream* stream, std::vector<BYTE>& bytes);

private:
    ComRefCountImpl _ref_count;

    CriticalSection _cs_init_data;
    bool _initialized;
    flifDecoder _decoder;
    std::vector<ComPtr<flifBitmapFrameDecode>> _frames;
};
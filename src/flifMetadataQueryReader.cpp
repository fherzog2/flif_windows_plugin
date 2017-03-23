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

#define NOMINMAX
#include "flifMetadataQueryReader.h"
#include <vector>
#include <Shlwapi.h>

/*!
* APP1 header byte layout:
* 0xFF 0xE1 size1 size2 [DATA_ID_BYTES] [DATA_BYTES]
*/
bool pushAPP1Header(const unsigned char* prefix, const size_t prefix_size, const unsigned char* chunk, const size_t chunk_size, vector<unsigned char>& output_buffer)
{
    const bool prefix_must_be_added = prefix != nullptr &&
        prefix_size != 0 &&
        (chunk_size < prefix_size || memcmp(chunk, prefix, prefix_size) != 0);

    size_t size_to_write = chunk_size + 2; // +2 for size of uint16
    if(prefix_must_be_added)
        size_to_write += prefix_size;

    // sizes bigger than 16 bytes are unsupported for APP1 headers
    if(size_to_write > std::numeric_limits<uint16_t>::max())
        return false;

    output_buffer.push_back(0xFF);
    output_buffer.push_back(0xE1);

    const uint8_t size_low = static_cast<uint8_t>(size_to_write);
    const uint8_t size_high = static_cast<uint8_t>(size_to_write >> 8);
    output_buffer.push_back(size_high);
    output_buffer.push_back(size_low);

    if(prefix_must_be_added)
        output_buffer.insert(output_buffer.end(), prefix, prefix + prefix_size);

    output_buffer.insert(output_buffer.end(), chunk, chunk + chunk_size);

    return true;
}

HRESULT createMetadataQueryReaderFromFLIF(FLIF_IMAGE* image, ComPtr<IWICMetadataQueryReader>& metadata_query_reader)
{
    /*
    This is a bit hacky, but it works.

    FLIF uses the same metadata formats as JPG, so to parse them using the default facilities from Microsoft,
    a dummy JPG is constructed around the available metadata chunk from the input image.

    Pros:
    - very concise
    - re-uses exsting implementation from OS
    Cons:
    - some overhead (copy metadata into dummy JPG, parse JPG), but negligible compared to decoding a FLIF image

    */

    // jpg start + jpg_end together form a JPG file of size 1x1 (saved from MS Paint)

    const unsigned char JPG_START[] = {
        0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01,
        0x01, 0x01, 0x00, 0x60, 0x00, 0x60, 0x00, 0x00
    };

    const unsigned char JPG_END[] = {
        0xFF, 0xDB, 0x00, 0x43,
        0x00, 0x02, 0x01, 0x01, 0x02, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x03, 0x05, 0x03, 0x03, 0x03, 0x03, 0x03, 0x06, 0x04,
        0x04, 0x03, 0x05, 0x07, 0x06, 0x07, 0x07, 0x07, 0x06, 0x07, 0x07, 0x08,
        0x09, 0x0B, 0x09, 0x08, 0x08, 0x0A, 0x08, 0x07, 0x07, 0x0A, 0x0D, 0x0A,
        0x0A, 0x0B, 0x0C, 0x0C, 0x0C, 0x0C, 0x07, 0x09, 0x0E, 0x0F, 0x0D, 0x0C,
        0x0E, 0x0B, 0x0C, 0x0C, 0x0C, 0xFF, 0xDB, 0x00, 0x43, 0x01, 0x02, 0x02,
        0x02, 0x03, 0x03, 0x03, 0x06, 0x03, 0x03, 0x06, 0x0C, 0x08, 0x07, 0x08,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
        0x0C, 0x0C, 0xFF, 0xC0, 0x00, 0x11, 0x08, 0x00, 0x01, 0x00, 0x01, 0x03,
        0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xFF, 0xC4, 0x00,
        0x1F, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x10, 0x00,
        0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00,
        0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21,
        0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81,
        0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24,
        0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A,
        0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
        0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A,
        0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86,
        0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99,
        0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3,
        0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6,
        0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9,
        0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1,
        0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xC4, 0x00,
        0x1F, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
        0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0xFF, 0xC4, 0x00, 0xB5, 0x11, 0x00,
        0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00,
        0x01, 0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31,
        0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08,
        0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09, 0x23, 0x33, 0x52, 0xF0, 0x15,
        0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25, 0xF1, 0x17, 0x18,
        0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38, 0x39,
        0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
        0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84,
        0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA,
        0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4,
        0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7,
        0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
        0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00,
        0x0C, 0x03, 0x01, 0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3F, 0x00, 0xFD,
        0xFC, 0xA2, 0x8A, 0x28, 0x03, 0xFF, 0xD9
    };

    flifMetaData exif(image, "eXif");
    flifMetaData xmp(image, "eXmp");

    if (exif.data() == nullptr &&
        xmp.data() == nullptr)
    {
        // early out, no metadata to parse
        return E_INVALIDARG;
    }

    std::vector<unsigned char> dummy_jpg;

    // start of image
    dummy_jpg.insert(dummy_jpg.end(), JPG_START, JPG_START + sizeof(JPG_START));
    // EXIF header
    if(exif.data() != nullptr)
    {
        pushAPP1Header(0, 0, exif.data(), exif.size(), dummy_jpg);
    }
    // XMP header
    if(xmp.data() != nullptr)
    {
        const unsigned char ADOBE_XMP_ID[] = "http://ns.adobe.com/xap/1.0/\x00";
        size_t ADOBE_XMP_ID_SIZE = sizeof(ADOBE_XMP_ID) - 1; // sizeof includes terminating null character because the array was string-initialized

        pushAPP1Header(xmp.data(), xmp.size(), ADOBE_XMP_ID, ADOBE_XMP_ID_SIZE, dummy_jpg);
    }
    // end of image
    dummy_jpg.insert(dummy_jpg.end(), JPG_END, JPG_END + sizeof(JPG_END));

    // parse JPG

    ComPtr<IWICImagingFactory> imaging_factory;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_IWICImagingFactory,
            (LPVOID*)imaging_factory.ptrptr());
    if(FAILED(hr))
        return hr;

    if(dummy_jpg.size() > std::numeric_limits<UINT>::max())
        return E_INVALIDARG;

    ComPtr<IStream> stream(SHCreateMemStream(dummy_jpg.data(), static_cast<UINT>(dummy_jpg.size())));
    if(stream.get() == nullptr)
        return E_FAIL;

    ComPtr<IWICBitmapDecoder> decoder;
    hr = imaging_factory->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, decoder.ptrptr());
    if(FAILED(hr))
        return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, frame.ptrptr());
    if (FAILED(hr))
        return hr;

    hr = frame->GetMetadataQueryReader(metadata_query_reader.ptrptr());
    return hr;
}
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

#include <flif_dec.h>

/*!
* Basic RAII wrapper for the decoder
*/
class flifDecoder
{
public:
    flifDecoder()
        : _decoder(flif_create_decoder())
    {
    }

    ~flifDecoder()
    {
        if(_decoder != 0)
            flif_destroy_decoder(_decoder);
        _decoder = 0;
    }

    flifDecoder(flifDecoder&& other)
    {
        _decoder = other._decoder;
        other._decoder = 0;
    }

    flifDecoder& operator=(flifDecoder&& other)
    {
        _decoder = other._decoder;
        other._decoder = 0;
        return *this;
    }

    operator FLIF_DECODER*() const
    {
        return _decoder;
    }

private:
    flifDecoder(const flifDecoder& other);
    flifDecoder& operator=(const flifDecoder& other);

    FLIF_DECODER* _decoder;
};

class flifInfo
{
public:
    flifInfo()
        : _info(0)
    {
    }

    flifInfo(FLIF_INFO* info)
        : _info(info)
    {
    }

    ~flifInfo()
    {
        if(_info != 0)
            flif_destroy_info(_info);
        _info = 0;
    }

    flifInfo(flifInfo&& other)
    {
        _info = other._info;
        other._info = 0;
    }

    flifInfo& flifInfo::operator=(flifInfo&& other)
    {
        _info = other._info;
        other._info = 0;
        return *this;
    }

    operator FLIF_INFO*() const
    {
        return _info;
    }

private:
    flifInfo(const flifInfo& other);
    flifInfo& operator=(const flifInfo& other);

    FLIF_INFO* _info;
};

class flifMetaData
{
public:
    flifMetaData()
        : _image(0)
        , _metadata(0)
        , _length(0)
    {}

    flifMetaData(FLIF_IMAGE* image, const char* chunkname)
        : _image(image)
        , _metadata(0)
        , _length(0)
    {
        flif_image_get_metadata(_image, chunkname, &_metadata, &_length);
    }

    ~flifMetaData()
    {
        if(_metadata != 0)
            flif_image_free_metadata(_image, _metadata);
    }

    flifMetaData(flifMetaData&& other)
    {
        _image = other._image;
        _metadata = other._metadata;
        other._metadata = 0;
        _length = other._length;
    }

    flifMetaData& operator=(flifMetaData&& other)
    {
        _image = other._image;
        _metadata = other._metadata;
        other._metadata = 0;
        _length = other._length;
        return *this;
    }

    const unsigned char* data() const
    {
        return _metadata;
    }

    size_t size() const
    {
        return _length;
    }

private:
    flifMetaData(const flifMetaData& other);
    flifMetaData& operator=(const flifMetaData& other);

    FLIF_IMAGE* _image;
    unsigned char* _metadata;
    size_t _length;
};

struct flifRGBA
{
    uint8_t r,g,b,a;
};
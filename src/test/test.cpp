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

#define INITGUID

#include <wincodec.h>
#include <Shlwapi.h>
#include <Propsys.h>
#include <propvarutil.h>
#include <fstream>

#include <vector>

#include "util.h"
#include "plugin_guids.h"
#include "flif.h"
#include "flifWrapper.h"
#include <comdef.h>

typedef HRESULT (STDAPICALLTYPE *fpDllGetClassObject)(REFCLSID, REFIID, LPVOID);

inline string formatHRESULT(HRESULT hr)
{
	_com_error e(hr);
	return e.ErrorMessage();
}

vector<BYTE> read_file(const string& filename)
{
	ifstream file(filename, std::ios::binary);
	vector<BYTE> result;

	for (auto it = std::istreambuf_iterator<char>(file); it != std::istreambuf_iterator<char>(); ++it)
		result.push_back(*it);

	return result;
}

void debug_out(const string& message)
{
	printf("%s\n", message.data());
	fflush(stdout);
}

#define MY_ASSERT(condition, message) \
    if((condition)) { \
		string message2 = __FILE__ + string("(") + to_string(__LINE__) + "): " + (message); \
        debug_out(message2);\
		fflush(stdout);\
        return 1; \
    }

#define HR_ASSERT(hr) \
    if(FAILED(hr)) { \
        string message = __FILE__ + string("(") + to_string(__LINE__) + "): " + formatHRESULT((hr)); \
        debug_out(message);\
        return 1; \
    }

bool save_bmp(BYTE* Buffer, int width, int height, DWORD paddedsize, LPCSTR bmpfile)
{
    BITMAPFILEHEADER bmfh;
    BITMAPINFOHEADER info;
    memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
    memset(&info, 0, sizeof(BITMAPINFOHEADER));

    bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
    bmfh.bfOffBits = 0x36;

    info.biSize = sizeof(BITMAPINFOHEADER);
    info.biWidth = width;
    info.biHeight = -height;
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biCompression = BI_RGB;
    info.biSizeImage = 0;
    info.biXPelsPerMeter = 0x0ec4;
    info.biYPelsPerMeter = 0x0ec4;
    info.biClrUsed = 0;
    info.biClrImportant = 0;

    HANDLE file = CreateFileA(bmpfile, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(NULL == file)
    {
        CloseHandle(file);
        return false;
    }

    DWORD bwritten;
    if(!WriteFile(file, &bmfh, sizeof ( BITMAPFILEHEADER ), &bwritten, NULL))
    {
        CloseHandle(file);
        return false;
    }

    if(!WriteFile(file, &info, sizeof ( BITMAPINFOHEADER ), &bwritten, NULL))
    {
        CloseHandle(file);
        return false;
    }

    if(!WriteFile(file, Buffer, paddedsize, &bwritten, NULL))
    {
        CloseHandle(file);
        return false;
    }

    CloseHandle(file);
    return true;
}

HRESULT save_bitmapframe(IWICBitmapFrameDecode* frame, const string& filename)
{
    UINT width;
    UINT height;
    HRESULT hr = frame->GetSize(&width, &height);
    if(FAILED(hr))
        return hr;

    WICRect full_rect = { 0, 0, width, height };
    vector<BYTE> rgba_bytes;
    rgba_bytes.resize(width * height * 4);
    hr = frame->CopyPixels(&full_rect, width * 4, static_cast<UINT>(rgba_bytes.size()), rgba_bytes.data());
    if(FAILED(hr))
        return hr;

    vector<BYTE> rgb_bytes;

    UINT stride = width * 3;
    if(stride % 4 != 0)
        stride += 4 - (stride % 4);

    auto lerp = [](BYTE x, BYTE y, BYTE t){
        float fx = float(x) / 255.0f;
        float fy = float(y) / 255.0f;
        float ft = float(t) / 255.0f;
        return BYTE((fx * ft + fy * (1.0f - ft)) * 255.0f);
    };

    rgb_bytes.resize(stride * height);
    for (UINT y = 0; y < height; ++y)
        for (UINT x = 0; x < width; ++x)
        {
            BYTE r = rgba_bytes[y * width*4 + x*4];
            BYTE g = rgba_bytes[y * width*4 + x*4 + 1];
            BYTE b = rgba_bytes[y * width*4 + x*4 + 2];
            BYTE a = rgba_bytes[y * width*4 + x*4 + 3];

            if(a < 255)
            {
                // for transparency, use a magenta background
                r = lerp(r, 255, a);
                g = lerp(g, 0, a);
                b = lerp(b, 255, a);
            }

            rgb_bytes[y * stride + x*3]     = b;
            rgb_bytes[y * stride + x*3 + 1] = g;
            rgb_bytes[y * stride + x*3 + 2] = r;
        }

    if(!save_bmp(rgb_bytes.data(), width, height, static_cast<DWORD>(rgb_bytes.size()), filename.data()))
        return E_FAIL;

    return S_OK;
}

int test_file(const string& filename, IClassFactory* class_factory_decoder, IClassFactory* class_factory_props)
{
	HRESULT hr = S_OK;

	debug_out("Testing file " + filename);

	auto file_content = read_file(filename);
	MY_ASSERT(file_content.empty(), "Read file failed")

	ComPtr<IStream> stream;
	stream.reset(SHCreateMemStream(file_content.data(), static_cast<UINT>(file_content.size())));
	MY_ASSERT(stream.get() == 0, "CreateMemStream failed")

	{
		ComPtr<IWICBitmapDecoder> decoder;
		hr = class_factory_decoder->CreateInstance(0, IID_IWICBitmapDecoder, (void**)decoder.ptrptr());
		HR_ASSERT(hr)

		hr = decoder->Initialize(stream.get(), WICDecodeMetadataCacheOnDemand);
		HR_ASSERT(hr)

		ComPtr<IWICBitmapFrameDecode> frame;
		hr = decoder->GetFrame(0, frame.ptrptr());
		HR_ASSERT(hr)

		UINT w;
		UINT h;
		hr = frame->GetSize(&w, &h);
		HR_ASSERT(hr)

		{
			WICRect line_rect = { 0, 0, w, 1 };
			vector<BYTE> line;
			line.resize(w * 4);
			hr = frame->CopyPixels(&line_rect, w * 4, static_cast<UINT>(line.size()), line.data());
			HR_ASSERT(hr)
		}

		{
			WICRect full_rect = { 0, 0, w, h };
			vector<BYTE> full;
			full.resize(w * h * 4);
			hr = frame->CopyPixels(&full_rect, w * 4, static_cast<UINT>(full.size()), full.data());
			HR_ASSERT(hr)
		}

        // save the decoded image as bitmap so we can verify the plugin works without actually installing it

        string decoded_filename = filename;
        decoded_filename.replace(decoded_filename.rfind(".flif"), 5, " (decoded).bmp");
        auto path_end = decoded_filename.rfind("/");
        if(path_end != string::npos)
            decoded_filename.replace(0, path_end + 1, "");

        hr = save_bitmapframe(frame.get(), decoded_filename);
        HR_ASSERT(hr)
	}

	LARGE_INTEGER start_pos;
	start_pos.QuadPart = 0;
	hr = stream->Seek(start_pos, STREAM_SEEK_SET, 0);
	HR_ASSERT(hr)

	{
		ComPtr<IPropertyStore> props;
		hr = class_factory_props->CreateInstance(0, IID_IPropertyStore, (void**)props.ptrptr());
		HR_ASSERT(hr)

		ComPtr<IInitializeWithStream> props_init;
		hr = props->QueryInterface(IID_IInitializeWithStream, (void**)props_init.ptrptr());
		HR_ASSERT(hr)

		hr = props_init->Initialize(stream.get(), STGM_READ);
		HR_ASSERT(hr)

		DWORD count;
		hr = props->GetCount(&count);
		HR_ASSERT(hr)
		MY_ASSERT(count == 0, "No attributes");

        debug_out("Printing metadata");

        CoInitialize(0);

		for(DWORD i = 0; i < count; ++i)
		{
			PROPERTYKEY key;
			hr = props->GetAt(i, &key);
			HR_ASSERT(hr)

            PWSTR key_string;
            hr = PSGetNameFromPropertyKey (key, &key_string);
            HR_ASSERT(hr)

			PROPVARIANT var;
			PropVariantInit(&var);
			hr = props->GetValue(key, &var);
			HR_ASSERT(hr)

            bool printed = false;

            UINT value_uint;
            if(!printed && SUCCEEDED(PropVariantToUInt32(var, &value_uint)))
            {
                wprintf(L"    %s\t%d\n", key_string, value_uint);
                printed = true;
            }

            const UINT VALUE_STRING_SIZE = 1024;
            WCHAR value_string[VALUE_STRING_SIZE];
            if(!printed && SUCCEEDED(PropVariantToString(var, value_string, VALUE_STRING_SIZE)))
            {
                wprintf(L"    %s\t%s\n", key_string, value_string);
                printed = true;
            }

            if(!printed)
                wprintf(L"    %s\tUnsupported datatype, update this printing code\n", key_string);

            CoTaskMemFree(key_string);
		}

        CoUninitialize();
	}

	return 0;
}

int main(int argc, char** args)
{
    if(argc <= 1)
    {
        debug_out("missing argument");
        return 1;
    }

    auto plugin = LoadLibraryW(L"flif_windows_plugin.dll");
    
    auto get_class_object = reinterpret_cast<fpDllGetClassObject>(GetProcAddress(plugin, "DllGetClassObject"));
    
	ComPtr<IClassFactory> class_factory_decoder;
	HRESULT hr = get_class_object(CLSID_flifBitmapDecoder, IID_IClassFactory, class_factory_decoder.ptrptr());
    HR_ASSERT(hr)

	ComPtr<IClassFactory> class_factory_props;
	hr = get_class_object(CLSID_flifPropertyHandler, IID_IClassFactory, class_factory_props.ptrptr());
    HR_ASSERT(hr)

	// test premade file

	if(test_file(args[1], class_factory_decoder.get(), class_factory_props.get()) != 0)
		return 1;

	// create fresh file

	debug_out("creating fresh image");

	FLIF_ENCODER* encoder = flif_create_encoder();
	MY_ASSERT(encoder == 0, "flif_create_encoder failed");

	const uint32_t SIZE = 256;
	FLIF_IMAGE* image = flif_create_image(SIZE, SIZE);
	MY_ASSERT(image == 0, "flif_create_image failed");

	for(uint32_t y = 0; y < SIZE; ++y)
	{
		flifRGBA rgba[SIZE];
		for(uint32_t x = 0; x < SIZE; ++x)
		{
			rgba[x].r = x;
			rgba[x].g = y;
			rgba[x].b = 0;
			rgba[x].a = 255;
		}
		flif_image_write_row_RGBA8(image, y, rgba, sizeof(rgba));
	}

	debug_out("start encoding with FLIF");

	flif_encoder_add_image(encoder, image);

	MY_ASSERT(flif_encoder_encode_file(encoder, "test.flif") != 1, "flif_encoder_encode_file failed")

	// test with fresh file

	if(test_file("test.flif", class_factory_decoder.get(), class_factory_props.get()) != 0)
		return 1;

	return 0;
}
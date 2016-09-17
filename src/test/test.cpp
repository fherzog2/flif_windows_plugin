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

		for(DWORD i = 0; i < count; ++i)
		{
			PROPERTYKEY key;
			hr = props->GetAt(i, &key);
			HR_ASSERT(hr)

			PROPVARIANT var;
			PropVariantInit(&var);
			hr = props->GetValue(key, &var);
			HR_ASSERT(hr)

			wprintf(L"%d %s\n", PropVariantToUInt32WithDefault(var, -1), PropVariantToStringWithDefault(var, L"Not-A-String"));
		}
	}

	return 0;
}

int main(int argc, char** args)
{
    auto plugin = LoadLibraryW(L"flif_windows_plugin.dll");
    
    auto get_class_object = reinterpret_cast<fpDllGetClassObject>(GetProcAddress(plugin, "DllGetClassObject"));
    
	ComPtr<IClassFactory> class_factory_decoder;
	HRESULT hr = get_class_object(CLSID_flifBitmapDecoder, IID_IClassFactory, class_factory_decoder.ptrptr());
    HR_ASSERT(hr)

	ComPtr<IClassFactory> class_factory_props;
	hr = get_class_object(CLSID_flifPropertyHandler, IID_IClassFactory, class_factory_props.ptrptr());
    HR_ASSERT(hr)

	// test premade file

	if(test_file("E:\\workspace\\C++\\flif_windows_plugin\\src\\test\\flif.flif", class_factory_decoder.get(), class_factory_props.get()) != 0)
		return 1;

	//if(test_file(args[1], class_factory.get()) != 0)
		//return 1;

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
		flif_image_write_row_RGBA8(image, 0, rgba, sizeof(rgba));
	}

	debug_out("start encoding with FLIF");

	flif_encoder_add_image(encoder, image);

	MY_ASSERT(flif_encoder_encode_file(encoder, "test.flif") != 1, "flif_encoder_encode_file failed")

	// test with fresh file

	if(test_file("test.flif", class_factory_decoder.get(), class_factory_props.get()) != 0)
		return 1;

	return 0;
}
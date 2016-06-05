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
#include <fstream>

#include <vector>

#include "util.h"
#include "plugin_guids.h"

typedef HRESULT (STDAPICALLTYPE *fpDllGetClassObject)(REFCLSID, REFIID, LPVOID);

inline string formatHRESULT(HRESULT hr)
{
	const DWORD BUFFER_SIZE = 1024;
	CHAR buffer[BUFFER_SIZE];
	if(0 != FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, hr, 0, buffer, BUFFER_SIZE, 0))
		return buffer;

	return string();
}

vector<BYTE> read_file(const string& filename)
{
	ifstream file(filename, std::ios::binary);
	vector<BYTE> result;

	for (auto it = std::istreambuf_iterator<char>(file); it != std::istreambuf_iterator<char>(); ++it)
		result.push_back(*it);

	return result;
}

int main(int argc, char** args)
{
	printf("Loading plugin\n");

    auto plugin = LoadLibraryW(L"flif_windows_plugin.dll");
    
    auto get_class_object = reinterpret_cast<fpDllGetClassObject>(GetProcAddress(plugin, "DllGetClassObject"));

	printf("Getting interface\n");
    
	ComPtr<IClassFactory> class_factory;
	HRESULT hr = get_class_object(CLSID_flifBitmapDecoder, IID_IClassFactory, class_factory.ptrptr());
	if(FAILED(hr))
		return 1;

	ComPtr<IWICBitmapDecoder> decoder;
	hr = class_factory->CreateInstance(0, IID_IWICBitmapDecoder, (void**)decoder.ptrptr());
	if(FAILED(hr))
		return 1;

	printf("Opening file %s\n", args[1]);

	auto file_content = read_file(args[1]);
	if(file_content.empty())
		return 1;

	ComPtr<IStream> stream;
	stream.reset(SHCreateMemStream(file_content.data(), static_cast<UINT>(file_content.size())));
	if(stream.get() == 0)
		return 1;

	printf("Decompressing file\n");

	hr = decoder->Initialize(stream.get(), WICDecodeMetadataCacheOnDemand);
	if(FAILED(hr))
		return 1;

	printf("Retrieving frame\n");

	ComPtr<IWICBitmapFrameDecode> frame;
	hr = decoder->GetFrame(0, frame.ptrptr());
	if(FAILED(hr))
		return 1;

	UINT w;
	UINT h;
	hr = frame->GetSize(&w, &h);
	if(FAILED(hr))
		return 1;

	{
		WICRect line_rect = { 0, 0, w, 1 };
		vector<BYTE> line;
		hr = frame->CopyPixels(&line_rect, w * 4, static_cast<UINT>(line.size()), line.data());
		if(FAILED(hr))
			return 1;
	}

	{
		WICRect full_rect = { 0, 0, w, h };
		vector<BYTE> full;
		hr = frame->CopyPixels(&full_rect, w * 4, static_cast<UINT>(full.size()), full.data());
		if(FAILED(hr))
			return 1;
	}

	return 0;
}
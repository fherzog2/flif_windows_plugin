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

		if (cProps == 0)
			return E_INVALIDARG;

		*cProps = _countof(supported_flif_properties);

		return S_OK;

	CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::GetAt( DWORD iProp, PROPERTYKEY *pkey)
{
	CUSTOM_TRY

		if (pkey == 0)
			return E_INVALIDARG;

		if (iProp >= _countof(supported_flif_properties))
			return E_BOUNDS;

		*pkey = supported_flif_properties[iProp];
		return S_OK;

	CUSTOM_CATCH_RETURN_HRESULT
}

HRESULT STDMETHODCALLTYPE flifPropertyHandler::GetValue(REFPROPERTYKEY key, PROPVARIANT *pv)
{
	CUSTOM_TRY

		if (pv == 0)
			return E_INVALIDARG;

		if (key == PKEY_Image_HorizontalSize)
		{
			pv->uintVal = _width;
			pv->vt = VT_UI4;
			return S_OK;
		}
		else if (key == PKEY_Image_VerticalSize)
		{
			pv->uintVal = _height;
			pv->vt = VT_UI4;
			return S_OK;
		}
		else if (key == PKEY_Image_Dimensions)
		{
			auto dimensions = to_wstring(_width) + L" x " + to_wstring(_height);

			HRESULT init_result = InitPropVariantFromString(dimensions.data(), pv);

			return FAILED(init_result) ? init_result : S_OK;
		}
		else if (key == PKEY_Image_BitDepth)
		{
			pv->uintVal = _bitdepth;
			pv->vt = VT_UI4;
			return S_OK;
		}

		pv->vt = VT_EMPTY;
		return S_OK;

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

HRESULT STDMETHODCALLTYPE flifPropertyHandler::Initialize(IStream *stream, DWORD grfMode)
{
	CUSTOM_TRY

		lock_guard<CriticalSection> lock(_cs_init);
		if(_is_initialized)
			return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);

		/*
		   HACK

		   There is currently no nice interface to get the dimensions of a FLIF file.
		   This code makes use of the fact that truncated FLIF files can still be decoded.
		   So it read a tiny amount of the file and decodes an image from this to get the dimensions.
		   For some files this fails, because it is not clear how much data must be read to get an image.

		   TODO: Obviously this is inefficient. Need to create an interface in FLIF.
		*/ 

		const ULONG max_read = 100;
		ULONG actually_read;
		vector<BYTE> bytes;
		bytes.resize(max_read);
		HRESULT read_result = stream->Read(bytes.data(), max_read, &actually_read);
		if(FAILED(read_result))
			return read_result;

		bytes.resize(actually_read);

		flifDecoder decoder;
		if(decoder == 0)
			return E_FAIL;

		if(flif_decoder_decode_memory(decoder, bytes.data(), bytes.size()) == 0)
			return E_FAIL;

		FLIF_IMAGE* image = flif_decoder_get_image(decoder, 0);
		if(image == 0)
			return E_FAIL;

		_width = flif_image_get_width(image);
		_height = flif_image_get_height(image);
		_bitdepth = flif_image_get_depth(image) * flif_image_get_nb_channels(image);
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
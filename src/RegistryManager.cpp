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

#include "RegistryManager.h"

inline std::wstring formatRegErrorCode(LONG error)
{
    const DWORD BUFFER_SIZE = 256;
    WCHAR buffer[BUFFER_SIZE];
    if(0 != FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, buffer, BUFFER_SIZE, 0))
        return buffer;

    return std::wstring();
}

//=============================================================================

RegistryManager::ErrorRecord::ErrorRecord(const Key& key, const std::wstring& value_name, LONG error, const std::wstring& context)
    : _key(key)
    , _value_name(value_name)
    , _error_code(HRESULT_FROM_WIN32(error))
    , _error(context + L": " + formatRegErrorCode(error))
{
}

//=============================================================================

RegistryManager::Key RegistryManager::key(HKEY root, const std::wstring& subkey)
{
    return Key(root, subkey);
}

HRESULT RegistryManager::writeValue(const Key& key, const std::wstring& value_name, DWORD value_type, const BYTE* value_ptr, DWORD value_size)
{
    HKEY hkey = 0;
    LONG create_key_result = RegCreateKeyW(key._root, key._subkey.data(), &hkey);
    if(create_key_result != ERROR_SUCCESS)
    {
        _error_buffer.push_back(ErrorRecord(key, value_name, create_key_result, L"RegCreateKey"));
        return HRESULT_FROM_WIN32(create_key_result);
    }

    LONG set_value_result = RegSetValueExW(hkey, value_name.data(), 0, value_type, value_ptr, value_size);
    if(set_value_result != ERROR_SUCCESS)
        _error_buffer.push_back(ErrorRecord(key, value_name, set_value_result, L"RegSetValueEx"));

    LONG close_result = RegCloseKey(hkey);
    if(close_result != ERROR_SUCCESS)
        _error_buffer.push_back(ErrorRecord(key, value_name, close_result, L"RegCloseKey"));

    if(set_value_result != ERROR_SUCCESS)
        return HRESULT_FROM_WIN32(set_value_result);

    return S_OK;
}

HRESULT RegistryManager::writeDWORD(const Key& key, const std::wstring& value_name, DWORD value)
{
    return writeValue(key, value_name, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(DWORD));
}

HRESULT RegistryManager::writeString(const Key& key, const std::wstring& value_name, const std::wstring& value)
{
    // check limits before truncating value
    size_t bytes_to_write = (value.size() + 1) * sizeof(WCHAR);
    if(bytes_to_write > ULONG_MAX)
        return E_INVALIDARG;

    return writeValue(key, value_name, REG_SZ, reinterpret_cast<const BYTE*>(value.data()), static_cast<DWORD>(bytes_to_write));
}

HRESULT RegistryManager::writeExpandableString(const Key& key, const std::wstring& value_name, const std::wstring& value)
{
    // check limits before truncating value
    size_t bytes_to_write = (value.size() + 1) * sizeof(WCHAR);
    if(bytes_to_write > ULONG_MAX)
        return E_INVALIDARG;

    return writeValue(key, value_name, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(value.data()), static_cast<DWORD>(bytes_to_write));
}

HRESULT RegistryManager::writeData(const Key& key, const std::wstring& value_name, const BYTE* data, DWORD data_size)
{
    return writeValue(key, value_name, REG_BINARY, data, data_size);
}

HRESULT RegistryManager::removeKey(const Key& key)
{
    LONG delete_result = RegDeleteKeyW(key._root, key._subkey.data());
    if (delete_result != ERROR_SUCCESS)
    {
        _error_buffer.push_back(ErrorRecord(key, L"", delete_result, L"RegDeleteKey"));
        return HRESULT_FROM_WIN32(delete_result);
    }

    return S_OK;
}

HRESULT RegistryManager::removeTree(const Key& key)
{
    LONG delete_result = RegDeleteTreeW(key._root, key._subkey.data());
    if (delete_result != ERROR_SUCCESS)
    {
        _error_buffer.push_back(ErrorRecord(key, L"", delete_result, L"RegDeleteTree"));
        return HRESULT_FROM_WIN32(delete_result);
    }

    return S_OK;
}

const std::vector<RegistryManager::ErrorRecord>& RegistryManager::getErrors() const
{
    return _error_buffer;
}

void RegistryManager::showErrors() const
{
    std::wstring text;

    for(size_t i = 0; i < _error_buffer.size() && i < 10; ++i)
    {
        const ErrorRecord& e = _error_buffer[i];

        std::wstring root_str = L"unknown root";

        if(e._key._root == HKEY_CLASSES_ROOT)
            root_str = L"HKEY_CLASSES_ROOT";
        if(e._key._root == HKEY_CURRENT_USER)
            root_str = L"HKEY_CURRENT_USER";
        if(e._key._root == HKEY_LOCAL_MACHINE)
            root_str = L"HKEY_LOCAL_MACHINE";
        if(e._key._root == HKEY_USERS)
            root_str = L"HKEY_USERS";
        if(e._key._root == HKEY_CURRENT_CONFIG)
            root_str = L"HKEY_CURRENT_CONFIG";

        text += L"(" + root_str + L"," + e._key._subkey + L")\n";
        if(!e._value_name.empty())
            text += L"Value: " + e._value_name + L"\n";
        text += e._error + L"\n";
    }

    MessageBoxW(0, text.data(), 0, 0);
}
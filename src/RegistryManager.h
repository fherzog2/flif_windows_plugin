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

#include <Windows.h>
#include <string>
#include <vector>

class RegistryManager
{
public:
    class Key
    {
    public:
        Key()
            : _root(HKEY_CURRENT_USER)
        {
        }
        Key(HKEY root, const std::wstring& subkey)
            : _root(root)
            , _subkey(subkey)
        {
        }

        HKEY _root;
        std::wstring _subkey;
    };

    class ErrorRecord
    {
    public:
        ErrorRecord(const Key& key, const std::wstring& value_name, LONG error, const std::wstring& context);

        Key _key;
        std::wstring _value_name;

        HRESULT _error_code;
        std::wstring _error;
    };

    static Key key(HKEY root, const std::wstring& subkey);

    HRESULT writeDWORD(const Key& key, const std::wstring& value_name, DWORD value);
    HRESULT writeString(const Key& key, const std::wstring& value_name, const std::wstring& value);
    HRESULT writeExpandableString(const Key& key, const std::wstring& value_name, const std::wstring& value);
    HRESULT writeData(const Key& key, const std::wstring& value_name, const BYTE* data, DWORD data_size);
    HRESULT removeKey(const Key& key);
    HRESULT removeTree(const Key& key);

    const std::vector<ErrorRecord>& getErrors() const;
    void showErrors() const;
private:
    HRESULT writeValue(const Key& key, const std::wstring& value_name, DWORD value_type, const BYTE* value_ptr, DWORD value_size);

    std::vector<ErrorRecord> _error_buffer;
};
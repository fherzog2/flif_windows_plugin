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
#include <mutex>
using namespace std;

/*!
* All interface functions must contain this exception wrapper (start)
*/

#define CUSTOM_TRY try{

/*!
* All interface functions must contain this exception wrapper (end)
*/

#define CUSTOM_CATCH_RETURN_HRESULT } \
    catch(const bad_alloc&) {\
        return E_OUTOFMEMORY; \
    } \
    catch(...) {\
        return E_UNEXPECTED; \
    } \

/*!
* RAII class for CRITICAL_SECTION.
* Can be used with std::lock_guard.
*/
class CriticalSection
{
public:
    CriticalSection()
    {
        InitializeCriticalSection(&_cs);
    }
    ~CriticalSection()
    {
        DeleteCriticalSection(&_cs);
    }

    void lock()
    {
        EnterCriticalSection(&_cs);
    }

    void unlock()
    {
        LeaveCriticalSection(&_cs);
    }
private:
    CRITICAL_SECTION _cs;
};

/*!
* Smart pointer for COM interfaces.
* Manages the necessary calls for IUnknown::AddRef and IUnknown::Release.
*
* ComPtr<ISomeInterface> ptr(new MyInterface());
*/
template<class T>
class ComPtr
{
public:
    ComPtr()
        : _ptr(0)
    {
    }

    ComPtr(T* ptr)
        : _ptr(0)
    {
        reset(ptr);
    }
    
    ~ComPtr()
    {
        reset(0);
    }
    
    ComPtr(const ComPtr& other)
        : _ptr()
    {
        operator=(other);
    }

    ComPtr& operator=(const ComPtr& other)
    {
        reset(other.get());
        return *this;
    }
    
    ComPtr(ComPtr&& other)
        : _ptr(0)
    {
        operator=(other);
    }

    ComPtr& operator=(ComPtr&& other)
    {
        _ptr = other._ptr;
        other._ptr = 0;
        return *this;
    }

    /*!
    * Access referenced instance.
    */
    T* get() const
    {
        return _ptr;
    }

    T* operator->() const
    {
        return _ptr;
    }

    /*!
    * Assign a new instance, possibly releasing a previously referenced instance.
    */
    void reset(T* ptr)
    {
        if(ptr)
            ptr->AddRef();
        
        if(_ptr)
            _ptr->Release();
        _ptr = ptr;
    }

    /*!
    * Get address of internal pointer.
    * Some COM functions require this.
    */
    T** ptrptr()
    {
        reset(0);
        return &_ptr;
    }

private:
    T* _ptr;
};

/*!
* Common implementation for the AddRef and Release interface of COM objects.
*/
class ComRefCountImpl
{
public:
    ComRefCountImpl()
        : _rc(0)
    {
    }
    
    int addRef()
    {
        return InterlockedIncrement(&_rc);
    }
    
    template<class T>
    int releaseRef(T* com_object)
    {
        ULONG count = InterlockedDecrement(&_rc);
        if(0 == count)
            delete com_object;
        return count;
    }
private:
    ULONG _rc;
};

void DllAddRef();
void DllRelease();
wstring getThisLibraryPath();
wstring to_wstring(const GUID& guid);
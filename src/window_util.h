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

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace win
{
#define STATIC_ASSERT_NO_COPY(CLASSNAME) \
    static_assert(std::is_default_constructible<CLASSNAME>::value, #CLASSNAME ": default constructor required"); \
    static_assert(!std::is_copy_constructible<CLASSNAME>::value, #CLASSNAME ": no copy construction allowed"); \
    static_assert(!std::is_copy_assignable<CLASSNAME>::value, #CLASSNAME ": no copy construction allowed"); \
    static_assert(std::is_move_constructible<CLASSNAME>::value, #CLASSNAME ": move construction required"); \
    static_assert(std::is_move_assignable<CLASSNAME>::value, #CLASSNAME ": move construction required");

#define STATIC_ASSERT_NO_COPY_NO_MOVE(CLASSNAME) \
    static_assert(!std::is_default_constructible<CLASSNAME>::value, #CLASSNAME ": no default constructor allowed"); \
    static_assert(!std::is_copy_constructible<CLASSNAME>::value, #CLASSNAME ": no copy construction allowed"); \
    static_assert(!std::is_copy_assignable<CLASSNAME>::value, #CLASSNAME ": no copy construction allowed"); \
    static_assert(!std::is_move_constructible<CLASSNAME>::value, #CLASSNAME ": no move construction allowed"); \
    static_assert(!std::is_move_assignable<CLASSNAME>::value, #CLASSNAME ": no move construction allowed");

    class Window
    {
    public:
        Window() = default;
        Window(HWND hwnd)
            : _hwnd(hwnd)
        {
        }

        Window(Window&& other)
        {
            _hwnd = other._hwnd;
            other._hwnd = 0;
        }

        Window& operator=(Window&& other)
        {
            if (_hwnd)
                DestroyWindow(_hwnd);

            _hwnd = other._hwnd;
            other._hwnd = 0;
            return *this;
        }

        ~Window()
        {
            if (_hwnd)
                DestroyWindow(_hwnd);
        }

        operator HWND() const
        {
            return _hwnd;
        }

        HWND get() const
        {
            return _hwnd;
        }

    private:
        HWND _hwnd = 0;
    };

    STATIC_ASSERT_NO_COPY(Window)

    class ClientDC
    {
    public:
        ClientDC(HWND hwnd)
            : _hwnd(hwnd)
            , _dc(GetDC(hwnd))
        {
        }

        ClientDC(const ClientDC& other) = delete;
        ClientDC& operator=(const ClientDC& other) = delete;

        ~ClientDC()
        {
            if (_dc)
                ReleaseDC(_hwnd, _dc);
        }

        operator HDC() const
        {
            return _dc;
        }

        HWND _hwnd = 0;
        HDC _dc = 0;
    };

    STATIC_ASSERT_NO_COPY_NO_MOVE(ClientDC)

    class MemoryDC
    {
    public:
        MemoryDC(HDC dc)
            : _dc(CreateCompatibleDC(dc))
        {
        }

        MemoryDC(const MemoryDC& other) = delete;
        MemoryDC& operator=(const MemoryDC& other) = delete;

        ~MemoryDC()
        {
            DeleteDC(_dc);
        }

        operator HDC() const
        {
            return _dc;
        }
    private:
        HDC _dc;
    };

    STATIC_ASSERT_NO_COPY_NO_MOVE(MemoryDC)

    class SaveAndRestoreDC
    {
    public:
        SaveAndRestoreDC(HDC dc)
            : _dc(dc)
            , _save(SaveDC(dc))
        {
        }

        SaveAndRestoreDC(const SaveAndRestoreDC& other) = delete;
        SaveAndRestoreDC& operator=(const SaveAndRestoreDC& other) = delete;

        ~SaveAndRestoreDC()
        {
            RestoreDC(_dc, _save);
        }
    private:
        HDC _dc = 0;
        int _save;
    };

    STATIC_ASSERT_NO_COPY_NO_MOVE(SaveAndRestoreDC)

    class ScopedSelectObject
    {
    public:
        ScopedSelectObject(HDC dc, HGDIOBJ object)
            : _dc(dc)
            , _prev_object(SelectObject(dc, object))
        {
        }

        ScopedSelectObject(const ScopedSelectObject& other) = delete;
        ScopedSelectObject& operator=(const ScopedSelectObject& other) = delete;

        ~ScopedSelectObject()
        {
            SelectObject(_dc, _prev_object);
        }

        HDC _dc = 0;
        HGDIOBJ _prev_object = 0;
    };

    STATIC_ASSERT_NO_COPY_NO_MOVE(ScopedSelectObject)

    template<class HANDLE_T>
    class GdiObj
    {
    public:
        GdiObj() = default;
        GdiObj(HANDLE_T gdiobj)
            : _gdiobj(gdiobj)
        {
        }

        GdiObj(GdiObj&& other)
        {
            _gdiobj = other._gdiobj;
            other._gdiobj = 0;
        }

        GdiObj& operator=(GdiObj&& other)
        {
            if (_gdiobj)
                DeleteObject(_gdiobj);

            _gdiobj = other._gdiobj;
            other._gdiobj = 0;
            return *this;
        }

        ~GdiObj()
        {
            if (_gdiobj)
                DeleteObject(_gdiobj);
        }

        operator HANDLE_T() const
        {
            return _gdiobj;
        }

        HANDLE_T get() const
        {
            return _gdiobj;
        }

    private:
        HANDLE_T _gdiobj = 0;
    };

    typedef GdiObj<HBITMAP> Bitmap;
    typedef GdiObj<HBRUSH> Brush;
    typedef GdiObj<HPEN> Pen;

    STATIC_ASSERT_NO_COPY(Bitmap)
    STATIC_ASSERT_NO_COPY(Brush)
    STATIC_ASSERT_NO_COPY(Pen)

    class Icon
    {
    public:
        Icon() = default;
        Icon(HICON icon)
            : _icon(icon)
        {}

        Icon(Icon&& other)
            : _icon(other._icon)
        {
            other._icon = 0;
        }

        Icon& operator=(Icon&& other)
        {
            if (_icon)
                DestroyIcon(_icon);

            _icon = other._icon;
            other._icon = 0;
            return *this;
        }

        ~Icon()
        {
            if (_icon)
                DestroyIcon(_icon);
        }

        operator HICON() const
        {
            return _icon;
        }

        HICON get() const
        {
            return _icon;
        }

    private:
        HICON _icon = 0;
    };

    STATIC_ASSERT_NO_COPY(Icon)
}
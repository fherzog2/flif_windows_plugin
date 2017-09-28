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

    static_assert(std::is_default_constructible<Window>{}, "default constructor required");
    static_assert(!std::is_copy_constructible<Window>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<Window>{}, "no copy construction allowed");
    static_assert(std::is_move_constructible<Window>{}, "move construction required");
    static_assert(std::is_move_assignable<Window>{}, "move construction required");

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

    static_assert(!std::is_default_constructible<ClientDC>{}, "no default constructor allowed");
    static_assert(!std::is_copy_constructible<ClientDC>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<ClientDC>{}, "no copy construction allowed");
    static_assert(!std::is_move_constructible<ClientDC>{}, "no move construction allowed");
    static_assert(!std::is_move_assignable<ClientDC>{}, "no move construction allowed");

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

    static_assert(!std::is_default_constructible<MemoryDC>{}, "no default constructor allowed");
    static_assert(!std::is_copy_constructible<MemoryDC>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<MemoryDC>{}, "no copy construction allowed");
    static_assert(!std::is_move_constructible<MemoryDC>{}, "no move construction allowed");
    static_assert(!std::is_move_assignable<MemoryDC>{}, "no move construction allowed");

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

    static_assert(!std::is_default_constructible<SaveAndRestoreDC>{}, "no default constructor allowed");
    static_assert(!std::is_copy_constructible<SaveAndRestoreDC>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<SaveAndRestoreDC>{}, "no copy construction allowed");
    static_assert(!std::is_move_constructible<SaveAndRestoreDC>{}, "no move construction allowed");
    static_assert(!std::is_move_assignable<SaveAndRestoreDC>{}, "no move construction allowed");

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

    static_assert(!std::is_default_constructible<ScopedSelectObject>{}, "no default constructor allowed");
    static_assert(!std::is_copy_constructible<ScopedSelectObject>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<ScopedSelectObject>{}, "no copy construction allowed");
    static_assert(!std::is_move_constructible<ScopedSelectObject>{}, "no move construction allowed");
    static_assert(!std::is_move_assignable<ScopedSelectObject>{}, "no move construction allowed");

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

    static_assert(std::is_default_constructible<Bitmap>{}, "default constructor required");
    static_assert(!std::is_copy_constructible<Bitmap>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<Bitmap>{}, "no copy construction allowed");
    static_assert(std::is_move_constructible<Bitmap>{}, "move construction required");
    static_assert(std::is_move_assignable<Bitmap>{}, "move construction required");

    static_assert(std::is_default_constructible<Brush>{}, "default constructor required");
    static_assert(!std::is_copy_constructible<Brush>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<Brush>{}, "no copy construction allowed");
    static_assert(std::is_move_constructible<Brush>{}, "move construction required");
    static_assert(std::is_move_assignable<Brush>{}, "move construction required");

    static_assert(std::is_default_constructible<Pen>{}, "default constructor required");
    static_assert(!std::is_copy_constructible<Pen>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<Pen>{}, "no copy construction allowed");
    static_assert(std::is_move_constructible<Pen>{}, "move construction required");
    static_assert(std::is_move_assignable<Pen>{}, "move construction required");

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

    static_assert(std::is_default_constructible<Icon>{}, "default constructor required");
    static_assert(!std::is_copy_constructible<Icon>{}, "no copy construction allowed");
    static_assert(!std::is_copy_assignable<Icon>{}, "no copy construction allowed");
    static_assert(std::is_move_constructible<Icon>{}, "move construction required");
    static_assert(std::is_move_assignable<Icon>{}, "move construction required");
}
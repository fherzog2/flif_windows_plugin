setlocal

rem Builds all dependencies.
rem Call this script in the library root directory, so cmake can find the dependencies.
rem A directory "3rdparty" is created where all work is done.
rem At the end, all libraries and header files are pasted to "3rdparty\bin".

set zliburl=https://sourceforge.net/projects/libpng/files/zlib/1.2.8/zlib128.zip/download
set zlibzip=zlib128.zip
set zlibdir=zlib-1.2.8

set lpngurl=https://sourceforge.net/projects/libpng/files/libpng16/1.6.22/lpng1622.zip/download
set lpngzip=png1622.zip
set lpngdir=lpng1622

set flifurl=https://github.com/FLIF-hub/FLIF/archive/1f20c0ebeda52d6f24b0ba23725e8bcc5bc8336a.zip
set flifzip=FLIF-1f20c0ebeda52d6f24b0ba23725e8bcc5bc8336a.zip
set flifdir=FLIF-1f20c0ebeda52d6f24b0ba23725e8bcc5bc8336a

set BUILD_CFG=Release

if not exist "3rdparty" (
    mkdir 3rdparty
)

cd 3rdparty

if not exist bin (
    mkdir bin
)

rem Clear previous build result
del /q bin\*

rem Allow libraries find previously build libraries
set CMAKE_INCLUDE_PATH=%cd%\bin
set CMAKE_LIBRARY_PATH=%cd%\bin



rem building zlib
if not exist "%zlibzip%" ( 
	echo download %zlibzip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%zliburl%', '%zlibzip%')"
)
if not exist "%zlibdir%\\" ( 
	echo unzip %zlibzip%
	PowerShell "$s=New-Object -Com shell.application;$s.NameSpace('%CD%').CopyHere($s.NameSpace('%CD%\%zlibzip%').Items())"
)
if not exist build.zlib (
    mkdir build.zlib
    cd build.zlib
    cmake ..\%zlibdir% -G "Visual Studio 14 2015 Win64"
    cd ..
)
msbuild build.zlib\zlib.sln /p:Configuration=%BUILD_CFG%

copy %zlibdir%\zlib.h bin\zlib.h
copy build.zlib\zconf.h bin\zconf.h
copy build.zlib\%BUILD_CFG%\zlibstatic.lib bin\zlib.lib



rem building libpng
if not exist "%lpngzip%" ( 
	echo download %lpngzip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%lpngurl%', '%lpngzip%')"
)
if not exist "%lpngdir%\\" ( 
	echo unzip %lpngzip%
	PowerShell "$s=New-Object -Com shell.application;$s.NameSpace('%CD%').CopyHere($s.NameSpace('%CD%\%lpngzip%').Items())"
)
if not exist build.png (
    mkdir build.png
    cd build.png
    cmake ..\%lpngdir% -G "Visual Studio 14 2015 Win64"
    cd ..
)
msbuild build.png\libpng.sln /p:Configuration=%BUILD_CFG%

copy %lpngdir%\png.h bin\png.h
copy %lpngdir%\pngconf.h bin\pngconf.h
copy build.png\pnglibconf.h bin\pnglibconf.h
copy build.png\%BUILD_CFG%\libpng16_static.lib bin\libpng16.lib



rem building FLIF
if not exist "%flifzip%" ( 
	echo download %flifzip%
	PowerShell "(New-Object System.Net.WebClient).DownloadFile('%flifurl%', '%flifzip%')"
)
if not exist "%flifdir%\\" ( 
	echo unzip %flifzip%
	PowerShell "$s=New-Object -Com shell.application;$s.NameSpace('%CD%').CopyHere($s.NameSpace('%CD%\%flifzip%').Items())"
)
if not exist build.flif (
    mkdir build.flif
    cd build.flif
    cmake ..\%flifdir%\src -G "Visual Studio 14 2015 Win64"
    cd ..
)
msbuild build.flif\flif.sln /p:Configuration=%BUILD_CFG% /t:flif_lib

robocopy build.flif\%BUILD_CFG% bin
robocopy %flifdir%\src\library bin *.h
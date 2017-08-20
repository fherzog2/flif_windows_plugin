setlocal

rem Builds all dependencies.
rem Call this script in the library root directory, so cmake can find the dependencies.
rem A directory "3rdparty" is created where all work is done.
rem At the end, all libraries and header files are pasted to "3rdparty\bin".

set zliburl=https://github.com/madler/zlib/archive/v1.2.11.zip
set zlibzip=v1.2.11.zip
set zlibdir=zlib-1.2.11

set lpngurl=https://github.com/glennrp/libpng/archive/v1.6.28.zip
set lpngzip=v1.6.28.zip
set lpngdir=libpng-1.6.28

set flifurl=https://github.com/FLIF-hub/FLIF/archive/fe2c1558a7245d9efaafd082f6237e9e06b72e2d.zip
set flifzip=FLIF-fe2c1558a7245d9efaafd082f6237e9e06b72e2d.zip
set flifdir=FLIF-fe2c1558a7245d9efaafd082f6237e9e06b72e2d

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
    call :cmake_init_with_static_runtime ..\%zlibdir%
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
    call :cmake_init_with_static_runtime ..\%lpngdir%
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
    call :cmake_init_with_static_runtime ..\%flifdir%\src
    cd ..
)
msbuild build.flif\flif.sln /p:Configuration=%BUILD_CFG% /t:flif_lib

robocopy build.flif\%BUILD_CFG% bin
robocopy %flifdir%\src\library bin *.h

exit /b 0

rem This is a trick to compile all dependencies with static runtime.
rem Init the build dir, rewrite compiler flags in the cache, then run init again.
:cmake_init_with_static_runtime
cmake %1 -G "Visual Studio 14 2015 Win64"
PowerShell "$content = [System.IO.File]::ReadAllText('CMakeCache.txt').Replace('/MD ', '/MT '); [System.IO.File]::WriteAllText('CMakeCache.txt', $content)"
cmake %1 -G "Visual Studio 14 2015 Win64"
exit /b 0
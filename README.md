# FLIF Windows Plugin
This plugin enables decoding of FLIF images in applications which use the Windows Imaging Component API. In this way, FLIF images can be viewed in Windows Explorer like other common image formats.

## Installation

The latest installer is provided under "Releases". Please note, a restart may be required before Explorer thumbnails are loaded for .flif files.

## Build instructions
* open a commandline (Visual Studio 2015 and CMake must be in the PATH)
* navigate to the project root dir
* building the dependencies
  * `build_3rdparty.cmd`
* building the library for the first time
  * `mkdir build`
  * `cd build`
  * `cmake .. -G "Visual Studio 14 2015 Win64"`
  * `cmake --build . --config release`
* building the installer ([WiX Toolset](http://wixtoolset.org/) must be in the PATH)
  * `cd setup`
  * `make_installer.cmd`

See also: [https://github.com/FLIF-hub/FLIF](https://github.com/FLIF-hub/FLIF)

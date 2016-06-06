cmake_minimum_required(VERSION 2.8.12)
project(flif_windows_plugin)

set(CMAKE_CXX_FLAGS_RELEASE "/MT /O2 /Ob2 /D NDEBUG")

set(_FLIF_SEARCHES)

if(FLIF_ROOT)
  set(_FLIF_SEARCH_ROOT PATHS ${FLIF_ROOT} NO_DEFAULT_PATH)
  list(APPEND _FLIF_SEARCHES _FLIF_SEARCH_ROOT)
endif()

set(_FLIF_SEARCH_NORMAL
    PATHS "${CMAKE_SOURCE_DIR}/3rdparty/bin"
)
list(APPEND _FLIF_SEARCHES _FLIF_SEARCH_NORMAL)

# Try each search configuration.
foreach(search ${_FLIF_SEARCHES})
  find_path(FLIF_INCLUDE_DIR NAME flif.h ${${search}} PATH_SUFFIXES bin)
endforeach()

# Allow FLIF_LIBRARY to be set manually, as the location of the flif library
if(NOT FLIF_LIBRARY)
  foreach(search ${_FLIF_SEARCHES})
    find_library(FLIF_LIBRARY NAMES flif ${${search}} PATH_SUFFIXES bin)
  endforeach()
endif()

set(SRC_FILES src/flifBitmapDecoder.cpp
              src/dll_interface.cpp
              src/RegistryManager.cpp
              src/flif_windows_plugin.rc
              src/plugin_export.def)

add_library(flif_windows_plugin SHARED ${SRC_FILES})
target_link_libraries(flif_windows_plugin ${FLIF_LIBRARY} Windowscodecs)
include_directories(flif_windows_plugin ${FLIF_INCLUDE_DIR})

# test

add_executable(test1 src/test/test.cpp)
target_link_libraries(test1 flif_windows_plugin Shlwapi)
target_include_directories(test1 PRIVATE "3rdparty/bin" "src")

enable_testing()
add_test(NAME test1 COMMAND test1 ${CMAKE_SOURCE_DIR}/src/test/flif.flif)
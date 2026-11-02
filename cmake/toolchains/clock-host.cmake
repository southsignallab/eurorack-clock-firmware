# CLOCK Windows host builds use MSYS2 UCRT64 GCC, not ambient legacy cl.exe.
# Author: Axel Napolitano
# License: PolyForm-Noncommercial-1.0.0
# No effect on Linux/macOS. Explicitly selected compilers are preserved.
if(NOT CMAKE_HOST_WIN32)
    return()
endif()

# CMake reuses compiler paths from existing build caches even when PATH changes.
# Fail rather than silently running a compiler from a stale Visual Studio cache.
if(DEFINED CMAKE_CXX_COMPILER)
    file(TO_CMAKE_PATH "${CMAKE_CXX_COMPILER}" _clock_existing_cxx)
    if(_clock_existing_cxx MATCHES "/cl\\.exe$")
        message(FATAL_ERROR
            "CLOCK simulator is configured with MSVC (${CMAKE_CXX_COMPILER}). "
            "Use 'python scripts/configure_simulator.py --build' to reset a stale "
            "Windows CMake cache and select MSYS2 UCRT64 GCC.")
    endif()
endif()

if(DEFINED CMAKE_C_COMPILER AND DEFINED CMAKE_CXX_COMPILER)
    return()
endif()

if(DEFINED ENV{CLOCK_UCRT64_ROOT} AND NOT "$ENV{CLOCK_UCRT64_ROOT}" STREQUAL "")
    set(_clock_ucrt64_root "$ENV{CLOCK_UCRT64_ROOT}")
elseif(DEFINED ENV{MSYSTEM_PREFIX} AND "$ENV{MSYSTEM_PREFIX}" MATCHES "[/\\]ucrt64$")
    set(_clock_ucrt64_root "$ENV{MSYSTEM_PREFIX}")
else()
    set(_clock_ucrt64_root "C:/msys64/ucrt64")
endif()

find_program(_CLOCK_UCRT64_GCC NAMES gcc.exe PATHS "${_clock_ucrt64_root}/bin" NO_DEFAULT_PATH)
find_program(_CLOCK_UCRT64_GXX NAMES g++.exe PATHS "${_clock_ucrt64_root}/bin" NO_DEFAULT_PATH)
if(NOT _CLOCK_UCRT64_GCC OR NOT _CLOCK_UCRT64_GXX)
    message(FATAL_ERROR
        "CLOCK Windows simulator requires MSYS2 UCRT64 GCC/G++ in "
        "${_clock_ucrt64_root}/bin. Set CLOCK_UCRT64_ROOT, or run "
        "'python scripts/configure_simulator.py --build'.")
endif()

if(NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER "${_CLOCK_UCRT64_GCC}" CACHE FILEPATH "CLOCK Windows host C compiler")
endif()
if(NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER "${_CLOCK_UCRT64_GXX}" CACHE FILEPATH "CLOCK Windows host C++ compiler")
endif()

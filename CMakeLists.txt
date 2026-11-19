cmake_minimum_required(VERSION 3.24)

# Read default version from VERSION file, overridable via -DTRAMI_VERSION=x.y.z
if(NOT DEFINED TRAMI_VERSION OR TRAMI_VERSION STREQUAL "")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _trami_version_lines LIMIT_COUNT 1)
    list(GET _trami_version_lines 0 TRAMI_VERSION)
    string(STRIP "${TRAMI_VERSION}" TRAMI_VERSION)
endif()
string(REGEX REPLACE "^v" "" TRAMI_VERSION "${TRAMI_VERSION}")

project(TramiLoop VERSION ${TRAMI_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

if(WIN32)
    # Link the MSVC C++ runtime statically (/MT in Release, /MTd in Debug) so trami-loop.exe
    # is 100% standalone with no external vcruntime140.dll / msvcp140.dll dependency.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

option(TRAMI_BUILD_TESTS "Build unit tests" ON)

include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(TRAMI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

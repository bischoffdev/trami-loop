cmake_minimum_required(VERSION 3.24)

project(TramiLoop VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()

if(WIN32)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
endif()

option(TRAMI_BUILD_TESTS "Build unit tests" ON)

include(cmake/Dependencies.cmake)

add_subdirectory(src)

if(TRAMI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

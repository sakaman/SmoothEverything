cmake_minimum_required(VERSION 3.25)

set(SE_VERSION "0.1.1" CACHE STRING "SmoothEverything release version")
if(NOT SE_VERSION MATCHES "^[0-9]+[.][0-9]+[.][0-9]+$")
    message(FATAL_ERROR "SE_VERSION must use numeric major.minor.patch format")
endif()

project(
    SmoothEverything
    VERSION "${SE_VERSION}"
    DESCRIPTION "Native Windows mouse-wheel smoothing"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SE_BUILD_ENGINE "Build the Win32 background engine" ON)
option(SE_BUILD_TESTS "Build deterministic unit tests" ON)

add_subdirectory(src/core)

if(SE_BUILD_ENGINE AND WIN32)
    add_subdirectory(src/engine)
endif()

if(SE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

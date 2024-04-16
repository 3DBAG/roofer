cmake_minimum_required(VERSION 3.22)

project(RooferLogger VERSION 1.1.42 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(USE_LOGGER_SPDLOG "Whether to use spdlog as logging backend." OFF)
option(USE_LOGGER_RERUN "Whether to use rerun as logging backend." OFF)

# Project/Library Names
set(EXECUTABLE_NAME "RooferExe")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")
include(FetchContent)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt
    GIT_TAG 10.2.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(fmt)

if(USE_LOGGER_SPDLOG)
    message(STATUS "Logging backend: spdlog")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog
        GIT_TAG v1.13.0
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(spdlog)
elseif(USE_LOGGER_RERUN)
    message(STATUS "Logging backend: rerun")
    FetchContent_Declare(
        rerun_sdk
        URL https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
    FetchContent_MakeAvailable(rerun_sdk)
else()
    message(STATUS "Logging backend: internal")
endif()

add_subdirectory(configured)
add_subdirectory(src)
add_subdirectory(app)

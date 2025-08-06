# Require at least CMake 3.25, because in 3.24 and 3.25 there were a couple of
# FetchContent-related changes
cmake_minimum_required(VERSION 3.25)

# Options
option(RF_USE_LOGGER_SPDLOG "Use spdlog as logging backend." OFF)
option(RF_USE_RERUN "Enable features dependent on Rerun" OFF)
option(RF_USE_VAL3DITY "Enable features dependent on Val3dity" OFF)
option(RF_BUILD_APPS "Build applications in addition to library" ON)
option(RF_BUILD_DOC_HELPER "Build documentation generation helper app" OFF)
option(RF_BUILD_BINDINGS "Build python bindings with pybind" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries (may not work)" OFF)
option(RF_BUILD_TESTING "Enable tests for roofer" OFF)
option(RF_ENABLE_HEAP_TRACING "Enable heap allocation overloads" OFF)

# Enable the vcpkg features that are required by the options
if(RF_USE_LOGGER_SPDLOG)
  list(APPEND VCPKG_MANIFEST_FEATURES "spdlog")
endif()
if(RF_BUILD_TESTING)
  list(APPEND VCPKG_MANIFEST_FEATURES "test")
endif()
if(RF_BUILD_APPS)
  list(APPEND VCPKG_MANIFEST_FEATURES "apps")
endif()
if(RF_USE_VAL3DITY)
  list(APPEND VCPKG_MANIFEST_FEATURES "val3dity")
endif()
if(RF_USE_RERUN)
  list(APPEND VCPKG_MANIFEST_FEATURES "app-rerun")
endif()
# if(RF_BUILD_BINDINGS)
#   list(APPEND VCPKG_MANIFEST_FEATURES "python")
# endif()

# TODO: add version number
project(roofer VERSION 1.0.0 LANGUAGES C CXX)

# Global CMake variables are set here We use C++20, with the assumption that we
# only implement features that are supported by GCC, Clang, MSVC, Apple Clang
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use extensions, because they might affect compiler compatibility
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# Enable position independent code for shared libraries in case we build them
if(BUILD_SHARED_LIBS OR RF_BUILD_BINDINGS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# CMAKE MODULES

# CMake modules, like the documentation module, go in here
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# EXTERNAL LIBRARIES

find_package(fmt REQUIRED)
find_package(
  CGAL 6.0 QUIET
  COMPONENTS Core
  REQUIRED)

# Make sure CGAL is thread-safe
add_compile_definitions("CGAL_HAS_THREADS=1")

# For arm64 set FPU rounding mode to prevent hanging in multi-threaded
# environments (this occurred during testing on Apple Silicon)
MESSAGE("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
  add_compile_definitions("CGAL_ALWAYS_ROUND_TO_NEAREST=1")
endif()

if(RF_USE_LOGGER_SPDLOG)
  message(STATUS "Logging backend: spdlog")
  set(SPDLOG_FMT_EXTERNAL ON)
  find_package(spdlog REQUIRED)
else()
  message(STATUS "Logging backend: internal")
endif()

# We have to use CPM (or FetchContent) even with vcpkg, because of
# cmake-git-version-tracking, val3dity, rerun
include(CPM)
cpmaddpackage(
  "gh:andrew-hardin/cmake-git-version-tracking#6c0cb87edd029ddfb403a8e24577c144a03605a6"
)
set(GIT_IGNORE_UNTRACKED TRUE)

if(RF_USE_RERUN)
  set(RERUN_DOWNLOAD_AND_BUILD_ARROW OFF)
  cpmaddpackage(
    NAME rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip
  )
endif()

if(MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(ROOFER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(src)
if(RF_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# Python binding
if(RF_BUILD_BINDINGS)
  add_subdirectory(rooferpy)
endif()

# EXECUTABLES
if(RF_BUILD_APPS OR RF_BUILD_DOC_HELPER)
  add_subdirectory(apps)
endif()

# Require at least CMake 3.25, because in 3.24 and 3.25 there were a couple of
# FetchContent-related changes
cmake_minimum_required(VERSION 3.25)

# TODO: add version number
project(roofer LANGUAGES C CXX)

# Global CMake variables are set here We use C++20, with the assumption that we
# only implement features that are supported by GCC, Clang, MSVC, Apple Clang
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use extensions, because they might affect compiler compatibility
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(RF_ENABLE_TESTING "Enable a Unit Testing build." OFF)
option(RF_USE_LOGGER_SPDLOG "Whether to use spdlog as logging backend." OFF)
option(RF_USE_RERUN "Enable features dependent on Rerun" OFF)
option(RF_USE_VAL3DITY "Enable features dependent on Val3dity" OFF)
option(RF_BUILD_APPS "Build applications in addition to library" ON)

# CMAKE MODULES

# CMake modules, like the documentation module, go in here
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# EXTERNAL LIBRARIES

find_package(fmt REQUIRED)
find_package(
  CGAL 5.4 QUIET
  COMPONENTS Core
  REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GEOS CONFIG REQUIRED)
find_package(laslib CONFIG REQUIRED)

# GDAL
find_package(GDAL CONFIG REQUIRED)
if(WIN32) # vcpkg doesn't work when there is a version number here
  find_package(PROJ REQUIRED CONFIG)
else()
  find_package(PROJ 9.0.0 REQUIRED CONFIG)
endif()

if(RF_USE_LOGGER_SPDLOG)
  message(STATUS "Logging backend: spdlog")
  set(SPDLOG_FMT_EXTERNAL ON)
  find_package(spdlog REQUIRED)
else()
  message(STATUS "Logging backend: internal")
endif()

if(RF_ENABLE_TESTING)
  find_package(Catch2 REQUIRED)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()

# We have to use CPM (or FetchContent) even with vcpkg, because of
# cmake-git-version-tracking, val3dity, rerun
include(CPM)
cpmaddpackage(
  "gh:andrew-hardin/cmake-git-version-tracking#6c0cb87edd029ddfb403a8e24577c144a03605a6"
)
set(GIT_IGNORE_UNTRACKED TRUE)

if(RF_USE_VAL3DITY)
  cpmaddpackage("gh:Ylannl/val3dity#gfp")
endif()

if(RF_USE_RERUN)
  cpmaddpackage(
    NAME rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
endif()

if(MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
endif()

# Use configure files to configure compile-time variable for using them runtime,
# e.g. version numbers and feature flags
set(ROOFER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(configured)
add_subdirectory(src)
if(RF_ENABLE_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()

# INSTALL TARGETS
if(BUILD_APPS)
  add_subdirectory(apps)
  install(
    TARGETS "crop"
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)

  install(
    TARGETS "reconstruct"
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
endif()
# Require at least CMake 3.25, because in 3.24 and 3.25 there were a couple of
# FetchContent-related changes
cmake_minimum_required(VERSION 3.25)

# Specify the language too, just to be neat
project(
  "ParallelReconstruct"
  VERSION 1.0.0
  LANGUAGES CXX)

# Global CMake variables are set here We use C++20, with the assumption that we
# only implement features that are supported by GCC, Clang, MSVC, Apple Clang
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use extensions, because they might affect compiler compatibility
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(USE_VCPKG "Whether to use VCPKG." OFF)
option(ENABLE_TESTING "Enable a Unit Testing build." OFF)
option(USE_LOGGER_SPDLOG "Whether to use spdlog as logging backend." OFF)

# CMAKE MODULES

# CMake modules, like the documentation module, go in here
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# EXTERNAL LIBRARIES

if(USE_VCPKG)
  message(STATUS "Using VCPKG package manager")
  include(${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake)
  find_package(nlohmann_json REQUIRED)
  if(ENABLE_TESTING)
    find_package(Catch2 REQUIRED)
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  endif()
else()
  message(STATUS "Using CPM package manager")
  include(CPM)
  cpmaddpackage("gh:nlohmann/json#v3.11.3")
  cpmaddpackage("gh:gabime/spdlog#v1.14.1")
  if(ENABLE_TESTING)
    cpmaddpackage("gh:catchorg/Catch2#v3.5.3")
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
  endif()
endif()

# Use configure files to configure compile-time variable for using them runtime,
# e.g. version numbers and feature flags
add_subdirectory(configured)
add_subdirectory(src)
if(ENABLE_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
add_subdirectory(app)

# INSTALL TARGETS

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

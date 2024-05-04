# Require at least CMake 3.25, because in 3.24 and 3.25 there were a couple of
# FetchContent-related changes
cmake_minimum_required(VERSION 3.25)

project (roofer LANGUAGES C CXX)

# Global CMake variables are set here We use C++20, with the assumption that we
# only implement features that are supported by GCC, Clang, MSVC, Apple Clang
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use extensions, because they might affect compiler compatibility
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(ENABLE_TESTING "Enable a Unit Testing build." OFF)
option(USE_LOGGER_SPDLOG "Whether to use spdlog as logging backend." OFF)

# CMAKE MODULES

# CMake modules, like the documentation module, go in here
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

## Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
#if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
#    cmake_policy(SET CMP0135 NEW)
#endif()

# EXTERNAL LIBRARIES

message(STATUS "Using VCPKG package manager")
include(${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake)
find_package(nlohmann_json REQUIRED)
find_package(fmt REQUIRED)
find_package(GEOS CONFIG REQUIRED)
find_package(laslib CONFIG REQUIRED)
find_package(CGAL 5.4 QUIET COMPONENTS Core REQUIRED)
# GDAL
find_package( GDAL CONFIG REQUIRED )
if (WIN32) # vcpkg doesn't work when there is a version number here
  find_package(PROJ REQUIRED CONFIG)
else()
  find_package(PROJ 9.0.0 REQUIRED CONFIG)
endif()
if(USE_LOGGER_SPDLOG)
  message(STATUS "Logging backend: spdlog")
  find_package(spdlog REQUIRED)
else()
    message(STATUS "Logging backend: internal")
endif()
if(ENABLE_TESTING)
  find_package(Catch2 REQUIRED)
  list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
endif()

# We have to use CPM (or FetchContent) even with vcpkg, because of
# cmake-git-version-tracking and val3dity
include(CPM)
CPMAddPackage("gh:andrew-hardin/cmake-git-version-tracking#6c0cb87edd029ddfb403a8e24577c144a03605a6")
set(GIT_IGNORE_UNTRACKED TRUE)
CPMAddPackage("gh:Ylannl/val3dity#gfp")

# ptinpoly
add_library(ptinpoly STATIC external/ptinpoly/ptinpoly.c)
set_target_properties(
  ptinpoly PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

if (MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
endif()

#add_library(roofer
#  src/common/common.cpp
#  src/io/VectorReaderOGR.cpp
#  src/io/VectorWriterOGR.cpp
#  src/io/RasterWriterGDAL.cpp
#  src/io/StreamCropper.cpp
#  src/io/PointCloudWriterLASlib.cpp
#  src/io/PointCloudReaderLASlib.cpp
#  src/io/CityJsonWriter.cpp
#  src/datastructures/Raster.cpp
#  src/misc/pip_util.cpp
#  src/misc/projHelper.cpp
#  src/misc/PC2MeshDistCalculator.cpp
#  src/misc/Val3dator.cpp
#  src/misc/Vector2DOpsGEOS.cpp
#  src/misc/NodataCircleComputer.cpp
#  src/misc/PointcloudRasteriser.cpp
#  src/misc/select_pointcloud.cpp
#  src/reconstruction/PlaneDetector.cpp
#  src/reconstruction/AlphaShaper.cpp
#  src/reconstruction/LineDetector.cpp
#  src/reconstruction/LineDetectorBase.cpp
#  src/reconstruction/LineRegulariserBase.cpp
#  src/reconstruction/LineRegulariser.cpp
#  src/reconstruction/PlaneIntersector.cpp
#  src/reconstruction/SegmentRasteriser.cpp
#  src/reconstruction/ArrangementBase.cpp
#  src/reconstruction/ArrangementBuilder.cpp
#  src/reconstruction/ArrangementOptimiser.cpp
#  src/reconstruction/ArrangementDissolver.cpp
#  src/reconstruction/ArrangementSnapper.cpp
#  src/reconstruction/ArrangementExtruder.cpp
#  src/reconstruction/cdt_util.cpp
#  src/reconstruction/MeshTriangulatorLegacy.cpp
#  src/reconstruction/SimplePolygonExtruder.cpp
#)
#set_target_properties(
#  roofer PROPERTIES
#  CXX_STANDARD 20
#)
#
#target_include_directories(roofer PRIVATE
#  src
#  external/ptinpoly
#)
#target_link_libraries( roofer
#  PRIVATE
#    PROJ::proj
#    GDAL::GDAL
#    ptinpoly
#    CGAL::CGAL CGAL::CGAL_Core #Eigen3::Eigen
#    LASlib
#    GEOS::geos_c
#    val3dity
#  PUBLIC
#    fmt::fmt
#)

# Use configure files to configure compile-time variable for using them runtime,
# e.g. version numbers and feature flags
add_subdirectory(configured)
add_subdirectory(src)
if(ENABLE_TESTING)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
add_subdirectory(apps)

# INSTALL TARGETS
install(
    TARGETS "crop"
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
cmake_minimum_required(VERSION 3.21)
project (roofer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
# option(RF_BUILD_GUI "Build the GUI apps" OFF)
option(USE_LOGGER_SPDLOG "Whether to use spdlog as logging backend." OFF)

include(FetchContent)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(cmake_git_version_tracking
  GIT_REPOSITORY https://github.com/andrew-hardin/cmake-git-version-tracking.git
  GIT_TAG 904dbda1336ba4b9a1415a68d5f203f576b696bb
)
set(GIT_IGNORE_UNTRACKED TRUE)
FetchContent_MakeAvailable(cmake_git_version_tracking)

# ptinpoly
add_library(ptinpoly STATIC external/ptinpoly/ptinpoly.c)
set_target_properties(
  ptinpoly PROPERTIES
  C_STANDARD 11
  POSITION_INDEPENDENT_CODE ON
)

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
else()
    message(STATUS "Logging backend: internal")
endif()

find_package(GEOS CONFIG REQUIRED)

find_package(laslib CONFIG REQUIRED)
# find_package(Eigen3 CONFIG REQUIRED)
# add_definitions(-DCGAL_EIGEN3_ENABLED)
find_package(CGAL 5.4 QUIET COMPONENTS Core REQUIRED)

# GDAL
find_package( GDAL CONFIG REQUIRED )
if (WIN32) # vcpkg doesn't work when there is a version number here
  find_package(PROJ REQUIRED CONFIG)
else()
  find_package(PROJ 9.0.0 REQUIRED CONFIG)
endif()

find_package( nlohmann_json 3.10.5 CONFIG REQUIRED )
add_subdirectory(external/val3dity)

if (MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
endif()

add_library(roofer
  src/common.cpp
  src/misc/projHelper.cpp
  src/io/VectorReaderOGR.cpp
  src/io/VectorWriterOGR.cpp
  src/io/RasterWriterGDAL.cpp
  src/io/StreamCropper.cpp
  src/io/PointCloudWriterLASlib.cpp
  src/io/PointCloudReaderLASlib.cpp
  src/io/CityJsonWriter.cpp
  src/datastructures/Raster.cpp
  src/misc/pip_util.cpp
  src/reconstruction/PlaneDetector.cpp
  src/reconstruction/AlphaShaper.cpp
  src/reconstruction/LineDetector.cpp
  src/reconstruction/LineDetectorBase.cpp
  src/reconstruction/LineRegulariserBase.cpp
  src/reconstruction/LineRegulariser.cpp
  src/reconstruction/PlaneIntersector.cpp
  src/reconstruction/SegmentRasteriser.cpp
  src/reconstruction/ArrangementBase.cpp
  src/reconstruction/ArrangementBuilder.cpp
  src/reconstruction/ArrangementOptimiser.cpp
  src/reconstruction/ArrangementDissolver.cpp
  src/reconstruction/ArrangementSnapper.cpp
  src/reconstruction/ArrangementExtruder.cpp
  src/reconstruction/cdt_util.cpp
  src/reconstruction/MeshTriangulatorLegacy.cpp
  src/reconstruction/SimplePolygonExtruder.cpp
  src/misc/PC2MeshDistCalculator.cpp
  src/misc/Val3dator.cpp
  src/misc/Vector2DOpsGEOS.cpp
  src/misc/NodataCircleComputer.cpp
  src/misc/PointcloudRasteriser.cpp
  src/misc/select_pointcloud.cpp
)
set_target_properties(
  roofer PROPERTIES
  CXX_STANDARD 20
)

target_include_directories(roofer PRIVATE
  src
  external/ptinpoly
)
target_link_libraries( roofer
  PRIVATE
    PROJ::proj
    GDAL::GDAL
    ptinpoly
    CGAL::CGAL CGAL::CGAL_Core #Eigen3::Eigen
    LASlib
    GEOS::geos_c
    val3dity
  PUBLIC
    fmt::fmt
)

add_subdirectory(configured)
add_subdirectory(src)
add_subdirectory(apps)
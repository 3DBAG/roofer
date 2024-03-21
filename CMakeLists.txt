cmake_minimum_required(VERSION 3.21)
project (roofer)

option(RF_BUILD_GUI "Build the GUI apps" OFF)

include(FetchContent)
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

#spdlog
add_subdirectory(external/fmt EXCLUDE_FROM_ALL)
set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(external/spdlog)

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

if (MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
endif()

add_library(rooferlib
  src/common.cpp
  src/projHelper.cpp
  src/io/VectorReaderOGR.cpp
  src/io/VectorWriterOGR.cpp
  src/io/RasterWriterGDAL.cpp
  src/io/StreamCropper.cpp
  src/io/PointCloudWriterLASlib.cpp
  src/io/PointCloudReaderLASlib.cpp
  src/geometry/Raster.cpp
  src/pip_util.cpp
  src/detection/PlaneDetector.cpp
  src/detection/AlphaShaper.cpp
  src/detection/LineDetector.cpp
  src/detection/LineDetectorBase.cpp
  src/detection/LineRegulariserBase.cpp
  src/detection/LineRegulariser.cpp
  src/detection/PlaneIntersector.cpp
  src/detection/SegmentRasteriser.cpp
  src/partitioning/ArrangementBase.cpp
  src/partitioning/ArrangementBuilder.cpp
  src/partitioning/ArrangementOptimiser.cpp
  src/partitioning/ArrangementDissolver.cpp
  src/partitioning/ArrangementSnapper.cpp
  src/partitioning/cdt_util.cpp
  src/geometry/Vector2DOpsGEOS.cpp
  src/geometry/NodataCircleComputer.cpp
  src/geometry/PointcloudRasteriser.cpp
  src/quality/select_pointcloud.cpp
)
set_target_properties(
  rooferlib PROPERTIES 
  CXX_STANDARD 17
)

target_include_directories(rooferlib PRIVATE
  src
  external/ptinpoly
)
target_link_libraries( rooferlib 
  PRIVATE
    PROJ::proj
    GDAL::GDAL
    ptinpoly
    CGAL::CGAL CGAL::CGAL_Core #Eigen3::Eigen
    LASlib
    GEOS::geos_c
  PUBLIC
    fmt::fmt
    spdlog::spdlog
)

add_subdirectory(apps)
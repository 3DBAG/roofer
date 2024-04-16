cmake_minimum_required(VERSION 3.16)
project (roofer_api)

set( CMAKE_CXX_FLAGS "-O2" )

find_package(CGAL 5.4 QUIET COMPONENTS Core REQUIRED)

if (MSVC)
    # windows.h breaks std::min/std::max, fix by define
    add_definitions(-DNOMINMAX)
    # enable permissive compiling and/or statements
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
endif()


add_library(roofer_api STATIC
        common.cpp
        datastructures/Raster.cpp
        reconstruction/PlaneDetector.cpp
        reconstruction/AlphaShaper.cpp
        reconstruction/LineDetector.cpp
        reconstruction/LineDetectorBase.cpp
        reconstruction/LineRegulariserBase.cpp
        reconstruction/LineRegulariser.cpp
        reconstruction/PlaneIntersector.cpp
        reconstruction/SegmentRasteriser.cpp
        reconstruction/ArrangementBase.cpp
        reconstruction/ArrangementBuilder.cpp
        reconstruction/ArrangementOptimiser.cpp
        reconstruction/ArrangementDissolver.cpp
        reconstruction/ArrangementSnapper.cpp
        reconstruction/ArrangementExtruder.cpp
        reconstruction/ElevationProvider.cpp
        reconstruction/cdt_util.cpp
        reconstruction/MeshTriangulatorLegacy.cpp
        reconstruction/SimplePolygonExtruder.cpp
)

set_target_properties(
        roofer_api
        PROPERTIES CXX_STANDARD 17
)

target_link_libraries(roofer_api PRIVATE CGAL::CGAL)

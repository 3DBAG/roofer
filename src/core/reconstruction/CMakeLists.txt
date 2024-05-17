set(LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/AlphaShaper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementBase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementBuilder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementDissolver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementExtruder.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementOptimiser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementSnapper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdt_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineDetector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineDetectorBase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineRegulariser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineRegulariserBase.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshTriangulatorLegacy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaneDetector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaneIntersector.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SegmentRasteriser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SimplePolygonExtruder.cpp")
set(LIBRARY_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/AlphaShaper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementBuilder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementDissolver.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementExtruder.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementOptimiser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ArrangementSnapper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdt_util.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cgal_shared_definitions.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineDetector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineDetectorBase.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineRegulariser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/LineRegulariserBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/MeshTriangulator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaneDetector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaneDetectorBase.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PlaneIntersector.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RegionGrower.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RegionGrower_DS_CGAL.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SegmentRasteriser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/SimplePolygonExtruder.hpp")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("reconstruction" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("reconstruction" PUBLIC ${LIBRARY_INCLUDES})

target_link_libraries(
  "reconstruction" PRIVATE "common" CGAL::CGAL
                           CGAL::CGAL_Core # Eigen3::Eigen
)

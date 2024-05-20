set(LIBRARY_SOURCES
    "AlphaShaper.cpp"
    "ArrangementBase.cpp"
    "ArrangementBuilder.cpp"
    "ArrangementDissolver.cpp"
    "ArrangementExtruder.cpp"
    "ArrangementOptimiser.cpp"
    "ArrangementSnapper.cpp"
    "cdt_util.cpp"
    "LineDetector.cpp"
    "LineDetectorBase.cpp"
    "LineRegulariser.cpp"
    "LineRegulariserBase.cpp"
    "MeshTriangulatorLegacy.cpp"
    "PlaneDetector.cpp"
    "PlaneIntersector.cpp"
    "SegmentRasteriser.cpp"
    "SimplePolygonExtruder.cpp")
set(LIBRARY_HEADERS
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/AlphaShaper.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementBase.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementBuilder.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementDissolver.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementExtruder.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementOptimiser.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/ArrangementSnapper.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/cdt_util.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/cgal_shared_definitions.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/LineDetector.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/LineDetectorBase.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/LineRegulariser.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/LineRegulariserBase.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/MeshTriangulator.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/PlaneDetector.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/PlaneDetectorBase.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/PlaneIntersector.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/RegionGrower.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/RegionGrower_DS_CGAL.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/SegmentRasteriser.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/reconstruction/SimplePolygonExtruder.hpp")
set(LIBRARY_INCLUDES "${ROOFER_INCLUDE_DIR}")

add_library("reconstruction" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("reconstruction" PUBLIC ${LIBRARY_INCLUDES})

target_link_libraries(
  "reconstruction" PUBLIC CGAL::CGAL CGAL::CGAL_Core # Eigen3::Eigen
)

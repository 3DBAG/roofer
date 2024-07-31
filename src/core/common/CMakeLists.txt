set(LIBRARY_SOURCES "Raster.cpp"
                    "GridPIPTester.cpp"
                    "common.cpp")
set(LIBRARY_HEADERS "${ROOFER_INCLUDE_DIR}/roofer/common/Raster.hpp"
                    "${ROOFER_INCLUDE_DIR}/roofer/common/datastructures.hpp"
                    "${ROOFER_INCLUDE_DIR}/roofer/common/ptinpoly.h"
                    "${ROOFER_INCLUDE_DIR}/roofer/common/GridPIPTester.hpp"
                    "${ROOFER_INCLUDE_DIR}/roofer/common/box.hpp"
                    "${ROOFER_INCLUDE_DIR}/roofer/common/common.hpp")
set(LIBRARY_INCLUDES  "${ROOFER_INCLUDE_DIR}")

add_library("ptinpoly" OBJECT "ptinpoly.c")
set_target_properties("ptinpoly" PROPERTIES C_STANDARD 90)
target_include_directories("ptinpoly" PUBLIC ${LIBRARY_INCLUDES})

add_library("common" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("common" PUBLIC ${LIBRARY_INCLUDES})

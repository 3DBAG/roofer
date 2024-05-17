set(LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/NodataCircleComputer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PC2MeshDistCalculator.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pip_util.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointcloudRasteriser.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/projHelper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/select_pointcloud.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vector2DOpsGEOS.cpp")
if(USE_VAL3DITY)
    list(APPEND LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Val3dator.cpp")
endif()
set(LIBRARY_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/NodataCircleComputer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PC2MeshDistCalculator.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/pip_util.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointcloudRasteriser.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/projHelper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/select_pointcloud.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Vector2DOps.hpp")
if(USE_VAL3DITY)
    list(APPEND LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/Val3dator.hpp")
endif()
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include"
                     "${PROJECT_SOURCE_DIR}/external/ptinpoly")

add_library("misc" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("misc" PUBLIC ${LIBRARY_INCLUDES})

set(MISC_LINK_LIBS 
    "common"
    "logger"
    "reconstruction"
    PROJ::proj
    ptinpoly
    CGAL::CGAL
    CGAL::CGAL_Core # Eigen3::Eigen
    GEOS::geos_c
)
if(USE_VAL3DITY)
    list(APPEND MISC_LINK_LIBS val3dity)
endif()

target_link_libraries(
  "misc"
  PRIVATE ${MISC_LINK_LIBS})

set(LIBRARY_SOURCES
    "NodataCircleComputer.cpp"
    "PC2MeshDistCalculator.cpp"
    "PointcloudRasteriser.cpp"
    "projHelper.cpp"
    "select_pointcloud.cpp"
    "Vector2DOpsGEOS.cpp"
    "MeshPropertyCalculator.cpp")
if(RF_USE_VAL3DITY)
  list(APPEND LIBRARY_SOURCES "Val3dator.cpp")
endif()
set(LIBRARY_HEADERS
    "${ROOFER_INCLUDE_DIR}/roofer/misc/NodataCircleComputer.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/PC2MeshDistCalculator.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/PointcloudRasteriser.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/projHelper.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/select_pointcloud.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/Vector2DOps.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/misc/MeshPropertyCalculator.hpp")
if(RF_USE_VAL3DITY)
  list(APPEND LIBRARY_HEADERS "${ROOFER_INCLUDE_DIR}/roofer/misc/Val3dator.hpp")
endif()
set(LIBRARY_INCLUDES
    "${ROOFER_INCLUDE_DIR}")

find_package(GEOS CONFIG REQUIRED)
# Workaround: Conan's geos recipe sets an empty generator expression for
# INTERFACE_INCLUDE_DIRECTORIES on geos_c. Append the real path via find_path,
# which searches CMAKE_INCLUDE_PATH that Conan correctly populates.
find_path(GEOS_C_INCLUDE_DIR geos_c.h REQUIRED)
set_property(TARGET GEOS::geos_c APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${GEOS_C_INCLUDE_DIR}")

add_library("misc" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("misc" PUBLIC ${LIBRARY_INCLUDES})

set(MISC_LINK_LIBS
    CGAL::CGAL
    GEOS::geos_c
    fmt::fmt
)
if(RF_USE_VAL3DITY)
  if(RF_USE_CPM)
    cpmaddpackage(
            NAME val3dity
            GITHUB_REPOSITORY "ylannl/val3dity"
            GIT_TAG "master"
            OPTIONS "VAL3DITY_LIBRARY ON"
                    "VAL3DITY_USE_INTERNAL_DEPS OFF")
  else()
    find_package(val3dity REQUIRED)
  endif()
  list(APPEND MISC_LINK_LIBS val3dity)
endif()

target_link_libraries("misc" PUBLIC ${MISC_LINK_LIBS})

set(LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/Raster.cpp")
set(LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/Raster.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/datastructures.hpp")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("datastructures" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("datastructures" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries( "datastructures"
  PUBLIC
    "common"
)
set(LIBRARY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/CityJsonWriter.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointCloudReaderLASlib.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointCloudWriterLASlib.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RasterWriterGDAL.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StreamCropper.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/VectorReaderOGR.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/VectorWriterOGR.cpp")
set(LIBRARY_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/CityJsonWriter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointCloudReader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/PointCloudWriter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/RasterWriter.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/StreamCropper.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/VectorReader.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/VectorWriter.hpp")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("io" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("io" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("io" PRIVATE "datastructures" "misc" "logger" GDAL::GDAL
                                   LASlib)

set(LIBRARY_SOURCES
    "CityJsonWriter.cpp"
    "PointCloudReaderLASlib.cpp"
    "PointCloudWriterLASlib.cpp"
    "RasterWriterGDAL.cpp"
    "StreamCropper.cpp"
    "VectorReaderOGR.cpp"
    "VectorWriterOGR.cpp")
set(LIBRARY_HEADERS
    "${ROOFER_INCLUDE_DIR}/roofer/io/CityJsonWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/PointCloudReader.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/PointCloudWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/RasterWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/StreamCropper.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/VectorReader.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/VectorWriter.hpp")
set(LIBRARY_INCLUDES
    "${ROOFER_INCLUDE_DIR}")

find_package(nlohmann_json REQUIRED)
find_package(laslib CONFIG REQUIRED)

# GDAL
find_package(GDAL CONFIG REQUIRED)

add_library("io" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("io" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("io" PUBLIC GDAL::GDAL LASlib)

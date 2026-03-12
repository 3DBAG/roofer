set(LIBRARY_SOURCES
    "CityJsonWriter.cpp"
    "PointCloudReaderLASlib.cpp"
    "PointCloudWriterLASlib.cpp"
    "RasterWriterGDAL.cpp"
    "StreamCropper.cpp"
    "VectorReaderOGR.cpp"
    "VectorWriterOGR.cpp"
    SpatialReferenceSystemOGR.cpp)
set(LIBRARY_HEADERS
    "${ROOFER_INCLUDE_DIR}/roofer/io/CityJsonWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/PointCloudReader.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/PointCloudWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/RasterWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/StreamCropper.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/VectorReader.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/VectorWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/SpatialReferenceSystem.hpp")
set(LIBRARY_INCLUDES
    "${ROOFER_INCLUDE_DIR}")

find_package(laslib CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(proj CONFIG REQUIRED)
# GDAL
find_package(GDAL CONFIG REQUIRED)

if(TARGET laslib::laslib)
  set(LASLIB_TARGET laslib::laslib)
elseif(TARGET LASlib)
  set(LASLIB_TARGET LASlib)
else()
  message(FATAL_ERROR "Could not find a usable LASlib target")
endif()

add_library("io" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("io" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("io" PUBLIC GDAL::GDAL ${LASLIB_TARGET} nlohmann_json::nlohmann_json fmt::fmt)

if(DEFINED gdal_PACKAGE_FOLDER_RELEASE AND EXISTS "${gdal_PACKAGE_FOLDER_RELEASE}/res/gdal")
  install(
    DIRECTORY "${gdal_PACKAGE_FOLDER_RELEASE}/res/gdal"
    DESTINATION "share")
endif()

if(DEFINED proj_PACKAGE_FOLDER_RELEASE AND EXISTS "${proj_PACKAGE_FOLDER_RELEASE}/res")
  install(
    DIRECTORY "${proj_PACKAGE_FOLDER_RELEASE}/res/"
    DESTINATION "share/proj")
endif()

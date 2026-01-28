set(LIBRARY_SOURCES
    "CityJsonWriter.cpp"
    "FcbWriter.cpp"
    "PointCloudReaderLASlib.cpp"
    "PointCloudWriterLASlib.cpp"
    "RasterWriterGDAL.cpp"
    "StreamCropper.cpp"
    "VectorReaderOGR.cpp"
    "VectorWriterOGR.cpp"
    SpatialReferenceSystemOGR.cpp)

# Add cxx-generated C++ source if FCB FFI is enabled
# The .cc file is generated during roofer_fcb_ffi_build and copied to build directory
if(TARGET roofer_fcb_ffi_build)
  set(FCB_CC_FILE "${CMAKE_BINARY_DIR}/roofer-fcb-ffi/src/lib.rs.cc")
  list(APPEND LIBRARY_SOURCES "${FCB_CC_FILE}")
  # Mark as generated so CMake doesn't complain if it doesn't exist at configure time
  set_source_files_properties("${FCB_CC_FILE}" PROPERTIES GENERATED TRUE)
endif()
set(LIBRARY_HEADERS
    "${ROOFER_INCLUDE_DIR}/roofer/io/CityJsonWriter.hpp"
    "${ROOFER_INCLUDE_DIR}/roofer/io/FcbWriter.hpp"
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
# GDAL
find_package(GDAL CONFIG REQUIRED)

add_library("io" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("io" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("io" PUBLIC GDAL::GDAL LASlib nlohmann_json::nlohmann_json)
if(TARGET roofer_fcb_ffi)
  target_link_libraries("io" PUBLIC roofer_fcb_ffi)
  add_dependencies("io" roofer_fcb_ffi)
endif()
if(CXXBRIDGE1_LIB)
  # Link cxx static runtime so rust::cxxbridge1::Str::Str and friends are defined
  target_link_libraries("io" PUBLIC "${CXXBRIDGE1_LIB}")
endif()
if(TARGET roofer_fcb_ffi_build)
  add_dependencies("io" roofer_fcb_ffi_build)
endif()

set(TEST_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(DATA_URL_ROOT "https://data.3dgi.xyz/roofer-test-data")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")

FetchContent_Declare(
  wippolder
  URL "${DATA_URL_ROOT}/wippolder.zip"
  URL_HASH MD5=cc62bbdd6f4042159587b5906427f872
  SOURCE_DIR "${DATA_DIR}/wippolder")

FetchContent_MakeAvailable(wippolder)

add_test(
  NAME "crop-wippolder"
  COMMAND $<TARGET_FILE:crop> --config "${TEST_CONFIG_DIR}/crop-wippolder.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set_tests_properties("crop-wippolder" PROPERTIES
  ENVIRONMENT "PROJ_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj;GDAL_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal")

add_test(
  NAME "reconstruct-wippolder"
  COMMAND $<TARGET_FILE:reconstruct> --config
          "${CMAKE_CURRENT_SOURCE_DIR}/output/wippolder/objects/503100000030812/config_.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set_tests_properties("reconstruct-wippolder" PROPERTIES
  ENVIRONMENT "PROJ_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj;GDAL_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal")
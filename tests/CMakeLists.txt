set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(DATA_URL_ROOT "https://data.3dgi.xyz/roofer-test-data")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_ENVIRONMENT
    "PROJ_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj;GDAL_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal"
)

FetchContent_Declare(
  wippolder
  URL "${DATA_URL_ROOT}/wippolder.zip"
  URL_HASH MD5=cc62bbdd6f4042159587b5906427f872
  SOURCE_DIR "${DATA_DIR}/wippolder")
FetchContent_MakeAvailable(wippolder)

# Integration tests that are run on the built executables, before installation.

add_test(
  NAME "crop-wippolder"
  COMMAND $<TARGET_FILE:crop> --config "${CONFIG_DIR}/crop-wippolder.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_test(
  NAME "reconstruct-wippolder"
  COMMAND
    $<TARGET_FILE:reconstruct> --config
    "${CMAKE_CURRENT_SOURCE_DIR}/output/wippolder/objects/503100000030812/config_.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

set(tests_built "crop-wippolder;reconstruct-wippolder")
set_tests_properties(${tests_built} PROPERTIES ENVIRONMENT
                                               "${TEST_ENVIRONMENT}")

# Integration tests that are run on the installed artifacts must be prefixed with
# "installed-". Note that these tests must be invoked *after* the artifacts were
# installed.
# We don't set the TEST_ENVIRONMENT for the installed apps, because the required
# paths are supposed to be set by the install process.

add_test(
  NAME "installed-apps"
  COMMAND pytest -k TestApps
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

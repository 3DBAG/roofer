set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(DATA_URL_ROOT "https://data.3dgi.xyz/roofer-test-data")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
if (DEFINED VCPKG_TOOLCHAIN)
  set(TEST_ENVIRONMENT
      "PROJ_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj;GDAL_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal"
  )
endif()

FetchContent_Declare(
  wippolder
  URL "${DATA_URL_ROOT}/wippolder.zip"
  URL_HASH MD5=8d51a7a110810a9e2cbf9b6e83ea3833
  SOURCE_DIR "${DATA_DIR}/wippolder")
FetchContent_MakeAvailable(wippolder)

# Integration tests that are run on the built executables, before installation.
if(RF_BUILD_APPS)
  add_test(
    NAME "crop-wippolder"
    COMMAND $<TARGET_FILE:crop> --config "${CONFIG_DIR}/crop-wippolder.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "reconstruct-wippolder"
    COMMAND
      $<TARGET_FILE:reconstruct> --config
      "${CONFIG_DIR}/reconstruct-wippolder.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "reconstruct-api-wippolder"
    COMMAND
      $<TARGET_FILE:reconstruct_api>
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  set(tests_built "crop-wippolder;reconstruct-wippolder;reconstruct-api-wippolder")
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
  if(WIN32)
    set_tests_properties("installed-apps" PROPERTIES ENVIRONMENT
      "PROJ_DATA=${CMAKE_INSTALL_PREFIX}\\share\\proj;GDAL_DATA=${CMAKE_INSTALL_PREFIX}\\share\\gdal;PATH=C:\\Program Files (x86)\\roofer\\bin\;$ENV{PATH}")
  endif()

  add_test(
    NAME "reconstruct-with-bindings"
    COMMAND pytest -k TestBindings -s
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  if(WIN32)
    set_tests_properties("reconstruct-with-bindings" PROPERTIES ENVIRONMENT
      "PROJ_DATA=${CMAKE_INSTALL_PREFIX}\\share\\proj;GDAL_DATA=${CMAKE_INSTALL_PREFIX}\\share\\gdal;PATH=C:\\Program Files (x86)\\roofer\\bin\;$ENV{PATH}")
  endif()

endif()

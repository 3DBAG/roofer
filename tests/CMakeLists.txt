option(RF_TEST_INSTALL "Enable optional tests" OFF)

find_package(Catch2 3 REQUIRED)
include(FetchContent)

set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(DATA_URL_ROOT "https://data.3dbag.nl/testdata/roofer")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
if(DEFINED VCPKG_TOOLCHAIN)
  set(TEST_ENVIRONMENT
      "PROJ_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj;GDAL_DATA=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal"
  )
endif()

FetchContent_Declare(
  wippolder
  URL "${DATA_URL_ROOT}/wippolder.zip"
  URL_HASH MD5=872f74080ba8946fe2e9f7840a4dc78c
  SOURCE_DIR "${DATA_DIR}/wippolder")
FetchContent_MakeAvailable(wippolder)

FetchContent_Declare(
  issue-64
  URL "${DATA_URL_ROOT}/issue-64.zip"
  URL_HASH MD5=49fe5d2f088f4c81089813f746264002
  SOURCE_DIR "${DATA_DIR}/issue-64")
FetchContent_MakeAvailable(issue-64)

#FetchContent_Declare(
#  wateringen
#  URL "${DATA_URL_ROOT}/wateringen.zip"
#  URL_HASH MD5=f96240033427f03cffeeb2d3cae63f3a
#  SOURCE_DIR "${DATA_DIR}/wateringen")
#FetchContent_MakeAvailable(wateringen)
#
#FetchContent_Declare(
#  wateringen_large
#  URL "${DATA_URL_ROOT}/wateringen-large.zip"
#  URL_HASH MD5=10844a5722d8ffeb554363e404a3c27b
#  SOURCE_DIR "${DATA_DIR}/wateringen-large")
#FetchContent_MakeAvailable(wateringen_large)

# --- Library testing
include(Catch)

add_executable("test_logger" "${CMAKE_CURRENT_SOURCE_DIR}/test_logger.cpp")
target_link_libraries("test_logger" PUBLIC logger)
target_link_libraries("test_logger" PRIVATE Catch2::Catch2WithMain)

# --- API testing
include_directories("${PROJECT_SOURCE_DIR}/apps/external")
add_executable("reconstruct_api"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_reconstruct_api.cpp")
set_target_properties("reconstruct_api" PROPERTIES CXX_STANDARD 20)
target_link_libraries("reconstruct_api" PRIVATE roofer-extra
                                                cmake_git_version_tracking)
add_test(
  NAME "reconstruct-api-wippolder"
  COMMAND $<TARGET_FILE:reconstruct_api>
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set(tests_api "reconstruct-api-wippolder")
set_tests_properties(${tests_api} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# --- Integration tests that are run on the built executables, before
# installation.
if(RF_BUILD_APPS)
  # add_test(
  #   NAME "crop-wippolder"
  #   COMMAND $<TARGET_FILE:crop> --config "${CONFIG_DIR}/crop-wippolder.toml"
  #   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "reconstruct-wippolder"
    COMMAND $<TARGET_FILE:reconstruct> --config
            "${CONFIG_DIR}/reconstruct-wippolder.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "roofer-wippolder"
    COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/roofer-wippolder.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "issue-64"
    COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/issue-64.toml" --filter identificatie='NL.IMBAG.Pand.0603100000011074'
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  set(tests_built "reconstruct-wippolder;roofer-wippolder;issue-64")
  set_tests_properties(${tests_built} PROPERTIES ENVIRONMENT
                                                 "${TEST_ENVIRONMENT}")

  # Integration tests that are run on the installed artifacts must be prefixed
  # with "installed-". Note that these tests must be invoked *after* the
  # artifacts were installed. We don't set the TEST_ENVIRONMENT for the
  # installed apps, because the required paths are supposed to be set by the
  # install process.
  if(RF_TEST_INSTALL)
    add_test(
      NAME "installed-roofer-version"
      COMMAND roofer --version
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
      NAME "installed-roofer-wippolder"
      COMMAND roofer --config "${CONFIG_DIR}/roofer-wippolder.toml"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
      NAME "installed-reconstruct-version"
      COMMAND reconstruct --version
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
      NAME "installed-reconstruct-wippolder"
      COMMAND reconstruct --config "${CONFIG_DIR}/roofer-wippolder.toml"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    set(tests_installed
        "installed-roofer-version;installed-reconstruct-version;installed-roofer-wippolder;installed-reconstruct-wippolder"
    )
    set(WINDOWS_INSTALLED_ENVIRONMENT
        "GDAL_DATA=${CMAKE_INSTALL_PREFIX}\\share\\gdal;PATH=C:\\Program Files (x86)\\roofer\\bin\;$ENV{PATH}"
    )
    if(WIN32)
      set_tests_properties(
        ${tests_installed} PROPERTIES ENVIRONMENT
                                      "${WINDOWS_INSTALLED_ENVIRONMENT}")
    endif()
  endif()
endif()

if(RF_BUILD_BINDINGS)
  add_test(
    NAME "rooferpy"
    COMMAND python "${CMAKE_SOURCE_DIR}/rooferpy/example_rooferpy.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
  if(WIN32)
    set_tests_properties(
      "rooferpy" PROPERTIES ENVIRONMENT
                            "PYTHONPATH=${CMAKE_BINARY_DIR}/rooferpy/Release")
  else()
    set_tests_properties(
      "rooferpy" PROPERTIES ENVIRONMENT
                            "PYTHONPATH=${CMAKE_BINARY_DIR}/rooferpy")
  endif()
endif()

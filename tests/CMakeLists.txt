option(RF_TEST_INSTALL "Enable optional tests" OFF)

find_package(Catch2 3 REQUIRED)
include(FetchContent)

set(CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")
set(DATA_URL_ROOT "https://data.3dbag.nl/testdata/roofer")
set(DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(TEST_ENVIRONMENT
    "PROJ_DATA=${CMAKE_INSTALL_PREFIX}/share/proj;GDAL_DATA=${CMAKE_INSTALL_PREFIX}/share/gdal"
)

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

FetchContent_Declare(
  issue-71-v2
  URL "${DATA_URL_ROOT}/issue-71-v2.zip"
  URL_HASH MD5=7143bc65f71ac7c66f57bc9a57947e85
  SOURCE_DIR "${DATA_DIR}/issue-71-v2")
FetchContent_MakeAvailable(issue-71-v2)

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

add_executable("test_reconstruction_config"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_reconstruction_config.cpp")
target_link_libraries("test_reconstruction_config"
                      PRIVATE Catch2::Catch2WithMain roofer-core)
catch_discover_tests("test_reconstruction_config")

add_executable("test_app_reconstruction_config"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_app_reconstruction_config.cpp")
target_include_directories(
  "test_app_reconstruction_config"
  PRIVATE "${PROJECT_SOURCE_DIR}/apps/roofer-app"
          "${PROJECT_SOURCE_DIR}/apps/external" "${CMAKE_BINARY_DIR}")
target_link_libraries("test_app_reconstruction_config"
                      PRIVATE Catch2::Catch2WithMain roofer-core fmt::fmt)
if(TARGET update_git_hash)
  add_dependencies("test_app_reconstruction_config" update_git_hash)
endif()
catch_discover_tests("test_app_reconstruction_config")

add_executable("test_arrangement_dissolver"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_arrangement_dissolver.cpp")
target_link_libraries("test_arrangement_dissolver"
                      PRIVATE Catch2::Catch2WithMain roofer-core)
catch_discover_tests("test_arrangement_dissolver")

add_executable("test_arrangement_snapper"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_arrangement_snapper.cpp")
target_link_libraries("test_arrangement_snapper"
                      PRIVATE Catch2::Catch2WithMain roofer-core)
catch_discover_tests("test_arrangement_snapper")

add_executable("test_vector_reader"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_vector_reader.cpp")
target_link_libraries("test_vector_reader" PRIVATE Catch2::Catch2WithMain)
target_link_libraries("test_vector_reader" PRIVATE roofer-extra)

add_executable("test_terrain" "${CMAKE_CURRENT_SOURCE_DIR}/test_terrain.cpp")
target_link_libraries("test_terrain" PRIVATE Catch2::Catch2WithMain
                                             roofer-extra)

# TODO: refactor config helpers so that we can depend without the githash
if(RF_GIT_HASH)
  target_compile_definitions("test_terrain" PRIVATE RF_GIT_HASH="${RF_GIT_HASH}")
else()
  target_include_directories("test_terrain" PRIVATE ${CMAKE_BINARY_DIR})
  if(TARGET update_git_hash)
    add_dependencies("test_terrain" update_git_hash)
  endif()
endif()
catch_discover_tests("test_terrain")

# --- API testing
include_directories("${PROJECT_SOURCE_DIR}/apps/external")
add_executable("reconstruct_api"
               "${CMAKE_CURRENT_SOURCE_DIR}/test_reconstruct_api.cpp")
set_target_properties("reconstruct_api" PROPERTIES CXX_STANDARD 20)
target_link_libraries("reconstruct_api" PRIVATE roofer-extra)
add_test(
  NAME "reconstruct-api-wippolder"
  COMMAND $<TARGET_FILE:reconstruct_api>
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set(tests_api "reconstruct-api-wippolder")
set_tests_properties(${tests_api} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(
  NAME "vector-reader-skip-invalid-polygons"
  COMMAND $<TARGET_FILE:test_vector_reader>
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_tests_properties("vector-reader-skip-invalid-polygons"
                     PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# --- Integration tests that are run on the built executables, before
# installation.
if(RF_BUILD_APPS)
  # add_test(
  #   NAME "crop-wippolder"
  #   COMMAND $<TARGET_FILE:crop> --config "${CONFIG_DIR}/crop-wippolder.toml"
  #   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  # add_test(
  #   NAME "reconstruct-wippolder"
  #   COMMAND $<TARGET_FILE:reconstruct> --config
  #           "${CONFIG_DIR}/reconstruct-wippolder.toml"
  #   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "roofer-wippolder"
    COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/roofer-wippolder.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "issue-64"
    COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/issue-64.toml" --filter identificatie='NL.IMBAG.Pand.0603100000011074'
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  add_test(
    NAME "issue-71-v2"
    COMMAND $<TARGET_FILE:roofer> --config "${CONFIG_DIR}/issue-71-v2.toml"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

  set(tests_built "roofer-wippolder;issue-64;issue-71-v2")
  set_tests_properties(${tests_built} PROPERTIES ENVIRONMENT
                                                 "${TEST_ENVIRONMENT}")

  # Integration tests that are run on the installed artifacts must be prefixed
  # with "installed-". Note that these tests must be invoked *after* the
  # artifacts were installed.
  if(RF_TEST_INSTALL)
    add_test(
      NAME "installed-roofer-version"
      COMMAND roofer --version
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_test(
      NAME "installed-roofer-wippolder"
      COMMAND roofer --config "${CONFIG_DIR}/roofer-wippolder.toml"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    set(tests_installed
        "installed-roofer-version;installed-roofer-wippolder"
    )
    set_tests_properties(
      ${tests_installed} PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
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
                            "PYTHONPATH=${CMAKE_BINARY_DIR}\\rooferpy\\Release")
  else()
    set_tests_properties(
      "rooferpy" PROPERTIES ENVIRONMENT
                            "PYTHONPATH=${CMAKE_BINARY_DIR}/rooferpy")
  endif()
endif()

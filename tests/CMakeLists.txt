set(TEST_CONFIG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_test(
  NAME "crop-wippolder"
  COMMAND $<TARGET_FILE:crop> --config "${TEST_CONFIG_DIR}/crop-wippolder.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_test(
  NAME "reconstruct-wippolder"
  COMMAND $<TARGET_FILE:reconstruct> --config
          "${TEST_CONFIG_DIR}/reconstruct-wippolder.toml"
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

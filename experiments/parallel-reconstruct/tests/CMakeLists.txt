add_executable("sequential" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential.cpp")
target_link_libraries("sequential" PRIVATE "LibIo" "LibCropper" "LibReconstruction")

add_test(NAME "sequential" COMMAND $<TARGET_FILE:sequential> WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

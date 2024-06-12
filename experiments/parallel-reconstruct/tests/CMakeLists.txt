add_executable("sequential" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential.cpp")
add_executable("sequential_coro" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential_coro.cpp")
target_link_libraries("sequential" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
target_link_libraries("sequential_coro" PRIVATE "LibIo" "LibCropper" "LibReconstruction")

add_test(NAME "sequential" COMMAND $<TARGET_FILE:sequential> WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME "sequential_coro" COMMAND $<TARGET_FILE:sequential_coro> WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

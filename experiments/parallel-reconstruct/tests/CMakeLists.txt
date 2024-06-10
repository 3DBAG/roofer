add_executable("sequential" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential.cpp")
add_executable("async_coro" "${CMAKE_CURRENT_SOURCE_DIR}/test_async_coro.cpp")
target_link_libraries("sequential" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
target_link_libraries("async_coro" PRIVATE "LibIo" "LibCropper" "LibReconstruction")

add_test(NAME "sequential" COMMAND $<TARGET_FILE:sequential> WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME "async_coro" COMMAND $<TARGET_FILE:async_coro> WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

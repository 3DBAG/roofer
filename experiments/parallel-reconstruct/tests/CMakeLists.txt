set("NrLAZ" 3)
set("NrPointsPerLAZ" 2000)

add_executable("sequential" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential.cpp")
target_link_libraries("sequential" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
add_test(NAME "sequential" COMMAND $<TARGET_FILE:sequential> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("sequential_coro" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential_coro.cpp")
target_link_libraries("sequential_coro" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
add_test(NAME "sequential_coro" COMMAND $<TARGET_FILE:sequential_coro> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("parallel_reconstruct" "${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_reconstruct.cpp")
target_link_libraries("parallel_reconstruct" PRIVATE "LibIo" "LibCropper" "LibReconstruction" libfork::libfork)
add_test(NAME "parallel_reconstruct" COMMAND $<TARGET_FILE:parallel_reconstruct> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

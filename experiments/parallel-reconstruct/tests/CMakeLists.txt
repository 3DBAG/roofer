set("NrLAZ" 5)
set("NrPointsPerLAZ" 10000)

add_executable("sequential" "${CMAKE_CURRENT_SOURCE_DIR}/test_sequential.cpp")
target_link_libraries("sequential" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
add_test(NAME "sequential" COMMAND $<TARGET_FILE:sequential> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("streaming" "${CMAKE_CURRENT_SOURCE_DIR}/test_streaming.cpp")
target_link_libraries("streaming" PRIVATE "LibIo" "LibCropper" "LibReconstruction")
add_test(NAME "streaming" COMMAND $<TARGET_FILE:streaming> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("parallel_reconstruct" "${CMAKE_CURRENT_SOURCE_DIR}/test_parallel_reconstruct.cpp")
target_link_libraries("parallel_reconstruct" PRIVATE "LibIo" "LibCropper" "LibReconstruction" libfork::libfork)
add_test(NAME "parallel_reconstruct" COMMAND $<TARGET_FILE:parallel_reconstruct> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable("async" "${CMAKE_CURRENT_SOURCE_DIR}/test_async.cpp")
target_link_libraries("async" PRIVATE "LibIo" "LibCropper" "LibReconstruction" libfork::libfork)
add_test(NAME "async" COMMAND $<TARGET_FILE:async> ${NrLAZ} ${NrPointsPerLAZ} WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

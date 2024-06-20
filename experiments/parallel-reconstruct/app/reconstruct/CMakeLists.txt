set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

add_executable("reconstruct" ${APP_SOURCES})

target_link_libraries("reconstruct" PRIVATE "LibIo" "LibReconstruction")

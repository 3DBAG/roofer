set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")

add_executable("crop" ${APP_SOURCES})

target_link_libraries("crop" PRIVATE "LibIo")

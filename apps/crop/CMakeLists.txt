set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crop.cpp")

add_executable("crop" ${APP_SOURCES})
set_target_properties("crop" PROPERTIES CXX_STANDARD 20)
target_link_libraries("crop" PRIVATE "io" "datastructures" "misc" "logger"
                                     cmake_git_version_tracking fmt::fmt)

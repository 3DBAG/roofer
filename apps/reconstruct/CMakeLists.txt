set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/reconstruct.cpp")

add_executable("reconstruct" ${APP_SOURCES})
set_target_properties("reconstruct" PROPERTIES CXX_STANDARD 20)
target_link_libraries(
  "reconstruct"
  PRIVATE "common"
          "datastructures"
          "misc"
          "io"
          "reconstruction"
          "logger"
          CGAL::CGAL_Core
          cmake_git_version_tracking
          rerun_sdk)

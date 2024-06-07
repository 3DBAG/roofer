set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rooferpy.cpp")

find_package(pybind11 REQUIRED)

pybind11_add_module("rooferpy" ${APP_SOURCES})
set_target_properties("rooferpy" PROPERTIES CXX_STANDARD 20)

set(RECONSTRUCT_LINK_LIBS
  roofer-core
  cmake_git_version_tracking
)

target_link_libraries(
  "rooferpy"
  PRIVATE ${RECONSTRUCT_LINK_LIBS}
  pybind11::module)

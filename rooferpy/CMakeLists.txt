set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rooferpy.cpp")

find_package(pybind11 REQUIRED)

pybind11_add_module("rooferpy" ${APP_SOURCES})

set(RECONSTRUCT_LINK_LIBS
  roofer-core
)

target_link_libraries(
  "rooferpy"
  PRIVATE ${RECONSTRUCT_LINK_LIBS}
  pybind11::module)

  set_target_properties(rooferpy PROPERTIES OUTPUT_NAME "roofer")

set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/rooferpy.cpp")

set(PYBIND11_FINDPYTHON ON)
find_package(Python REQUIRED COMPONENTS Interpreter Development.Module)
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

install(TARGETS rooferpy
        DESTINATION "lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages"
)

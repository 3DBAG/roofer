set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/reconstruct_api.cpp")

add_executable("reconstruct_api" ${APP_SOURCES})
set_target_properties("reconstruct_api" PROPERTIES CXX_STANDARD 20)

set(RECONSTRUCT_LINK_LIBS
  roofer-extra
  cmake_git_version_tracking
)
if(RF_USE_RERUN)
    list(APPEND RECONSTRUCT_LINK_LIBS rerun_sdk)
endif()

target_link_libraries(
  "reconstruct_api"
  PRIVATE ${RECONSTRUCT_LINK_LIBS})

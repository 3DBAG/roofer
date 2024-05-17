set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/reconstruct.cpp")

add_executable("reconstruct" ${APP_SOURCES})
set_target_properties("reconstruct" PROPERTIES CXX_STANDARD 20)

set(RECONSTRUCT_LINK_LIBS 
  roofer-core
  roofer-extra
  cmake_git_version_tracking
)
if(RF_USE_RERUN)
    list(APPEND RECONSTRUCT_LINK_LIBS rerun_sdk)
endif()

target_link_libraries(
  "reconstruct"
  PRIVATE ${RECONSTRUCT_LINK_LIBS})

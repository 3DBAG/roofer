set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/roofer-app.cpp")

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  find_package(mimalloc CONFIG REQUIRED)
endif()

add_executable("roofer" ${APP_SOURCES})
set_target_properties("roofer" PROPERTIES CXX_STANDARD 20)
target_link_libraries("roofer" PRIVATE roofer-extra cmake_git_version_tracking
                                       fmt::fmt
                                       $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>)

# Used for getting the process memory usage
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions("roofer" PUBLIC "IS_LINUX")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  target_compile_definitions("roofer" PUBLIC "IS_MACOS")
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions("roofer" PUBLIC "IS_WINDOWS")
endif()

if(RF_USE_VAL3DITY)
  target_compile_definitions("roofer" PRIVATE RF_USE_VAL3DITY)
endif()
if(RF_ENABLE_HEAP_TRACING)
  target_compile_definitions("roofer" PRIVATE RF_ENABLE_HEAP_TRACING)
endif()

install(
  TARGETS "roofer"
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

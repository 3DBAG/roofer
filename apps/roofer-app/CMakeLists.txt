if(RF_BUILD_APPS)
  set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/roofer-app.cpp")

  set(ROOFER_LINK_LIBRARIES roofer-extra fmt::fmt)

  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(mimalloc CONFIG REQUIRED)
    # if(TARGET mimalloc-static)
    #   list(APPEND ROOFER_LINK_LIBRARIES mimalloc-static)
    # else()
      list(APPEND ROOFER_LINK_LIBRARIES mimalloc)
    # endif()
  endif()

  add_executable("roofer" ${APP_SOURCES})

  if(RF_USE_RERUN)
    list(APPEND ROOFER_LINK_LIBRARIES rerun_sdk)
    target_compile_definitions("roofer" PRIVATE RF_USE_RERUN)
  endif()

  set_target_properties("roofer" PROPERTIES CXX_STANDARD 20)
  target_link_libraries("roofer" PRIVATE ${ROOFER_LINK_LIBRARIES})

  # Used for getting the process memory usage
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions("roofer" PUBLIC "IS_LINUX")
    target_link_options("roofer" PRIVATE -static-libstdc++)
    target_compile_options("roofer" PRIVATE -static-libstdc++)
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
  if(RF_GIT_HASH)
    target_compile_definitions(roofer PRIVATE RF_GIT_HASH="${RF_GIT_HASH}")
  endif()
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    install (DIRECTORY ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj
            DESTINATION "share"
            FILES_MATCHING PATTERN "*" PATTERN "*.cmake" EXCLUDE)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal
            DESTINATION "share"
            FILES_MATCHING PATTERN "*" PATTERN "*.cmake" EXCLUDE)
  endif()
endif()

if(RF_BUILD_DOC_HELPER)
  add_executable("doc-helper" "doc-helper.cpp")
  set_target_properties("doc-helper" PROPERTIES CXX_STANDARD 20)
  target_link_libraries("doc-helper" PRIVATE roofer-core fmt::fmt)
  install(
     TARGETS "doc-helper"
     ARCHIVE DESTINATION lib
     LIBRARY DESTINATION lib
     RUNTIME DESTINATION bin)
  if(RF_GIT_HASH)
    target_compile_definitions(doc-helper PRIVATE RF_GIT_HASH="${RF_GIT_HASH}")
  endif()
endif()

# get the --version flag working. Try to use RF_GIT_HASH if defined (eg by nix build), otherwise fallback to cmake helper function
if(RF_BUILD_DOC_HELPER OR RF_BUILD_APPS)
  if(NOT RF_GIT_HASH)
    set(GIT_HASH_HEADER ${CMAKE_BINARY_DIR}/git_hash.h)

    add_custom_command(
        OUTPUT ${GIT_HASH_HEADER}
        COMMAND ${CMAKE_COMMAND} -DOUT=${GIT_HASH_HEADER} -P ${CMAKE_SOURCE_DIR}/cmake/gen_git_hash_header.cmake
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Generating git_hash.h"
        VERBATIM
    )

    add_custom_target(update_git_hash ALL DEPENDS ${GIT_HASH_HEADER})

    if (RF_BUILD_APPS)
      target_include_directories(roofer PRIVATE ${CMAKE_BINARY_DIR})
      add_dependencies(roofer update_git_hash)
    endif()

    if (RF_BUILD_DOC_HELPER)
      target_include_directories("doc-helper" PRIVATE ${CMAKE_BINARY_DIR})
      add_dependencies("doc-helper" update_git_hash)
    endif()
  endif()
endif()

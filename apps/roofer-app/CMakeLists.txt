if(RF_BUILD_APPS)
  set(APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/roofer-app.cpp")
 
  set(ROOFER_LINK_LIBRARIES roofer-extra fmt::fmt cmake_git_version_tracking)
  if(TARGET roofer_fcb_ffi)
    list(APPEND ROOFER_LINK_LIBRARIES roofer_fcb_ffi)
  endif()
  if(CXXBRIDGE1_LIB)
    # Also link cxx static runtime library for Str constructors, etc.
    list(APPEND ROOFER_LINK_LIBRARIES "${CXXBRIDGE1_LIB}")
  endif()

  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(mimalloc CONFIG REQUIRED)
    list(APPEND ROOFER_LINK_LIBRARIES $<IF:$<TARGET_EXISTS:mimalloc-static>,mimalloc-static,mimalloc>)
  endif()

  add_executable("roofer" ${APP_SOURCES})
  
  # Make roofer depend on Rust FFI library
  if(TARGET roofer_fcb_ffi)
    add_dependencies("roofer" roofer_fcb_ffi)
  endif()
  if(TARGET roofer_fcb_ffi_build)
    add_dependencies("roofer" roofer_fcb_ffi_build)
  endif()

  if(RF_USE_RERUN)
    list(APPEND ROOFER_LINK_LIBRARIES rerun_sdk)
    target_compile_definitions("roofer" PRIVATE RF_USE_RERUN)
  endif()

  set_target_properties("roofer" PROPERTIES CXX_STANDARD 20)
  # Link libraries - ensure roofer_fcb_ffi is linked after object files
  target_link_libraries("roofer" PRIVATE ${ROOFER_LINK_LIBRARIES})
  
  # Ensure Rust library and cxx static library are linked
  if(TARGET roofer_fcb_ffi)
    target_link_libraries("roofer" PRIVATE roofer_fcb_ffi)
  endif()
  
  # Set rpath for Rust library so it can be found at runtime
  if(TARGET roofer_fcb_ffi)
    get_target_property(FCB_LIB_LOC roofer_fcb_ffi IMPORTED_LOCATION)
    if(FCB_LIB_LOC)
      get_filename_component(FCB_LIB_DIR ${FCB_LIB_LOC} DIRECTORY)
      if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set_target_properties("roofer" PROPERTIES
          INSTALL_RPATH "${FCB_LIB_DIR}"
          BUILD_WITH_INSTALL_RPATH TRUE
        )
        target_link_options("roofer" PRIVATE "-Wl,-rpath,${FCB_LIB_DIR}")
      elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set_target_properties("roofer" PROPERTIES
          INSTALL_RPATH "${FCB_LIB_DIR}"
          BUILD_WITH_INSTALL_RPATH TRUE
        )
        target_link_options("roofer" PRIVATE "-Wl,-rpath,${FCB_LIB_DIR}")
      endif()
    endif()
  endif()

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
  if(DEFINED CMAKE_TOOLCHAIN_FILE)
    install (DIRECTORY ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/proj
            DESTINATION "share"
            FILES_MATCHING PATTERN "*" PATTERN "*.cmake" EXCLUDE)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share/gdal
            DESTINATION "share"
            FILES_MATCHING PATTERN "*" PATTERN "*.cmake" EXCLUDE)
  endif()
endif()

if(RF_BUILD_DOC_HELPER OR RF_BUILD_APPS)
  add_executable("doc-helper" "doc-helper.cpp")
  set_target_properties("doc-helper" PROPERTIES CXX_STANDARD 20)
  target_link_libraries("doc-helper" PRIVATE roofer-core fmt::fmt cmake_git_version_tracking)
  # install(
  #   TARGETS "doc-helper"
  #   ARCHIVE DESTINATION lib
  #   LIBRARY DESTINATION lib
  #   RUNTIME DESTINATION bin)
endif()

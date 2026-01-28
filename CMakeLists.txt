# Require at least CMake 3.25, because in 3.24 and 3.25 there were a couple of
# FetchContent-related changes
cmake_minimum_required(VERSION 3.25)

# Options
option(RF_USE_LOGGER_SPDLOG "Use spdlog as logging backend." OFF)
option(RF_USE_RERUN "Enable features dependent on Rerun" OFF)
option(RF_USE_VAL3DITY "Enable features dependent on Val3dity" OFF)
option(RF_BUILD_APPS "Build applications in addition to library" ON)
option(RF_BUILD_DOC_HELPER "Build documentation generation helper app" OFF)
option(RF_BUILD_BINDINGS "Build python bindings with pybind" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries (may not work)" OFF)
option(RF_BUILD_TESTING "Enable tests for roofer" OFF)
option(RF_ENABLE_HEAP_TRACING "Enable heap allocation overloads" OFF)

# Enable the vcpkg features that are required by the options
if(RF_USE_LOGGER_SPDLOG)
  list(APPEND VCPKG_MANIFEST_FEATURES "spdlog")
endif()
if(RF_BUILD_TESTING)
  list(APPEND VCPKG_MANIFEST_FEATURES "test")
endif()
if(RF_BUILD_APPS)
  list(APPEND VCPKG_MANIFEST_FEATURES "apps")
endif()
if(RF_USE_VAL3DITY)
  list(APPEND VCPKG_MANIFEST_FEATURES "val3dity")
endif()
if(RF_USE_RERUN)
  list(APPEND VCPKG_MANIFEST_FEATURES "app-rerun")
endif()
# if(RF_BUILD_BINDINGS)
#   list(APPEND VCPKG_MANIFEST_FEATURES "python")
# endif()

# TODO: add version number
project(roofer VERSION 1.0.0 LANGUAGES C CXX)

# Global CMake variables are set here We use C++20, with the assumption that we
# only implement features that are supported by GCC, Clang, MSVC, Apple Clang
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Don't use extensions, because they might affect compiler compatibility
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# Enable position independent code for shared libraries in case we build them
if(BUILD_SHARED_LIBS OR RF_BUILD_BINDINGS)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# CMAKE MODULES

# CMake modules, like the documentation module, go in here
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/")

# EXTERNAL LIBRARIES

find_package(fmt REQUIRED)
find_package(
  CGAL 6.0 QUIET
  COMPONENTS Core
  REQUIRED)

# Make sure CGAL is thread-safe
add_compile_definitions("CGAL_HAS_THREADS=1")

# For arm64 set FPU rounding mode to prevent hanging in multi-threaded
# environments (this occurred during testing on Apple Silicon)
MESSAGE("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
  add_compile_definitions("CGAL_ALWAYS_ROUND_TO_NEAREST=1")
endif()

if(RF_USE_LOGGER_SPDLOG)
  message(STATUS "Logging backend: spdlog")
  set(SPDLOG_FMT_EXTERNAL ON)
  find_package(spdlog REQUIRED)
else()
  message(STATUS "Logging backend: internal")
endif()

# We have to use CPM (or FetchContent) even with vcpkg, because of
# cmake-git-version-tracking, val3dity, rerun
include(CPM)
cpmaddpackage(
  "gh:andrew-hardin/cmake-git-version-tracking#6c0cb87edd029ddfb403a8e24577c144a03605a6"
)
set(GIT_IGNORE_UNTRACKED TRUE)

if(RF_USE_RERUN)
  set(RERUN_DOWNLOAD_AND_BUILD_ARROW OFF)
  cpmaddpackage(
    NAME rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip
  )
endif()

# Rust FFI for FCB output
find_program(CARGO cargo)
if(CARGO)
  set(RUST_TARGET_DIR "${CMAKE_BINARY_DIR}/rust-target")
  set(FCB_FFI_DIR "${CMAKE_SOURCE_DIR}/roofer-fcb-ffi")
  
  # Determine library extension based on platform
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(FCB_FFI_LIB_EXT "dll")
  elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(FCB_FFI_LIB_EXT "dylib")
  else()
    set(FCB_FFI_LIB_EXT "so")
  endif()
  set(FCB_FFI_LIB "${RUST_TARGET_DIR}/release/libroofer_fcb_ffi.${FCB_FFI_LIB_EXT}")
  # cxx also builds a static runtime library (libcxxbridge1.a) that contains
  # implementations like rust::cxxbridge1::Str::Str(const char*). We need to
  # link this alongside the cdylib to satisfy those symbols.
  file(GLOB CXXBRIDGE1_LIB "${RUST_TARGET_DIR}/release/build/cxx-*/out/libcxxbridge1.a")
  
  # Prefer cargo from ~/.cargo/bin (rustup-managed, 1.93.0) over nix cargo (1.86.0)
  # Nix provides cargo 1.86.0 which uses rustc 1.86.0, but we need 1.93.0 for time@0.3.46
  find_program(CARGO_RUSTUP "$ENV{HOME}/.cargo/bin/cargo")
  if(CARGO_RUSTUP)
    set(CARGO_TO_USE ${CARGO_RUSTUP})
    message(STATUS "Using rustup-managed cargo: ${CARGO_TO_USE}")
  else()
    set(CARGO_TO_USE ${CARGO})
    message(STATUS "Using system cargo: ${CARGO_TO_USE}")
  endif()
  
  # Build Rust library
  # The cxx header will be generated in: target/release/build/roofer-fcb-ffi-<hash>/out/cxxbridge/roofer-fcb-ffi/src/lib.rs.h
  # Use rustup-managed cargo to get rustc 1.93.0 instead of nix's 1.86.0
  add_custom_target(roofer_fcb_ffi_build
    COMMAND ${CMAKE_COMMAND} -E env "PATH=$ENV{HOME}/.cargo/bin:$ENV{PATH}" "RUSTUP_TOOLCHAIN=stable" ${CARGO_TO_USE} build --release --manifest-path ${FCB_FFI_DIR}/Cargo.toml --target-dir ${RUST_TARGET_DIR}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building Rust FCB FFI library"
    BYPRODUCTS ${FCB_FFI_LIB}
  )
  
  # Find and copy the generated header and source to known locations
  # cxx generates headers in target/cxxbridge/<crate>/src/lib.rs.h (standard location, symlink)
  # The actual file is in target/release/build/<crate>-<hash>/out/cxxbridge/include/<crate>/src/lib.rs.h
  set(FCB_FFI_HEADER_OUTPUT "${CMAKE_BINARY_DIR}/include/roofer-fcb-ffi/src/lib.rs.h")
  set(FCB_FFI_CC_OUTPUT "${CMAKE_BINARY_DIR}/roofer-fcb-ffi/src/lib.rs.cc")
  # Use the symlink location which is more predictable
  # NOTE: cargo is invoked with --target-dir ${RUST_TARGET_DIR}, so cxxbridge outputs
  # are emitted under that directory (not under ${FCB_FFI_DIR}/target).
  set(FCB_FFI_HEADER_SOURCE "${RUST_TARGET_DIR}/cxxbridge/roofer-fcb-ffi/src/lib.rs.h")
  set(FCB_FFI_CC_SOURCE "${RUST_TARGET_DIR}/cxxbridge/roofer-fcb-ffi/src/lib.rs.cc")
  add_custom_command(TARGET roofer_fcb_ffi_build POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/include/roofer-fcb-ffi/src"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/roofer-fcb-ffi/src"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FCB_FFI_HEADER_SOURCE}" "${FCB_FFI_HEADER_OUTPUT}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${FCB_FFI_CC_SOURCE}" "${FCB_FFI_CC_OUTPUT}"
    COMMENT "Copying cxx-generated header and source to known location"
  )
  
  # Store paths for use in src/extra/io/CMakeLists.txt
  set(FCB_FFI_CC_FILE "${FCB_FFI_CC_OUTPUT}" CACHE INTERNAL "Path to cxx-generated C++ source")

  add_library(roofer_fcb_ffi SHARED IMPORTED)
  get_filename_component(FCB_FFI_LIB_DIR ${FCB_FFI_LIB} DIRECTORY)
  set_target_properties(roofer_fcb_ffi PROPERTIES
    IMPORTED_LOCATION ${FCB_FFI_LIB}
    IMPORTED_NO_SONAME ON
    IMPORTED_IMPLIB ${FCB_FFI_LIB}
  )
  
  # Make the imported library depend on the build target
  add_dependencies(roofer_fcb_ffi roofer_fcb_ffi_build)
  
  # Add the library directory to the linker search path for all targets
  link_directories(${FCB_FFI_LIB_DIR})
  
  # Add include directory for the copied header
  include_directories(${CMAKE_BINARY_DIR}/include)
else()
  message(WARNING "Cargo not found. FCB output support will not be available.")
  add_library(roofer_fcb_ffi INTERFACE)
endif()

if(MSVC)
  # windows.h breaks std::min/std::max, fix by define
  add_definitions(-DNOMINMAX)
  # enable permissive compiling and/or statements
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /permissive-")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:precise")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")
endif()

set(ROOFER_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include")
add_subdirectory(src)
if(RF_BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# Python binding
if(RF_BUILD_BINDINGS)
  add_subdirectory(rooferpy)
endif()

# EXECUTABLES
if(RF_BUILD_APPS OR RF_BUILD_DOC_HELPER)
  add_subdirectory(apps)
endif()

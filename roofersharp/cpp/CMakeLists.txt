# CMakeLists.txt for C# bindings
# Place in roofer/bindings/csharp/ or adjust paths accordingly

cmake_minimum_required(VERSION 3.21)

# If this is the top-level CMakeLists, define project
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(roofer-csharp VERSION 1.0.0)
  
  # Set C++ standard
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  
  # Add roofer core - adjust path as needed
  # If in bindings/csharp: ../../src/core
  # If standalone: point to roofer source
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../src/core")
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../src/core ${CMAKE_BINARY_DIR}/core)
  else()
    message(FATAL_ERROR "Cannot find roofer core library. Set roofer source path or build as subdirectory.")
  endif()
endif()

# C# wrapper sources
set(CSHARP_WRAPPER_SOURCES 
  "${CMAKE_CURRENT_SOURCE_DIR}/roofer_wrapper.cpp"
)

# Create shared library for C# P/Invoke
add_library(roofer-csharp SHARED ${CSHARP_WRAPPER_SOURCES})

# Link to roofer core
target_link_libraries(roofer-csharp
  PRIVATE roofer-core
)

# Set output name (roofer.dll on Windows, roofer.so on Linux)
set_target_properties(roofer-csharp PROPERTIES 
  OUTPUT_NAME "roofer"
  PREFIX ""  # Remove 'lib' prefix on Unix
)

# Platform-specific settings
if(WIN32)
  target_compile_definitions(roofer-csharp PRIVATE ROOFER_EXPORTS)
  # Output to bin/ for easy access
  set_target_properties(roofer-csharp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
  )
elseif(UNIX AND NOT APPLE)
  set_target_properties(roofer-csharp PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  )
endif()

# Include directories
target_include_directories(roofer-csharp
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install
install(TARGETS roofer-csharp
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

install(FILES 
  "${CMAKE_CURRENT_SOURCE_DIR}/roofer_wrapper.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/Roofer.cs"
  DESTINATION include
)

# Print helpful info
if(WIN32)
  message(STATUS "C# DLL will be built: build/bin/Release/roofer.dll")
else()
  message(STATUS "C# library will be built: build/lib/roofer.so")
endif()
message(STATUS "Copy Roofer.cs to your C# project and reference the built DLL")
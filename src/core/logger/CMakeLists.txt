set(LIBRARY_SOURCES)  # Empty for header-only library
set(LIBRARY_HEADERS "${ROOFER_INCLUDE_DIR}/roofer/logger/logger.h")
set(LIBRARY_INCLUDES  "${ROOFER_INCLUDE_DIR}")

add_library("logger" INTERFACE)  # Changed to INTERFACE library
target_include_directories("logger" INTERFACE ${LIBRARY_INCLUDES})

# Always link spdlog since we're using it directly in the header
target_link_libraries("logger" INTERFACE fmt::fmt spdlog::spdlog)

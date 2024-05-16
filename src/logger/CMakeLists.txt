set(LIBRARY_SOURCES "logger.cpp" "logger_spdlog.cpp")
set(LIBRARY_HEADERS "logger.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("logger" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("logger" PUBLIC ${LIBRARY_INCLUDES})

if(${USE_LOGGER_SPDLOG})
  target_link_libraries("logger" PRIVATE fmt::fmt spdlog::spdlog)
else ()
  target_link_libraries("logger" PRIVATE fmt::fmt)
endif()

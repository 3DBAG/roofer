set(LIBRARY_SOURCES "logger.cpp" "logger_spdlog.cpp")
set(LIBRARY_HEADERS "${ROOFER_INCLUDE_DIR}/roofer/logger/logger.h")
set(LIBRARY_INCLUDES  "${ROOFER_INCLUDE_DIR}"
                      "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("logger" OBJECT ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("logger" PUBLIC ${LIBRARY_INCLUDES})

if(${USE_LOGGER_SPDLOG})
  target_link_libraries("logger" PRIVATE fmt::fmt spdlog::spdlog)
else ()
  target_link_libraries("logger" PRIVATE fmt::fmt)
endif()

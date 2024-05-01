set(LIBRARY_SOURCES "logger.cpp" "logger_spdlog.cpp" "logger_rerun.cpp")
set(LIBRARY_HEADERS "logger.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("logger" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("logger" PUBLIC ${LIBRARY_INCLUDES})

if (${USE_LOGGER_SPDLOG})
    target_link_libraries(
            "logger"
            PRIVATE spdlog::spdlog
    )
elseif (${USE_LOGGER_RERUN})
    target_link_libraries(
            "logger"
            PRIVATE rerun_sdk
    )
endif ()
set(LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/JsonReader.cpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/JsonWriter.cpp")
set(LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/JsonReader.h"
                    "${CMAKE_CURRENT_SOURCE_DIR}/JsonWriter.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("LibIo" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("LibIo" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("LibIo" PRIVATE nlohmann_json::nlohmann_json)

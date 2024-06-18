set(LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/datastructures.cpp")
set(LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/datastructures.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("LibDatastructures" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("LibDatastructures" PUBLIC ${LIBRARY_INCLUDES})

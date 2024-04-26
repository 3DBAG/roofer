set(LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/reconstruct.cpp")
set(LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/reconstruct.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("LibReconstruction" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("LibReconstruction" PUBLIC ${LIBRARY_INCLUDES})

set(LIBRARY_SOURCES "library.cpp")
set(LIBRARY_HEADERS "library.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include")

add_library("LibRoofer" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("LibRoofer" PUBLIC ${LIBRARY_INCLUDES})

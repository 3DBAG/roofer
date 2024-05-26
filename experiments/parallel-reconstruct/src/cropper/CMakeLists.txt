set(LIBRARY_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/crop.cpp")
set(LIBRARY_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/crop.h")
set(LIBRARY_INCLUDES "./" "${CMAKE_BINARY_DIR}/configured_files/include" "LibDatastructures")

add_library("LibCropper" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})
target_include_directories("LibCropper" PUBLIC ${LIBRARY_INCLUDES})
target_link_libraries("LibCropper" PUBLIC "LibDatastructures" "LibIo")

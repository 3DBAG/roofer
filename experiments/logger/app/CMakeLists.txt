set(EXE_SOURCES
    "main.cpp")
set(EXE_INCLUDES
    "./")

add_executable(${EXECUTABLE_NAME} ${EXE_SOURCES})
target_include_directories(
    ${EXECUTABLE_NAME}
    PUBLIC ${EXE_INCLUDES})
target_link_libraries(
    ${EXECUTABLE_NAME}
    PRIVATE "LibRoofer"
            "LibLogger"
            fmt::fmt)
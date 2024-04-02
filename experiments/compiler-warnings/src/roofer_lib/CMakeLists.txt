set(LIBRARY_SOURCES "library.cpp")
set(LIBRARY_HEADERS "library.h")

add_library("RooferLib" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

if(${ENABLE_WARNINGS})
    target_set_warnings(
        TARGET
        "RooferLib"
        ENABLE
        ${ENABLE_WARNINGS}
        AS_ERRORS
        ${ENABLE_WARNINGS_AS_ERRORS})
endif()

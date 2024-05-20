set(LIBRARY_SOURCES "library.cpp")
set(LIBRARY_HEADERS "library.h")

add_library("RooferLib" STATIC ${LIBRARY_SOURCES} ${LIBRARY_HEADERS})

if(${ENABLE_CLANG_TIDY})
    add_clang_tidy_to_target("RooferLib")
endif()
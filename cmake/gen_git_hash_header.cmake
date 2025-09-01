# gen_git_hash_header.cmake
execute_process(
    COMMAND git describe --match=NeVeRmAtCh --always --dirty
    OUTPUT_VARIABLE HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

file(WRITE ${OUT} "#pragma once\n")
file(APPEND ${OUT} "#define RF_GIT_HASH \"${HASH}\"\n")

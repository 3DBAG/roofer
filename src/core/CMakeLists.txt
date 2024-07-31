add_subdirectory(common)
add_subdirectory(logger)
add_subdirectory(reconstruction)

add_library(roofer-core)
target_link_libraries(roofer-core PUBLIC ptinpoly common logger reconstruction)

find_package(nlohmann_json CONFIG REQUIRED)

add_subdirectory(io)
add_subdirectory(misc)

add_library(roofer-extra)
target_link_libraries(roofer-extra PUBLIC io misc roofer-core)


#include_directories(../src)

# Download the rerun_sdk
include(FetchContent)
FetchContent_Declare(rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
FetchContent_MakeAvailable(rerun_sdk)

add_subdirectory(crop)
add_subdirectory(reconstruct)


# add polyscope
# if(${RF_BUILD_GUI})
#   add_subdirectory(external/polyscope)

#   add_executable(ransac_demo ransac_demo.cpp)
#   set_target_properties(
#     ransac_demo PROPERTIES 
#     CXX_STANDARD 20
#   )
#   target_link_libraries(ransac_demo PRIVATE roofer polyscope cmake_git_version_tracking)
# endif()
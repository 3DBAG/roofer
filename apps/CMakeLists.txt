include_directories(../src)

# Download the rerun_sdk
include(FetchContent)
FetchContent_Declare(rerun_sdk URL
    https://github.com/rerun-io/rerun/releases/latest/download/rerun_cpp_sdk.zip)
FetchContent_MakeAvailable(rerun_sdk)

add_executable(reconstruct reconstruct.cpp)
set_target_properties(
  reconstruct PROPERTIES 
  CXX_STANDARD 17
)
target_link_libraries(reconstruct PRIVATE roofer CGAL::CGAL_Core cmake_git_version_tracking rerun_sdk)

add_executable(reconstruct_api reconstruct_api.cpp)
set_target_properties(
        reconstruct_api PROPERTIES
        CXX_STANDARD 17
)
target_link_libraries(reconstruct_api PRIVATE roofer CGAL::CGAL_Core cmake_git_version_tracking rerun_sdk)

add_executable(crop crop.cpp)
set_target_properties(
  crop PROPERTIES 
  CXX_STANDARD 17
)
target_link_libraries(crop PRIVATE roofer cmake_git_version_tracking)

install(TARGETS crop DESTINATION bin)

# add polyscope
# if(${RF_BUILD_GUI})
#   add_subdirectory(external/polyscope)

#   add_executable(ransac_demo ransac_demo.cpp)
#   set_target_properties(
#     ransac_demo PROPERTIES 
#     CXX_STANDARD 17
#   )
#   target_link_libraries(ransac_demo PRIVATE roofer polyscope cmake_git_version_tracking)
# endif()
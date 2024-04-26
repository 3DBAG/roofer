include(Catch)

add_executable("IntegrationTestReconstruct" "test_reconstruct.cpp")
target_link_libraries("IntegrationTestReconstruct" PUBLIC "LibReconstruction")
target_link_libraries("IntegrationTestReconstruct" PRIVATE Catch2::Catch2WithMain)

add_executable("IntegrationTestCrop" "test_crop.cpp")
target_link_libraries("IntegrationTestCrop" PUBLIC "LibIo")
target_link_libraries("IntegrationTestCrop" PRIVATE Catch2::Catch2WithMain)

catch_discover_tests("IntegrationTestReconstruct")
catch_discover_tests("IntegrationTestCrop")
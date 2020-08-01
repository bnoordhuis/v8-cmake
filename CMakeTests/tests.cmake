### Top-level inclusion for test targets
message("tests")
include(${test_cmake_dir}/third_party_googletest.cmake)
include(${test_cmake_dir}/unittests.cmake)
include(${test_cmake_dir}/testing_gtest.cmake)

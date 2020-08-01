### Top-level inclusion for test targets
message("tests")
include(${test_cmake_dir}/third_party_googletest.cmake)
include(${test_cmake_dir}/unittests.cmake)
include(${test_cmake_dir}/testing_gtest.cmake)
include(${test_cmake_dir}/testing_gmock.cmake)

#
# From ${D}/BUILD.gn
#

add_library(wasm_test_common OBJECT)
target_sources(wasm_test_common PRIVATE
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-interpreter.cc
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-interpreter.h
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.cc
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.h
  )
target_compile_definitions(wasm_test_common
  PRIVATE
    ${v8_defines}
    ${external_config_defines}
    ${internal_config_base_defines}
  )
target_include_directories(wasm_test_common
  PRIVATE
    ${v8_includes}
    ${PROJECT_BINARY_DIR}/generated
  )
add_dependencies(wasm_test_common
  v8-bytecodes-builtin-list
  v8_torque_generated
  )

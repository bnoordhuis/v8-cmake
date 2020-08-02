### Top-level inclusion for test targets
message("tests")
include(${test_cmake_dir}/third_party_googletest.cmake)
include(${test_cmake_dir}/unittests.cmake)
include(${test_cmake_dir}/testing_gtest.cmake)
include(${test_cmake_dir}/testing_gmock.cmake)
include(${test_cmake_dir}/test_cctest.cmake)

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

add_library(wasm_module_runner STATIC)
target_sources(wasm_module_runner
  PRIVATE
    ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.cc
    ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.h
  )
target_compile_definitions(wasm_module_runner
  PRIVATE
    ${v8_defines}
    ${disable-exceptions-defines}
    ${internal_config_base_defines}
    ${external_config_defines}
  )
target_compile_options(wasm_module_runner
  PRIVATE
    ${disable-exceptions-flags}
    ${internal_config_base_flags}
    ${external_config_flags}
  )
target_include_directories(wasm_module_runner
  PRIVATE
    ${v8_includes}
    ${internal_config_base_includes}
    ${external_config_includes}
  )
target_link_libraries(wasm_module_runner PRIVATE
  v8-bytecodes-builtin-list # generate_bytecode_builtins_list
  v8_torque_generated # run_torque
  # v8_tracing
  )

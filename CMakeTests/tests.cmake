### Top-level inclusion for test targets

#
# Helper routines to construct and apply configs
#

function(config cfg)
  set(keywords DEFINES CFLAGS INCLUDES DEPCONFIG)
  cmake_parse_arguments(cfg "" "" "${keywords}" ${ARGN})
  if(cfg_UNPARSED_ARGUMENTS)
    message("bad invocation: config ${cfg} ${ARGN}")
  endif()
  set(${cfg}_declared 1 PARENT_SCOPE)
  set(${cfg}_defines ${${cfg}_defines} ${cfg_DEFINES} PARENT_SCOPE)
  set(${cfg}_cflags ${${cfg}_cflags} ${cfg_CFLAGS} PARENT_SCOPE)
  set(${cfg}_includes ${${cfg}_includes} ${cfg_INCLUDES} PARENT_SCOPE)
  set(${cfg}_depconfig ${${cfg}_depconfig} ${cfg_DEPCONFIG} PARENT_SCOPE)
endfunction()

function( target_config target )
  set(vis PRIVATE PUBLIC)
  cmake_parse_arguments( target_config "" "" "${vis}" ${ARGN})
  _get_dep_configs(cfgset ${target_config_PUBLIC})
  #message("public cfgset ${cfgset}")
  foreach(config IN LISTS cfgset)
    _add_config_to_target(${target} PUBLIC ${config})
  endforeach()
  _get_dep_configs(cfgset ${target_config_PRIVATE})
  #message("private cfgset ${cfgset}")
  foreach(config IN LISTS cfgset)
    #message("_add_config_to_target(${target} PRIVATE ${config})")
    _add_config_to_target(${target} PRIVATE ${config})
  endforeach()
endfunction()

function(_add_config_to_target target vis config)
  if(NOT ${config}_declared)
    message("not declared: ${config}")
  endif()
  #message("target_compile_definitions(${target} ${vis} ${${config}_defines})")
  target_compile_definitions(${target} ${vis} ${${config}_defines})
  #message("target_compile_options(${target} ${vis} ${${config}_flags})")
  target_compile_options(${target} ${vis} ${${config}_flags})
  #message("target_include_directories(${target} ${vis} ${${config}_includes})")
  target_include_directories(${target} ${vis} ${${config}_includes})
endfunction()

function(_get_dep_configs outvar )
  set(seen)
  set(cfgset ${ARGN})
  while(cfgset)
    list(POP_FRONT cfgset head)
    if(${head} IN_LIST seen)
      continue()
    endif()
    list(APPEND seen ${head})
    list(APPEND cfgset ${${head}_depconfig})
  endwhile()
  set(${outvar} ${seen} PARENT_SCOPE)
endfunction()

#
# From ${D}/BUILD.gn
#

config(v8_features
  DEFINES ${v8_defines}
  )

config(v8_enable_exceptions CFLAGS ${enable-exceptions-flags})
config(v8_disable_exceptions DEFINES ${disable-exceptions-defines} CFLAGS ${disable-exceptions-flags})

config(internal_config_base
  DEPCONFIG v8_tracing_config
  INCLUDES ${v8_includes}
  )

config(internal_config
  DEPCONFIG internal_config_base v8_header_features
  )

config(v8_tracing_config)

config(libplatform_config
  INCLUDES ${D}/include
  )

config(libbase_config)

config(cppgc_base_config
  DEFINES CPPGC_GAGED_HEAP
  )

config(lib_sampler_config
  INCLUDES ${D}/include
  )

config(external_config
  DEPCONFIG v8_header_features
  INCLUDES ${D}/include ${PROJECT_BINARY_DIR}/include
  )

config(v8_header_features)


add_library(wasm_test_common OBJECT
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-interpreter.cc
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-interpreter.h
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.cc
  ${PROJECT_SOURCE_DIR}/v8/test/common/wasm/wasm-module-runner.h
  )
target_config(wasm_test_common
  PRIVATE
    v8_features
    external_config
    internal_config_base
  )
target_include_directories(wasm_test_common
  PRIVATE
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

#
# torque_ls_base
#

add_library(torque_ls_base STATIC)
target_sources(torque_ls_base
  PRIVATE
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/globals.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/json-parser.cc
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/json-parser.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/json.cc
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/json.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/message-handler.cc
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/message-handler.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/message-macros.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/message-pipe.h
  ${PROJECT_SOURCE_DIR}/v8/src/torque/ls/message.h
  )
target_config(torque_ls_base
  PRIVATE internal_config
  )
target_link_libraries(torque_ls_base
  PRIVATE
    torque_base
  )

include(${test_cmake_dir}/third_party_googletest.cmake)
include(${test_cmake_dir}/unittests.cmake)
include(${test_cmake_dir}/testing_gtest.cmake)
include(${test_cmake_dir}/testing_gmock.cmake)
include(${test_cmake_dir}/test_cctest.cmake)
include(${test_cmake_dir}/third_party_zlib.cmake)



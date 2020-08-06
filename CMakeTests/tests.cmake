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
# From ${PROJECT_SOURCE_DIR}/v8/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8)

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

set(clang-or-not-win $<OR:${is-clang},$<NOT:${is-win}>>)
set(not-clang-and-win $<AND:$<NOT:${is-clang}>,${is-win}>)


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

#
# v8_cppgc_shared
#
add_library(v8_cppgc_shared OBJECT
  ${D}/src/heap/base/stack.cc
  ${D}/src/heap/base/stack.h
  $<${clang-or-not-win}:$<${is-x64}:${D}/src/heap/base/asm/x64/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-ia32}:${D}/src/heap/base/asm/ia32/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-arm}:${D}/src/heap/base/asm/arm/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-arm64}:${D}/src/heap/base/asm/arm64/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-ppc64}:${D}/src/heap/base/asm/ppc/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-s390x}:${D}/src/heap/base/asm/s390/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-mipsel}:${D}/src/heap/base/asm/mips/push_registers_asm.cc>>
  $<${clang-or-not-win}:$<${is-mips64el}:${D}/src/heap/base/asm/mips64/push_registers_asm.c>>
  $<${not-clang-and-win}:$<${is-x64}:${D}/src/heap/base/asm/x64/push_registers_masm.S>>
  $<${not-clang-and-win}:$<${is-ia32}:${D}/heap/base/asm/ia32/push_registers_masm.S>>
  $<${not-clang-and-win}:$<${is-arm64}:${D}/src/heap/base/asm/arm64/push_registers_masm.S>>
  )
target_config(v8_cppgc_shared
  PRIVATE internal_config
  )
target_link_libraries(v8_cppgc_shared
  PUBLIC
    v8_libbase
  )

#
# cppgc_base
#

add_library(cppgc_base OBJECT
  ${D}/include/cppgc/allocation.h
  ${D}/include/cppgc/common.h
  ${D}/include/cppgc/custom-space.h
  ${D}/include/cppgc/garbage-collected.h
  ${D}/include/cppgc/heap.h
  ${D}/include/cppgc/internal/api-constants.h
  ${D}/include/cppgc/internal/atomic-entry-flag.h
  ${D}/include/cppgc/internal/compiler-specific.h
  ${D}/include/cppgc/internal/finalizer-trait.h
  ${D}/include/cppgc/internal/gc-info.h
  ${D}/include/cppgc/internal/persistent-node.h
  ${D}/include/cppgc/internal/pointer-policies.h
  ${D}/include/cppgc/internal/prefinalizer-handler.h
  ${D}/include/cppgc/internal/process-heap.h
  ${D}/include/cppgc/internal/write-barrier.h
  ${D}/include/cppgc/liveness-broker.h
  ${D}/include/cppgc/liveness-broker.h
  ${D}/include/cppgc/macros.h
  ${D}/include/cppgc/member.h
  ${D}/include/cppgc/persistent.h
  ${D}/include/cppgc/platform.h
  ${D}/include/cppgc/prefinalizer.h
  ${D}/include/cppgc/source-location.h
  ${D}/include/cppgc/trace-trait.h
  ${D}/include/cppgc/type-traits.h
  ${D}/include/cppgc/visitor.h
  ${D}/include/v8config.h
  ${D}/src/heap/cppgc/allocation.cc
  ${D}/src/heap/cppgc/free-list.cc
  ${D}/src/heap/cppgc/free-list.h
  ${D}/src/heap/cppgc/garbage-collector.h
  ${D}/src/heap/cppgc/gc-info-table.cc
  ${D}/src/heap/cppgc/gc-info-table.h
  ${D}/src/heap/cppgc/gc-info.cc
  ${D}/src/heap/cppgc/gc-invoker.cc
  ${D}/src/heap/cppgc/gc-invoker.h
  ${D}/src/heap/cppgc/heap-base.cc
  ${D}/src/heap/cppgc/heap-base.h
  ${D}/src/heap/cppgc/heap-growing.cc
  ${D}/src/heap/cppgc/heap-growing.h
  ${D}/src/heap/cppgc/heap-object-header-inl.h
  ${D}/src/heap/cppgc/heap-object-header.cc
  ${D}/src/heap/cppgc/heap-object-header.h
  ${D}/src/heap/cppgc/heap-page-inl.h
  ${D}/src/heap/cppgc/heap-page.cc
  ${D}/src/heap/cppgc/heap-page.h
  ${D}/src/heap/cppgc/heap-space.cc
  ${D}/src/heap/cppgc/heap-space.h
  ${D}/src/heap/cppgc/heap-visitor.h
  ${D}/src/heap/cppgc/heap.cc
  ${D}/src/heap/cppgc/heap.h
  ${D}/src/heap/cppgc/liveness-broker.cc
  ${D}/src/heap/cppgc/logging.cc
  ${D}/src/heap/cppgc/marker.cc
  ${D}/src/heap/cppgc/marker.h
  ${D}/src/heap/cppgc/marking-visitor.cc
  ${D}/src/heap/cppgc/marking-visitor.h
  ${D}/src/heap/cppgc/object-allocator-inl.h
  ${D}/src/heap/cppgc/object-allocator.cc
  ${D}/src/heap/cppgc/object-allocator.h
  ${D}/src/heap/cppgc/object-start-bitmap-inl.h
  ${D}/src/heap/cppgc/object-start-bitmap.h
  ${D}/src/heap/cppgc/page-memory-inl.h
  ${D}/src/heap/cppgc/page-memory.cc
  ${D}/src/heap/cppgc/page-memory.h
  ${D}/src/heap/cppgc/persistent-node.cc
  ${D}/src/heap/cppgc/platform.cc
  ${D}/src/heap/cppgc/pointer-policies.cc
  ${D}/src/heap/cppgc/prefinalizer-handler.cc
  ${D}/src/heap/cppgc/prefinalizer-handler.h
  ${D}/src/heap/cppgc/process-heap.cc
  ${D}/src/heap/cppgc/raw-heap.cc
  ${D}/src/heap/cppgc/raw-heap.h
  ${D}/src/heap/cppgc/sanitizers.h
  ${D}/src/heap/cppgc/source-location.cc
  ${D}/src/heap/cppgc/stats-collector.cc
  ${D}/src/heap/cppgc/stats-collector.h
  ${D}/src/heap/cppgc/sweeper.cc
  ${D}/src/heap/cppgc/sweeper.h
  ${D}/src/heap/cppgc/task-handle.h
  ${D}/src/heap/cppgc/virtual-memory.cc
  ${D}/src/heap/cppgc/virtual-memory.h
  ${D}/src/heap/cppgc/visitor.cc
  ${D}/src/heap/cppgc/worklist.h
  ${D}/src/heap/cppgc/write-barrier.cc
  $<${cppgc_enable_caged_heap}:
  ${D}/include/cppgc/internal/caged-heap-local-data.h
  ${D}/src/heap/cppgc/caged-heap-local-data.cc
  ${D}/src/heap/cppgc/caged-heap.cc
  ${D}/src/heap/cppgc/caged-heap.h
  >
  )
target_config(cppgc_base
  PUBLIC v8_features v8_disable_exceptions internal_config cppgc_base_config
  )
target_link_libraries(cppgc_base
  PUBLIC v8_cppgc_shared v8_libbase
  )

include(${test_cmake_dir}/third_party_googletest.cmake)
include(${test_cmake_dir}/test_unittests.cmake)
include(${test_cmake_dir}/testing_gtest.cmake)
include(${test_cmake_dir}/testing_gmock.cmake)
#include(${test_cmake_dir}/test_cctest.cmake)
include(${test_cmake_dir}/third_party_zlib.cmake)
include(${test_cmake_dir}/third_party_zlib_google.cmake)



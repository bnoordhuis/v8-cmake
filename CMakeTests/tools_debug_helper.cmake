#
# From tools/debug_helper/BUILD.gn
#

set(D ${PROJECT_SOURCE_DIR}/v8/tools/debug_helper)

# alsy use internal_config
config(debug_helper_internal_config
  INCLUDES ${D} ${D}/../.. ${PROJECT_BINARY_DIR} # ${PBD}/../..
  )

# also use external_config
config(debug_helper_external_config
  INCLUDES ${D}
  )

set(debug_helper_generated_dir ${PROJECT_BINARY_DIR}/debug_helper)

add_library(v8_debug_helper STATIC
    ${torque_generated_dir}/class-debug-readers-tq.h
    ${torque_generated_dir}/instance-types-tq.h
    ${D}/debug-helper-internal.h
    ${D}/debug-helper.h
    ${D}/heap-constants.h

    ${torque_generated_dir}/class-debug-readers-tq.cc
    ${debug_helper_generated_dir}/heap-constants-gen.cc
    ${D}/compiler-types.cc
    ${D}/debug-helper-internal.cc
    ${D}/get-object-properties.cc
    ${D}/heap-constants.cc
    ${D}/list-object-classes.cc

    ${D}/debug-helper.h
)
target_config(v8_debug_helper
  PRIVATE v8_features v8_disable_exceptions internal_config debug_helper_internal_config
  PUBLIC external_config debug_helper_external_config
  )
target_link_libraries(v8_debug_helper
  PRIVATE
    v8_libbase
  )
add_dependencies(v8_debug_helper
  gen_heap_constants
  v8-bytecodes-builtin-list
  v8_torque_generated
)

add_custom_target(gen_heap_constants
  DEPENDS
    ${debug_helper_generated_dir}/heap-constants-gen.cc
  )
add_custom_command(
  OUTPUT ${debug_helper_generated_dir}/heap-constants-gen.cc
  COMMAND ${CMAKE_COMMAND} -E make_directory  ${debug_helper_generated_dir}
  COMMAND python ${D}/gen-heap-constants.py
  ${debug_helper_generated_dir}
  ${debug_helper_generated_dir}/heap-constants-gen.cc
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Action: gen_heap_constants"
  VERBATIM
)
add_dependencies(gen_heap_constants run_mkgrokdump)

add_custom_target(run_mkgrokdump
  DEPENDS ${debug_helper_generated_dir}/v8heapconst.py)
add_custom_command(
  OUTPUT ${debug_helper_generated_dir}/v8heapconst.py
  COMMAND ${CMAKE_COMMAND} -E make_directory ${debug_helper_generated_dir}
  COMMAND python ${PROJECT_SOURCE_DIR}/v8/tools/run.py
    ./mkgrokdump
    --outfile
    ${debug_helper_generated_dir}/v8heapconst.py
  WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
  COMMENT "Action: run_mkgrokdump"
  VERBATIM
)
add_dependencies(run_mkgrokdump mkgrokdump)

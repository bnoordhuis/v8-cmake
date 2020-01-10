function(v8_generate_builtins_list target-dir)
  set(directory ${target-dir}/builtins-generated)
  set(output ${directory}/bytecodes-builtins-list.h)
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E make_directory ${directory}
    OUTPUT ${directory}
    COMMENT "Generating ${directory}"
    VERBATIM)
  add_custom_command(
    COMMAND bytecode_builtins_list_generator ${output}
    DEPENDS ${directory}
    OUTPUT ${output}
    COMMENT "Generating ${output}"
    VERBATIM)
  add_library(v8-bytecodes-builtin-list INTERFACE)
  target_include_directories(v8-bytecodes-builtin-list INTERFACE ${target-dir})
  target_sources(v8-bytecodes-builtin-list INTERFACE ${output})
endfunction()

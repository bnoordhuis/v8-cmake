include_guard(GLOBAL)

function (check_python_module_exists result module)
  if (CACHE{result})
    return()
  endif()
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import ${module}"
    RESULT_VARIABLE return-code
    OUTPUT_QUIET
    ERROR_QUIET)
  if (NOT return-code)
    set(${result} ON CACHE INTERNAL "Does python module '${module}' exist")
  endif()
endfunction()

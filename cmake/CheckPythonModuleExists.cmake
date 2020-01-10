if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.10)
  include_guard(GLOBAL)
elseif (__check_python_module)
  return()
else()
  set(__check_python_module ON)
endif()

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

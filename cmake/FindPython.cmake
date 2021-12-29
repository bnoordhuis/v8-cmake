# This is a wrapper around FindPythonInterp so that older CMake versions
# can safely find_package(Python) when using v8, as this is the desired
# behavior.

if (NOT V8CMAKE_FIND_PYTHON)
  return()
endif()

include(FindPackageHandleStandardArgs)

if (NOT ${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS)
  set(${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS Interpreter Development)
endif()

if (${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_EXACT)
  list(APPEND find-args EXACT)
endif()

if (${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
  list(APPEND find-args QUIET)
endif()

if (${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED)
  list(APPEND find-args REQUIRED)
endif()

# When calling both PythonInterp and PythonLibs, we need to prioritize
# PythonInterp first
if ("Interpreter" IN_LIST ${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS)
  if (${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_Interpreter)
    list(APPEND find-args REQUIRED)
  endif()
  list(REMOVE_DUPLICATES find-args)
  find_package(PythonInterp ${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION} ${find-args} MODULE)
endif()

if (Development IN_LIST ${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS)
  if (${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED_Development)
    list(APPEND find-args REQUIRED)
  endif()
  list(REMOVE_DUPLICATES find-args)
  find_package(PythonLibs ${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION} ${find-args} MODULE)
endif()

set(Python_Interpreter_FOUND ${PYTHONINTERP_FOUND})
set(Python_Development_FOUND ${PYTHONLIBS_FOUND})
set(Python_EXECUTABLE ${PYTHON_EXECUTABLE} CACHE FILEPATH "Python Executable")
set(Python_VERSION ${PYTHON_VERSION_STRING})

find_package_handle_standard_args(Python
  REQUIRED_VARS Python_EXECUTABLE
  VERSION_VAR Python_VERSION
  HANDLE_COMPONENTS)

if (Interpreter IN_LIST ${CMAKE_FIND_PACKAGE_NAME}_FIND_COMPONENTS AND PYTHONINTERP_FOUND)
  add_executable(Python::Interpreter IMPORTED)
  set_target_properties(Python::Interpreter
    PROPERTIES
      IMPORTED_LOCATION "${PYTHON_EXECUTABLE}"
      VERSION "${PYTHON_VERSION_STRING}")
endif()

if (Python_Development_FOUND AND cmake-role STREQUAL "PROJECT")
  add_library(Python::Development GLOBAL INTERFACE IMPORTED)
  target_include_directories(Python::Devlopment INTERFACE ${PYTHON_INCLUDE_DIRS})
  target_link_libraries(Python::Development INTERFACE ${PYTHON_LIBRARIES})
  set_property(TARGET Python::Development PROPERTY VERSION ${PYTHONLIBS_VERSION_STRING})
endif()

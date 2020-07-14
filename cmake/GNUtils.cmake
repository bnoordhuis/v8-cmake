#
#   Macros to make conversion from BUILD.gn to CMakeLists.txt easier
#

#
# Prepends the base directory to items in a list
#
function(prepend_base_directory base var)
  set(_processed_list)
  foreach(dir IN LISTS ${var})
    if(IS_ABSOLUTE ${dir})
      list(APPEND _processed_list ${dir})
    else()
      list(APPEND _processed_list ${base}/${dir})
    endif()
  endforeach()
  set(${var} ${_processed_list} PARENT_SCOPE)
endfunction()

#
# Prepends a base directory to a set of files and adds them to a target.
# Lets the file set be cut/pasted from the v8 sources. Paths that are
#
function(target_relative_sources target base vis)
  set(_sources ${ARGN})
  prepend_base_directory(${base} _sources)
  #message("target_relative_sources ${target} ${vis} ${_sources}")
  target_sources(${target} ${vis} ${_sources})
endfunction()

#
# Adds a set of files as sources subject to a generator conditional.
# Lets the file set be cut/pasted from the v8 sources
#
function(target_conditional_relative_sources target base vis cond src)
  #message("target_conditional_relative_sources ${target} ${base} ${vis} ${cond} ${src}")
  set(_sources ${src})
  prepend_base_directory(${base} _sources)
  string(REPLACE _S_ "${_sources}" _replaced ${cond})
  #message("target_conditional_relative_sources ${target} ${vis} ${_replaced}")
  target_sources(${target} ${vis} ${_replaced})
  if (ARGN)
    target_conditional_relative_sources( ${target} ${base} ${vis} ${ARGN})
  endif()
endfunction()

#
# Configs are containers for defines/flags/include-directories. Configs
# are applied after target-specific defines/flags/include-directories are
# added to executables/libraries.
#
# BUGBUG - in GN there is a hierarchy where config information is inherited
# akin to cmake's PUBLIC/PRIVATE defines/includes/etc. The implementation
# does NOT do this, although it wouldn't be hard.
#
# One difficulty is that since we're doing this via variables, the ordering
# of config construction in CMakeLists.txt is important.
#
#   X is a config
#       X_defined indicates that X has been declared
#       X_<VIS>_defines is a set of definitions
#       X_<VIS>_cflags are the c flags used for X
#       X_<VIS>_include_dirs are the include directories
#
# Two special configs, features and toolchain are always applied first
# to a target
#

#
# Add defines to a config
#
function(config_defines config vis)
  #message("config_defines ${config} ${vis} ${ARGN}")
  #message("${config}_${vis}_defines")
  set(_list ${${config}_${vis}_defines} ${ARGN})
  set(${config}_${vis}_defines ${_list} PARENT_SCOPE)
  set(${config}_defined 1 PARENT_SCOPE)
endfunction()

#
# Add cflags to a config
#
function(config_cflags config vis)
  set(_list ${${config}_${vis}_cflags} ${ARGN})
  set(${config}_${vis}_cflags ${_list} PARENT_SCOPE)
  set(${config}_defined 1 PARENT_SCOPE)
endfunction()

#
# Add incldue dirs to a config
#
function(config_include_dirs config vis)
  #message( "config_include_dirs ${config} ${dirs} ${ARGN}")
  set(_list ${${config}_${vis}_include_dirs} ${ARGN})
  set(${config}_${vis}_include_dirs ${_list} PARENT_SCOPE)
  #message(" ${config}_${vis}_include_dirs ${${config}_${vis}_include_dirs}")
  set(${config}_defined 1 PARENT_SCOPE)
endfunction()

#
# Adds config info to a target
#
function(target_config target base)
  #message("target_config ${target} ${ARGN}")
  set(_configs features toolchain ${ARGN})
  foreach(config IN LISTS _configs)
    if(NOT ${config}_defined EQUAL 1)
      message("WARNING: ${config} not defined")
    endif()
    _target_config_vis(${target} ${base} ${config} PUBLIC)
    _target_config_vis(${target} ${base} ${config} PRIVATE)
  endforeach()
endfunction()

function(_target_config_vis target base config vis)
  if(DEFINED ${config}_${vis}_defines)
    #message(" target_compile_definitions(${target} ${vis} ${${config}_${vis}_defines})")
    target_compile_definitions(${target} ${vis} ${${config}_${vis}_defines})
  endif()
  if(DEFINED ${config}_${vis}_cflags)
    #message(" target_compile_options(${target} ${${config}_${vis}_cflags})")
    target_compile_options(${target} ${vis} ${${config}_${vis}_cflags})
  endif()
  if(DEFINED ${config}_${vis}_include_dirs)
    #message(" target_include_directories(${target} ${${config}_${vis}_include_dirs})")
    prepend_base_directory(${base} ${config}_${vis}_include_dirs)
    target_include_directories(${target} ${vis} ${${config}_${vis}_include_dirs})
  endif()
endfunction()
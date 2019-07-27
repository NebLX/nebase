
find_package(PkgConfig REQUIRED)

# Unsets the given variables
macro(_import_xconfig_unset var)
  set(${var} "" CACHE INTERNAL "")
endmacro()

macro(_import_xconfig_set var value)
  set(${var} ${value} CACHE INTERNAL "")
endmacro()

macro(_import_xconfig_parse_libs _libs _prefix)
  _import_xconfig_unset(${_prefix}_LIBRARIES)
  _import_xconfig_unset(${_prefix}_LIBRARY_DIRS)
  _import_xconfig_unset(${_prefix}_LDFLAGS)
  _import_xconfig_unset(${_prefix}_LDFLAGS_OTHER)

  set(_arglist "${_libs}")
  foreach(_arg IN LISTS _arglist)
    separate_arguments(_arg)
    foreach(_opt IN LISTS _arg)
      list(APPEND ${_prefix}_LDFLAGS ${_opt})
      if(_opt MATCHES "^-l")
        string(REPLACE "-l" "" _opt_out "${_opt}")
        list(APPEND ${_prefix}_LIBRARIES ${_opt_out})
        continue()
      endif()
      if(_opt MATCHES "^-L")
        string(REPLACE "-L" "" _opt_out "${_opt}")
        list(APPEND ${_prefix}_LIBRARY_DIRS ${_opt_out})
        continue()
      endif()
      list(APPEND ${_prefix}_LDFLAGS_OTHER ${_opt})
    endforeach()
  endforeach()
endmacro()

macro(_import_xconfig_parse_cflags _cflags _prefix)
  _import_xconfig_unset(${_prefix}_INCLUDE_DIRS)
  _import_xconfig_unset(${_prefix}_CFLAGS)
  _import_xconfig_unset(${_prefix}_CFLAGS_OTHER)

  set(_arglist "${_cflags}")
  foreach(_arg IN LISTS _arglist)
    separate_arguments(_arg)
    foreach(_opt IN LISTS _arg)
      list(APPEND ${_prefix}_CFLAGS ${_opt})
      if(_opt MATCHES "^-I")
        string(REPLACE "-I" "" _opt_out "${_opt}")
        list(APPEND ${_prefix}_INCLUDE_DIRS ${_opt_out})
        continue()
      endif()
      list(APPEND ${_prefix}_CFLAGS_OTHER ${_opt})
    endforeach()
  endforeach()
endmacro()

macro(_import_xconfig_parse_options _pkgconfig_modules _xconfig_executables _xconfig_opt_libs _xconfig_opt_cflags _xconfig_cc_ldflags _xconfig_cc_cflags _no_cmake_path _no_cmake_environment_path _imp_target _imp_target_global)
  set(${_pkgconfig_modules} "")
  set(${_xconfig_executables} "")
  set(${_xconfig_opt_libs} "")
  set(${_xconfig_opt_cflags} "")
  set(${_xconfig_cc_ldflags} "")
  set(${_xconfig_cc_cflags} "")
  set(${_no_cmake_path} "")
  set(${_no_cmake_environment_path} "")
  set(${_imp_target} "")
  set(${_imp_target_global} "")
  set(_extra_args "")

  set(_nlist "_extra_args")
  foreach(_arg ${ARGN})
    if(_arg STREQUAL "PKGCONFIG_MODULES")
      set(_nlist "${_pkgconfig_modules}")
      continue()
    endif()
    if(_arg STREQUAL "XCONFIG_EXECUTABLES")
      set(_nlist "${_xconfig_executables}")
      continue()
    endif()
    if(_arg STREQUAL "XCONFIG_OPT_LIBS")
      set(_nlist "${_xconfig_opt_libs}")
      continue()
    endif()
    if(_arg STREQUAL "XCONFIG_OPT_CFLAGS")
      set(_nlist "${_xconfig_opt_cflags}")
      continue()
    endif()
    if(_arg STREQUAL "XCONFIG_CC_LDFLAGS")
      set(_nlist "${_xconfig_cc_ldflags}")
      continue()
    endif()
    if(_arg STREQUAL "XCONFIG_CC_CFLAGS")
      set(_nlist "${_xconfig_cc_cflags}")
      continue()
    endif()
    if(_arg STREQUAL "NO_CMAKE_PATH")
      set(${_no_cmake_path} "NO_CMAKE_PATH")
      set(_nlist "_extra_args")
      continue()
    endif()
    if(_arg STREQUAL "NO_CMAKE_ENVIRONMENT_PATH")
      set(${_no_cmake_environment_path} "NO_CMAKE_ENVIRONMENT_PATH")
      set(_nlist "_extra_args")
      continue()
    endif()
    if(_arg STREQUAL "IMPORTED_TARGET")
      set(${_imp_target} "IMPORTED_TARGET")
      set(_nlist "_extra_args")
      continue()
    endif()
    if(_arg STREQUAL "GLOBAL")
      set(${_imp_target_global} "GLOBAL")
      set(_nlist "_extra_args")
      continue()
    endif()

    if (${_imp_target_global} AND NOT ${_imp_target})
      message(SEND_ERROR "the argument GLOBAL may only be used together with IMPORTED_TARGET")
    endif()

    list(APPEND ${_nlist} ${_arg})
  endforeach()

  if(_extra_args)
    message(WARNING "nebase_import_from_xconfig: unused extra args: ${_extra_args}")
  endif()
endmacro()

# create an imported target from all the information as returned by pkg-config
function(_import_xconfig_create_imp_target _prefix _imp_target_global)
  # only create the target if it is linkable, i.e. no executables
  if (NOT TARGET PkgConfig::${_prefix}
      AND ( ${_prefix}_INCLUDE_DIRS OR ${_prefix}_LINK_LIBRARIES OR ${_prefix}_CFLAGS_OTHER ))
    add_library(PkgConfig::${_prefix} INTERFACE IMPORTED ${_imp_target_global})

    if(${_prefix}_INCLUDE_DIRS)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_INCLUDE_DIRECTORIES "${${_prefix}_INCLUDE_DIRS}")
    endif()
    if(${_prefix}_LINK_LIBRARIES)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_LINK_LIBRARIES "${${_prefix}_LINK_LIBRARIES}")
    endif()
    if(${_prefix}_CFLAGS_OTHER)
      set_property(TARGET PkgConfig::${_prefix} PROPERTY
                   INTERFACE_COMPILE_OPTIONS "${${_prefix}_CFLAGS_OTHER}")
    endif()
  endif()
endfunction()

# scan the LDFLAGS returned by pkg-config for library directories and
# libraries, figure out the absolute paths of that libraries in the
# given directories
function(_import_xconfig_find_libs _prefix _no_cmake_path _no_cmake_environment_path)
  unset(_libs)
  unset(_search_paths)

  foreach (flag IN LISTS ${_prefix}_LDFLAGS)
    if (flag MATCHES "^-L(.*)")
      list(APPEND _search_paths ${CMAKE_MATCH_1})
      continue()
    endif()
    if (flag MATCHES "^-l(.*)")
      set(_pkg_search "${CMAKE_MATCH_1}")
    else()
      continue()
    endif()

    if(_search_paths)
        # Firstly search in -L paths
        find_library(pkgcfg_lib_${_prefix}_${_pkg_search}
                     NAMES ${_pkg_search}
                     HINTS ${_search_paths} NO_DEFAULT_PATH)
    endif()
    find_library(pkgcfg_lib_${_prefix}_${_pkg_search}
                 NAMES ${_pkg_search}
                 ${_no_cmake_path} ${_no_cmake_environment_path})
    list(APPEND _libs "${pkgcfg_lib_${_prefix}_${_pkg_search}}")
  endforeach()

  set(${_prefix}_LINK_LIBRARIES "${_libs}" PARENT_SCOPE)
endfunction()

#[========================================[.rst:
.. command:: nebase_import_from_xconfig

  The behavior of this command is the same as :command:`pkg_search_module` in
  FindPkgConfig module, but will fall back to use xconfig if no .pc found. ::

    pkg_search_module(<prefix>
                      [NO_CMAKE_PATH]
                      [NO_CMAKE_ENVIRONMENT_PATH]
                      [IMPORTED_TARGET [GLOBAL]]
                      [PKGCONFIG_MODULES [<moduleSpec>...]]
                      [XCONFIG_EXECUTABLES [<exectuables>...]]
                      [XCONFIG_OPT_CFLAGS [<cflags cmd option>...]]
                      [XCONFIG_OPT_LIBS [<libs cmd option>...]]
                      [XCONFIG_CC_CFLAGS [<cc cflags>...]]
                      [XCONFIG_CC_LDFLAGS [<cc ldflags>...]])
#]========================================]
macro(nebase_import_from_xconfig _prefix)
  _import_xconfig_parse_options(_pkgconfig_modules _xconfig_executables _xconfig_opt_libs _xconfig_opt_cflags _xconfig_cc_ldflags _xconfig_cc_cflags _xconfig_no_cmake_path _xconfig_no_cmake_environment_path _xconfig_imp_target _xconfig_imp_target_global ${ARGN})

  set(${_prefix}_FOUND "")

  if(_pkgconfig_modules)
    pkg_search_module(${_prefix}
      "${_xconfig_no_cmake_path}" "${_xconfig_no_cmake_environment_path}"
      "${_xconfig_imp_target}" "${_xconfig_imp_target_global}"
      ${_pkgconfig_modules})
  endif()
  if(NOT ${_prefix}_FOUND)
    if(_xconfig_executables)
      unset(_xconfig_cmd CACHE)
      find_program(_xconfig_cmd NAMES ${_xconfig_executables}
        ${_xconfig_no_cmake_path} ${_xconfig_no_cmake_environment_path})
      if(_xconfig_cmd)
        unset(_xconfig_result)
        if(NOT _xconfig_opt_cflags)
          set(_xconfig_opt_cflags "--cflags")
        endif()
        execute_process(COMMAND ${_xconfig_cmd} ${_xconfig_opt_cflags}
          RESULT_VARIABLE _xconfig_result
          OUTPUT_VARIABLE _xconfig_cc_cflags OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _xconfig_result EQUAL 0)
          message(FATAL_ERROR "Failed to get run ${_xconfig_cmd} ${_xconfig_opt_cflags}")
        endif()

        unset(_xconfig_result)
        if(NOT _xconfig_opt_libs)
          set(_xconfig_opt_libs "--libs")
        endif()
        execute_process(COMMAND ${_xconfig_cmd} ${_xconfig_opt_libs}
          RESULT_VARIABLE _xconfig_result
          OUTPUT_VARIABLE _xconfig_cc_ldflags OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _xconfig_result EQUAL 0)
          message(FATAL_ERROR "Failed to get run ${_xconfig_cmd} ${_xconfig_opt_libs}")
        endif()
      elseif(NOT _xconfig_cc_cflags AND NOT _xconfig_cc_ldflags)
        message(FATAL_ERROR "Failed to find xconfig program ${_xconfig_executables}")
      endif()
    endif()

    if(_xconfig_cc_cflags)
      _import_xconfig_parse_cflags("${_xconfig_cc_cflags}" "${_prefix}")
    endif()
    if(_xconfig_cc_ldflags)
      _import_xconfig_parse_libs("${_xconfig_cc_ldflags}" "${_prefix}")
    endif()

    _import_xconfig_find_libs(${_prefix} "${_xconfig_no_cmake_path}" "${_xconfig_no_cmake_environment_path}")
    if(${_xconfig_imp_target})
      _import_xconfig_create_imp_target("${_prefix}" "${_xconfig_imp_target_global}")
    endif()
  endif()
endmacro()

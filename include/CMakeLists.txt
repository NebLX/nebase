
macro(_Neb_get_include_str _include _var)
  set(${_var} "")
  set(SAFE_CMAKE_REQUIRED_QUIET ${CMAKE_REQUIRED_QUIET})
  set(CMAKE_REQUIRED_QUIET ON)
  include(CheckIncludeFile)
  unset(NEB_HAS_INCLUDE CACHE)
  CHECK_INCLUDE_FILE("${_include}" NEB_HAS_INCLUDE)
  if(NEB_HAS_INCLUDE)
    message(STATUS "Found platform header file ${_include}")
    set(${_var} "#include <${_include}>")
  endif()
  set(CMAKE_REQUIRED_QUIET ${SAFE_CMAKE_REQUIRED_QUIET})
endmacro()

_Neb_get_include_str("sys/cdefs.h" INCLUDE_ALL_SYS_CDEFS_H)
_Neb_get_include_str("TargetConditionals.h" INCLUDE_MACOS_TARGET_CONDITIONALS_H)

include(${CMAKE_SOURCE_DIR}/cmake/CompileTests/struct_initializer.cmake)

configure_file(platform.h.in platform.h @ONLY)

install(DIRECTORY nebase DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/platform.h" DESTINATION "${CMAKE_INSTALL_FULL_INCLUDEDIR}/nebase")


macro(_NebDetectLinker_get_linker_info _LINKER_VERSION_STRING)
  if("${_LINKER_VERSION_STRING}" MATCHES "^GNU ld ([^ ]+) \\[FreeBSD\\].*")
    set(NeBase_LINKER_ID "GNU.bfd")
    set(NeBase_LINKER_VERSION ${CMAKE_MATCH_1})
  elseif("${_LINKER_VERSION_STRING}" MATCHES "^GNU ld.* ([^ ]+)$")
    set(NeBase_LINKER_ID "GNU.bfd")
    set(NeBase_LINKER_VERSION ${CMAKE_MATCH_1})
  elseif("${_LINKER_VERSION_STRING}" MATCHES "^GNU gold.* ([^ ]+)$")
    set(NeBase_LINKER_ID "GNU.gold")
    set(NeBase_LINKER_VERSION ${CMAKE_MATCH_1})
  elseif("${_LINKER_VERSION_STRING}" MATCHES "^LLD ([^ ]+).*")
    set(NeBase_LINKER_ID "LLVM.lld")
    set(NeBase_LINKER_VERSION ${CMAKE_MATCH_1})
  elseif("${_LINKER_VERSION_STRING}" MATCHES ".*Solaris Link Editors: ([^ ]+)")
    set(NeBase_LINKER_ID "SUN.ld")
    set(NeBase_LINKER_VERSION ${CMAKE_MATCH_1})
  else()
    message(WARNING "Unsupported ld version:\n${_LINKER_VERSION_STRING}")
  endif()
endmacro(_NebDetectLinker_get_linker_info)

macro(_NebDetectLinker_detect_linker_id)
  message(STATUS "Detecting linker version info")

  # check -fuse-ld=XXX in CFLAGS
  if(NOT $ENV{CFLAGS} STREQUAL "")
    string(REGEX MATCHALL "-fuse-ld=[^ ]+" CFLAGS_USE_LDS $ENV{CFLAGS})
    if(CFLAGS_USE_LDS)
      list(GET CFLAGS_USE_LDS -1 CFLAGS_LD)
      string(SUBSTRING ${CFLAGS_LD} 9 -1 LD_TYPE)
      if("${LD_TYPE}" STREQUAL "bfd")
        set(NeBase_LINKER_ID "GNU.bfd")
      elseif("${LD_TYPE}" STREQUAL "gold")
        set(NeBase_LINKER_ID "GNU.gold")
      elseif("${LD_TYPE}" STREQUAL "lld")
        set(NeBase_LINKER_ID "LLVM.lld")
      else()
        message(WARNING "Unsupported -fuse-ld value ${LD_TYPE}")
      endif()

      if(NOT NeBase_LINKER_ID STREQUAL "")
        message(STATUS "The linker is ${NeBase_LINKER_ID}")
        return()
      endif()
    endif()
  endif()

  # check ld command
  set(LD_EXE "ld")
  if(NOT $ENV{LD} STREQUAL "")
    set(LD_EXE $ENV{LD})
  endif()

  if(OS_DARWIN)
    execute_process(COMMAND ${LD_EXE} -v
      RESULT_VARIABLE LD_VERSION_RESULT
      ERROR_VARIABLE LD_VERSION_OUTPUT
      OUTPUT_QUIET)
    if(LD_VERSION_RESULT EQUAL 0)
      string(REGEX REPLACE ".*PROJECT:([^\n-]+)-([^\n]+)[\n].*" "\\1;\\2" LD_VERSION_LIST "${LD_VERSION_OUTPUT}")
      list(GET LD_VERSION_LIST 0 LD_PROJECT)
      set(NeBase_LINKER_ID "Apple.${LD_PROJECT}") # set it to the real ld project name, i.e. ld64
      list(GET LD_VERSION_LIST 1 NeBase_LINKER_VERSION)
      message(STATUS "The linker is ${NeBase_LINKER_ID} version ${NeBase_LINKER_VERSION}")
    else()
      set(NeBase_LINKER_ID "Apple.ld")
      message(STATUS "The linker is ${NeBase_LINKER_ID}")
    endif()
    return()
  elseif(OS_ILLUMOS)
    execute_process(COMMAND ${LD_EXE} -V
      RESULT_VARIABLE LD_VERSION_RESULT
      ERROR_VARIABLE LD_VERSION_OUTPUT
      OUTPUT_QUIET)
    set(NeBase_LINKER_ID "SUN.ld")
    if(LD_VERSION_RESULT EQUAL 0)
      string(REGEX REPLACE ".*Solaris Link Editors: ([^ ]+) \\(illumos\\)" "\\1" NeBase_LINKER_VERSION "${LD_VERSION_OUTPUT}")
      message(STATUS "The linker is ${NeBase_LINKER_ID} version ${NeBase_LINKER_VERSION}")
    else()
      message(STATUS "The linker is ${NeBase_LINKER_ID}")
    endif()
    return()
  endif()

  execute_process(COMMAND ${LD_EXE} -V
    RESULT_VARIABLE LD_VERSION_RESULT
    OUTPUT_VARIABLE LD_VERSION_OUTPUT
    ERROR_QUIET)
  if(LD_VERSION_RESULT EQUAL 0)
    string(REGEX MATCH "^[^\n]+" LD_VERSION_STRING "${LD_VERSION_OUTPUT}")
    _NebDetectLinker_get_linker_info("${LD_VERSION_STRING}")
    message(STATUS "The linker is ${NeBase_LINKER_ID} version ${NeBase_LINKER_VERSION}")
    return()
  endif()

  message(FATAL_ERROR "Failed to get linker version info")
endmacro(_NebDetectLinker_detect_linker_id)

_NebDetectLinker_detect_linker_id()

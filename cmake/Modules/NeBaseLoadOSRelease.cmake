
macro(_NebLoadOSRelease_get _name)
  execute_process(COMMAND "${CMAKE_CURRENT_LIST_DIR}/GetFromOSRelease.sh" "${_name}"
    RESULT_VARIABLE _OS_RELEASE_RESULT
    OUTPUT_VARIABLE _OS_RELEASE_OUTPUT
    ERROR_QUIET)
  if(_OS_RELEASE_RESULT EQUAL 0)
    set(OS_RELEASE_${_name} "${_OS_RELEASE_OUTPUT}")
  endif()
endmacro()

set(_OS_RELEASE_NAMES "ID;VERSION_ID")
foreach(_name IN LISTS _OS_RELEASE_NAMES)
  _NebLoadOSRelease_get(${_name})
endforeach()

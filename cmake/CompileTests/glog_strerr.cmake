
try_run(GLOG_STRERR_RUN_RET GLOG_STRERR_COMPILE_RET
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompileTests/glog_strerr.c
  CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${GLIB2_INCLUDE_DIRS}" "-DLINK_DIRECTORIES:STRING=${GLIB2_LIBRARY_DIRS}"
  LINK_LIBRARIES ${GLIB2_LIBRARIES}
)

if(NOT GLOG_STRERR_COMPILE_RET)
  message(FATAL_ERROR "Failed to compile the glog_strerr test program")
else()
  if(GLOG_STRERR_RUN_RET STREQUAL "FAILED_TO_RUN")
    message(FATAL_ERROR "Failed to run the glog_strerr test program")
  elseif(GLOG_STRERR_RUN_RET EQUAL 0)
    set(GLOG_SUPPORT_STRERR ON CACHE INTERNAL "glog %m support")
  endif()

  if(GLOG_SUPPORT_STRERR)
    message(STATUS "glog strerr is supported")
  else()
    message(STATUS "glog strerr is not supported. we will parse %m in ourself code")
  endif()
endif()


try_run(PRINTF_STRERR_RUN_RET PRINTF_STRERR_COMPILE_RET
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/CompileTests/printf_strerr.c
  RUN_OUTPUT_VARIABLE PRINTF_STRERR_RUN_OUTPUT
)

unset(PRINTF_SUPPORT_STRERR)

if(NOT PRINTF_STRERR_COMPILE_RET)
  message(FATAL_ERROR "Failed to compile the printf_strerr test program")
else()
  if(PRINTF_STRERR_RUN_RET STREQUAL "FAILED_TO_RUN")
    message(FATAL_ERROR "Failed to run the printf_strerr test program")
  elseif(PRINTF_STRERR_RUN_RET EQUAL 0)
    string(STRIP "${PRINTF_STRERR_RUN_OUTPUT}" _LINE_OUTPUT)
    if(NOT _LINE_OUTPUT STREQUAL "m")
      set(PRINTF_SUPPORT_STRERR ON CACHE INTERNAL "printf %m support")
    endif()
  endif()

  if(PRINTF_SUPPORT_STRERR)
    message(STATUS "printf() has support for parsing %m")
  else()
    message(STATUS "printf() doesn't support parsing of %m")
  endif()
endif()


try_run(STRUCT_INITIALIZER_RUN_RET STRUCT_INITIALIZER_COMPILE_RET
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/CompileTests/struct_initializer.c
)

if(NOT STRUCT_INITIALIZER_COMPILE_RET)
  message(FATAL_ERROR "Failed to compile the struct_initializer test program")
else()
  if(STRUCT_INITIALIZER_RUN_RET STREQUAL "FAILED_TO_RUN")
    message(FATAL_ERROR "Failed to run the struct_initializer test program")
  elseif(STRUCT_INITIALIZER_RUN_RET EQUAL 0)
    set(NEB_STRUCT_INITIALIZER "{}" CACHE INTERNAL "struct initializer")
  else()
    set(NEB_STRUCT_INITIALIZER "{0}" CACHE INTERNAL "struct initializer")
  endif()
endif()


try_compile(STRUCT_INITIALIZER_COMPILE_RET
  ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_SOURCE_DIR}/cmake/CompileTests/struct_initializer.c)

if(NOT STRUCT_INITIALIZER_COMPILE_RET)
  set(NEB_STRUCT_INITIALIZER "{0}" CACHE INTERNAL "struct initializer")
else()
  set(NEB_STRUCT_INITIALIZER "{}" CACHE INTERNAL "struct initializer")
endif()

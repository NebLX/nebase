
set(_NeBaseCompatIncludes "")

if(OS_LINUX)
  set(_NebCompatOSIncludeDir "os/linux")
elseif(OS_ILLUMOS)
  set(_NebCompatOSIncludeDir "os/illumos")
elseif(OS_OPENBSD)
  set(_NebCompatOSIncludeDir "os/openbsd")
elseif(OS_DARWIN)
  set(_NebCompatOSIncludeDir "os/darwin")
else()
  set(_NebCompatOSIncludeDir "")
endif()

if(_NebCompatOSIncludeDir)
  list(APPEND _NeBaseCompatIncludes "${_NebCompatOSIncludeDir}")
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/${_NebCompatOSIncludeDir}")
endif(_NebCompatOSIncludeDir)

if(NOT HAVE_SYS_RBTREE)
  list(APPEND _NeBaseCompatIncludes "rbtree")
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/rbtree")

  add_library(compat_rbtree OBJECT rbtree/rb.c)
  target_include_directories(compat_rbtree BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/rbtree")
  target_compile_definitions(compat_rbtree PRIVATE -DHAVE_NBTOOL_CONFIG_H)
endif()

set(NeBaseCompatIncludes "${_NeBaseCompatIncludes}" PARENT_SCOPE)

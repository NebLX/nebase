
set(_NeBaseCompatIncludes "")

if(OS_LINUX)
  set(_NebCompatOSIncludeDir "os/linux")
elseif(OS_FREEBSD)
  set(_NebCompatOSIncludeDir "os/freebsd")
elseif(OS_NETBSD)
  set(_NebCompatOSIncludeDir "os/netbsd")
elseif(OS_DFLYBSD)
  set(_NebCompatOSIncludeDir "os/dragonfly")
elseif(OS_ILLUMOS)
  set(_NebCompatOSIncludeDir "os/illumos")
elseif(OS_OPENBSD)
  set(_NebCompatOSIncludeDir "os/openbsd")
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
  set_property(TARGET compat_rbtree PROPERTY C_CLANG_TIDY "")
  target_include_directories(compat_rbtree BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/rbtree")
  target_compile_definitions(compat_rbtree PRIVATE -DHAVE_NBTOOL_CONFIG_H)
endif(NOT HAVE_SYS_RBTREE)

if(NOT HAVE_SYS_OBSTACK)
  list(APPEND _NeBaseCompatIncludes "obstack")
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/obstack")

  add_library(compat_obstack OBJECT obstack/obstack.c obstack/handler.c)
  set_property(TARGET compat_obstack PROPERTY C_CLANG_TIDY "")
  target_include_directories(compat_obstack BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/obstack")
endif(NOT HAVE_SYS_OBSTACK)

if(TRUE)
  list(APPEND _NeBaseCompatIncludes "hashtable")
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/hashtable")

  add_library(compat_hashtable OBJECT hashtable/hash.c)
  set_property(TARGET compat_hashtable PROPERTY C_CLANG_TIDY "")
  target_include_directories(compat_hashtable BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/hashtable")
  target_compile_options(compat_hashtable PRIVATE -Wno-unused-variable)
endif(TRUE)

if(TRUE)
  list(APPEND _NeBaseCompatIncludes "net_radix")
  include_directories(BEFORE "${CMAKE_SOURCE_DIR}/include/nebase/compat/net_radix")

  add_library(compat_net_radix OBJECT net_radix/radix.c)
  set_property(TARGET compat_net_radix PROPERTY C_CLANG_TIDY "")
  target_compile_options(compat_net_radix PRIVATE -Wno-sign-compare)
endif(TRUE)

set(NeBaseCompatIncludes "${_NeBaseCompatIncludes}" PARENT_SCOPE)

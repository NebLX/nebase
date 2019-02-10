#
# The official FindThreads module won't work if we use -fsanitize=address,
# so we rewrite a simple version here
#

if(NOT TARGET NeBase::Threads)
  add_library(NeBase::Threads INTERFACE IMPORTED)

  if(OS_LINUX)
    set_property(TARGET NeBase::Threads PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
    set_property(TARGET NeBase::Threads PROPERTY INTERFACE_LINK_LIBRARIES "-pthread")
  elseif(OS_FREEBSD OR OS_DFLYBSD)
    set_property(TARGET NeBase::Threads PROPERTY INTERFACE_LINK_LIBRARIES "-lpthread")
  endif()
endif()

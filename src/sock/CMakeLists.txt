
add_library(sock_platform OBJECT
  $<$<BOOL:${OS_LINUX}>:linux.c>
  $<$<BOOL:${OS_FREEBSD}>:freebsd.c>
  $<$<BOOL:${OS_NETBSD}>:netbsd.c>
  $<$<BOOL:${OS_DFLYBSD}>:dflybsd.c>
  $<$<BOOL:${OS_OPENBSD}>:openbsd.c>
  $<$<BOOL:${OS_SOLARIS}>:solaris.c>
  $<$<BOOL:${OS_ILLUMOS}>:illumos.c>
  $<$<BOOL:${OS_DARWIN}>:xnu.c>
)
if(NOT CMAKE_VERSION VERSION_LESS "3.12.0")
  target_link_libraries(sock_platform
  INTERFACE
    $<$<BOOL:${OS_OPENBSD}>:kvm>
    $<$<BOOL:${OSTYPE_SUN}>:kstat>
  )
endif()

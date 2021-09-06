
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

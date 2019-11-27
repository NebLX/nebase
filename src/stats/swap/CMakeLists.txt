
add_library(stats_swap OBJECT
  $<$<BOOL:${OS_LINUX}>:linux_procfs.c>
  $<$<BOOL:${OS_FREEBSD}>:bsd_kvm.c>
  $<$<BOOL:${OS_NETBSD}>:bsd_swapctl.c>
  $<$<BOOL:${OS_OPENBSD}>:bsd_swapctl.c>
  $<$<BOOL:${OSTYPE_SUN}>:sunos_swapctl.c>
  $<$<BOOL:${OS_DARWIN}>:darwin_sysctl.c>
)

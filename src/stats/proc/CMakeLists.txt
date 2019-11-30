
add_library(stats_proc OBJECT
  $<$<BOOL:${OS_LINUX}>:linux_procfs.c>
  $<$<BOOL:${OS_FREEBSD}>:bsd_sysctl.c>
  $<$<BOOL:${OS_NETBSD}>:bsd_kvm.c>
  $<$<BOOL:${OS_OPENBSD}>:bsd_kvm.c>
  $<$<BOOL:${OSTYPE_SUN}>:sunos_procfs.c>
  $<$<BOOL:${OS_DARWIN}>:bsd_sysctl.c>
)

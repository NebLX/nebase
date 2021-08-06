
add_library(stats_proc OBJECT
  $<$<BOOL:${OS_LINUX}>:linux_procfs.c>
  $<$<BOOL:${OS_FREEBSD}>:bsd_sysctl.c>
  $<$<BOOL:${OS_DFLYBSD}>:bsd_sysctl.c>
  $<$<BOOL:${OS_NETBSD}>:bsd_kvm.c>
  $<$<BOOL:${OS_OPENBSD}>:bsd_kvm.c>
  $<$<BOOL:${OSTYPE_SUN}>:sunos_procfs.c>
  $<$<BOOL:${OS_DARWIN}>:mach_task.c>
)
target_link_libraries(stats_proc
PUBLIC
  $<$<BOOL:${OS_NETBSD}>:kvm>
)

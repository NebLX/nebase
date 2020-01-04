
add_subdirectory(evdp)
add_subdirectory(sock)
add_subdirectory(net)
add_subdirectory(str)
add_subdirectory(stats)

add_library(nebase SHARED
  $<$<NOT:$<BOOL:${HAVE_SYS_RBTREE}>>:$<TARGET_OBJECTS:compat_rbtree>>
  $<$<NOT:$<BOOL:${HAVE_SYS_OBSTACK}>>:$<TARGET_OBJECTS:compat_obstack>>
    # use TARGET_EXISTS if use cmake >= 3.12
  $<TARGET_OBJECTS:compat_hashtable>
  $<TARGET_OBJECTS:compat_net_radix>
  _init.c
  syslog.c
  events.c
  signal.c
  thread.c
  proc.c
  time.c
  file.c
  $<TARGET_OBJECTS:net>
  $<TARGET_OBJECTS:evdp>
  $<TARGET_OBJECTS:evdp_driver>
  $<$<BOOL:${WITH_CARES}>:resolver.c>
  $<TARGET_OBJECTS:sock>
  $<TARGET_OBJECTS:sock_platform>
  $<TARGET_OBJECTS:str>
  pidfile.c
  sem_posix.c
  sem_sysv.c
  pipe.c
  pty.c
  io.c
  random.c
  $<TARGET_OBJECTS:stats_swap>
  $<TARGET_OBJECTS:stats_proc>
  plugin.c
)
target_include_directories(nebase
PUBLIC
  $<$<BOOL:${WITH_CARES}>:${CARES_INCLUDE_DIRS}>
)
target_link_libraries(nebase
PRIVATE
  $<$<BOOL:${WITH_SYSTEMD}>:PkgConfig::SYSTEMD>
  $<$<BOOL:${WITH_GLIB2}>:PkgConfig::GLIB2>
  $<$<BOOL:${WITH_CARES}>:PkgConfig::CARES>
  $<$<BOOL:${OS_LINUX}>:PkgConfig::LIBBSD>
  $<$<BOOL:${OS_NETBSD}>:rt> # for sem_*
  $<$<BOOL:${OS_SOLARIS}>:kstat2>
  $<$<BOOL:${OS_HAIKU}>:bsd>
  $<$<BOOL:${OS_LINUX}>:util> # pty
  $<$<BOOL:${OSTYPE_BSD}>:util> # pty
  $<$<BOOL:${OS_LINUX}>:dl> # plugin
  # The following is introduced in objects, which could be removed if we use cmake >= 3.12
  $<$<BOOL:${USE_IO_URING}>:PkgConfig::URING> # evdp
  $<$<BOOL:${OS_LINUX}>:resolv> # net
  $<$<BOOL:${OSTYPE_SUN}>:resolv> # net
  $<$<BOOL:${OS_FREEBSD}>:kvm> # stats swap
  $<$<BOOL:${OS_DFLYBSD}>:kvm> # stats swap
  $<$<BOOL:${OS_NETBSD}>:kvm> # stats proc
  $<$<BOOL:${OS_OPENBSD}>:kvm> # sock
  $<$<BOOL:${OSTYPE_SUN}>:kstat> # sock
PUBLIC
  NebulaX::Threads
  $<$<BOOL:${OS_ILLUMOS}>:xnet> # use libxnet for socket api
)
set_target_properties(nebase PROPERTIES
  EXPORT_NAME NeBase
  SOVERSION 0
  VERSION ${PROJECT_VERSION})

install(TARGETS nebase EXPORT NeBaseTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_FULL_INCLUDEDIR}
)
export(EXPORT NeBaseTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/NeBaseTargets.cmake"
  NAMESPACE NebulaX::
)
install(EXPORT NeBaseTargets
  DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/${PROJECT_NAME}/Modules"
  NAMESPACE NebulaX::
  FILE NeBaseTargets.cmake
)

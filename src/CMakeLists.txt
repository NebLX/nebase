
add_subdirectory(evdp)
add_subdirectory(sock)

add_library(nebase SHARED
  $<$<NOT:$<BOOL:${HAVE_SYS_RBTREE}>>:$<TARGET_OBJECTS:compat_rbtree>>
    # use TARGET_EXISTS if use cmake >= 3.12
  syslog.c
  events.c
  signal.c
  thread.c
  proc.c
  time.c
  file.c
  $<TARGET_OBJECTS:evdp>
  sock.c
  $<TARGET_OBJECTS:sock_platform>
  pidfile.c
  sem_posix.c
  sem_sysv.c
  pipe.c
  io.c
)
target_link_libraries(nebase
PRIVATE
  $<$<BOOL:${WITH_SYSTEMD}>:PkgConfig::SYSTEMD>
  $<$<BOOL:${WITH_GLIB2}>:PkgConfig::GLIB2>
  $<$<BOOL:${OS_NETBSD}>:rt> # for sem_*
  $<$<BOOL:${OS_SOLARIS}>:kstat2>
  $<$<BOOL:${OS_HAIKU}>:bsd>
  # The following is introduced in objects, which could be removed if we use cmake >= 3.12
  $<$<BOOL:${USE_IO_URING}>:PkgConfig::URING>
  $<$<BOOL:${OS_OPENBSD}>:kvm>
  $<$<BOOL:${OSTYPE_SUN}>:kstat>
)
target_link_libraries(nebase
PUBLIC
  NebulaX::Threads
  $<$<BOOL:${OS_ILLUMOS}>:xnet> # for libxnet for socket api
)
set_target_properties(nebase PROPERTIES SOVERSION 0 VERSION ${PROJECT_VERSION})

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

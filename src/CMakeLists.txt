
add_library(nebase SHARED
  syslog.c
  events.c
  signal.c
  thread.c
  time.c
  file.c
  sock.c
  $<$<BOOL:${OS_LINUX}>:sock_linux.c>
  $<$<BOOL:${OS_FREEBSD}>:sock_freebsd.c>
  $<$<BOOL:${OS_NETBSD}>:sock_netbsd.c>
  $<$<BOOL:${OS_DFLYBSD}>:sock_dflybsd.c>
  $<$<BOOL:${OS_OPENBSD}>:sock_openbsd.c>
  dispatch.c
)
target_include_directories(nebase
PRIVATE
  ${SYSTEMD_INCLUDE_DIRS}
)
target_link_libraries(nebase
PRIVATE
  ${SYSTEMD_LIBRARIES}
  $<$<BOOL:${OS_SOLARIS}>:kstat2>
  $<$<BOOL:${OS_FREEBSD}>:Threads::Threads>
  $<$<BOOL:${OS_OPENBSD}>:kvm>
  $<$<BOOL:${OS_HAIKU}>:Threads::Threads>
  $<$<BOOL:${OS_HAIKU}>:bsd>
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

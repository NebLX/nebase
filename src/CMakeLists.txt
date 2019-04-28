
add_subdirectory(sock)

add_library(nebase SHARED
  syslog.c
  events.c
  signal.c
  thread.c
  time.c
  file.c
  sock.c
  $<TARGET_OBJECTS:sock_platform>
  dispatch/core.c
  dispatch/timer.c
  pidfile.c
  sem_posix.c
  sem_sysv.c
  pipe.c
  io.c
)
target_include_directories(nebase
PRIVATE
  ${SYSTEMD_INCLUDE_DIRS}
  ${GLIB2_INCLUDE_DIRS}
  $<$<BOOL:${OS_LINUX}>:${LIBBSD_INCLUDE_DIRS}>
)
target_link_libraries(nebase
PRIVATE
  NeBase::Threads
  ${SYSTEMD_LIBRARIES}
  ${GLIB2_LIBRARIES}
  $<$<BOOL:${OS_LINUX}>:${LIBBSD_LIBRARIES}>
  $<$<BOOL:${OS_NETBSD}>:rt> # for sem_*
  $<$<BOOL:${OS_SOLARIS}>:kstat2>
  $<$<BOOL:${OS_HAIKU}>:bsd>
  # The following is introduced in objects, which could be removed if we use cmake >= 3.12
  $<$<BOOL:${OS_OPENBSD}>:kvm>
  $<$<BOOL:${OS_SOLARIS}>:kstat>
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

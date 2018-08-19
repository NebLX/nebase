
add_library(nebase SHARED
  syslog.c
  events.c
  signal.c
)
target_include_directories(nebase
PRIVATE
  ${SYSTEMD_INCLUDE_DIRS}
)
target_link_libraries(nebase
PRIVATE
  ${SYSTEMD_LIBRARIES}
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

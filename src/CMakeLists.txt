
add_library(nebase SHARED syslog.c)
target_link_libraries(nebase
PUBLIC
  ${SYSTEMD_LIBRARIES}
)
set_target_properties(nebase PROPERTIES SOVERSION 0 VERSION ${PROJECT_VERSION})
install(TARGETS nebase LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR})


add_library(evdp OBJECT
  core.c
  $<$<BOOL:${OS_LINUX}>:_linux_epoll.c>
)
target_include_directories(evdp
PRIVATE
  ${GLIB2_INCLUDE_DIRS}
  $<$<BOOL:${OS_LINUX}>:${LIBBSD_INCLUDE_DIRS}>
)

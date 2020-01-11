
add_library(evdp_driver OBJECT
  queue.c
  source_itimer.c
  source_abstimer.c
  source_fd_util.c
  source_ro_fd.c
  source_os_fd.c
)
target_include_directories(evdp_driver PRIVATE "${CMAKE_SOURCE_DIR}/src/evdp")

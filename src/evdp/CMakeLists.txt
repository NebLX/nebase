
add_subdirectory(driver)

add_library(evdp OBJECT
  core.c
  timer.c
  sys_timer.c
  io_base.c
  io_socket.c
)

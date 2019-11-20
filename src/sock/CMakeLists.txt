
add_subdirectory(platform)

add_library(sock OBJECT
  common.c
  unix.c
  inet.c
  raw.c
  csum.c
)

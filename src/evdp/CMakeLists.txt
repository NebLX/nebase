
add_subdirectory(driver)

add_library(evdp OBJECT
  core.c
  timer.c
  helpers.c
)

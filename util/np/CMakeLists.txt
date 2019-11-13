
add_executable(np
  main.c
  ipv4.c
  ipv6.c
)
target_link_libraries(np PRIVATE $<TARGET_NAME:nebase>)

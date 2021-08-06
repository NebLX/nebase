
add_library(net OBJECT
  ipaddr.c
  radix.c
  device.c
)
target_link_libraries(net
PUBLIC
  $<$<BOOL:${OS_LINUX}>:resolv>
  $<$<BOOL:${OSTYPE_SUN}>:resolv>
)

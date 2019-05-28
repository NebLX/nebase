
unset(EVDP_PLATFORM_SRC)
if(USE_AIO_POLL)
  set(EVDP_PLATFORM_SRC "_linux_epoll.c")
elseif(OS_LINUX)
  set(EVDP_PLATFORM_SRC "_linux_epoll.c")
elseif(OSTYPE_BSD OR OS_DARWIN)
  set(EVDP_PLATFORM_SRC "_bsd_kevent.c")
elseif(OSTYPE_SUN)
  set(EVDP_PLATFORM_SRC "_sunos_port.c")
else()
  message(SEND_ERROR "evdp is not supported on this platform")
endif()

add_library(evdp OBJECT
  core.c
  ${EVDP_PLATFORM_SRC}
)

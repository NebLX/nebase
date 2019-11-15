
#ifndef NEB_COMPAT_SYS_SOCKET_H
#define NEB_COMPAT_SYS_SOCKET_H 1

#include_next <sys/socket.h>

#ifndef MSG_NOSIGNAL
# warning "MSG_NOSIGNAL is not supported on this platform"
# define MSG_NOSIGNAL 0
#endif

#endif


#ifndef NEB_COMPAT_FCNTL_H
#define NEB_COMPAT_FCNTL_H 1

#include_next <fcntl.h>

#ifndef O_DIRECTORY
# warning "O_DIRECTORY is not supported on this platform"
# define O_DIRECTORY 0
#endif

#endif


#include "core.h"

#include <sys/event.h>

int neb_evdp_source_fd_get_sockerr(const void *context, int *sockerr)
{
	const struct kevent *e = context;

	*sockerr = e->fflags;
	return 0;
}

int neb_evdp_source_fd_get_nread(const void *context, int *nbytes)
{
	const struct kevent *e = context;

	*nbytes = (int)e->data;
	return 0;
}

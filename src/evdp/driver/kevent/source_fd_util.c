
#include <nebase/evdp/io_common.h>

#include "core.h"

#include <sys/event.h>

int neb_evdp_sock_get_sockerr(const void *context, int *sockerr)
{
	const struct kevent *e = context;

	*sockerr = e->fflags;
	return 0;
}

int neb_evdp_io_get_nread(const void *context, int *nbytes)
{
	const struct kevent *e = context;

	*nbytes = (int)e->data;
	return 0;
}

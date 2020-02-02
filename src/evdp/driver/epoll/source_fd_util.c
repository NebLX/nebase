
#include <nebase/syslog.h>
#include <nebase/evdp/io_base.h>

#include "core.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

int neb_evdp_sock_get_sockerr(const void *context, int *sockerr)
{
	const int *fdp = context;

	socklen_t len = sizeof(int);
	if (getsockopt(*fdp, SOL_SOCKET, SO_ERROR, sockerr, &len) == -1) {
		neb_syslogl(LOG_ERR, "getsockopt(SO_ERROR): %m");
		return -1;
	}

	return 0;
}

int neb_evdp_io_get_nread(const void *context, int *nbytes)
{
	const int *fdp = context;

	if (ioctl(*fdp, FIONREAD, nbytes) == -1) {
		neb_syslogl(LOG_ERR, "ioctl(FIONREAD): %m");
		return -1;
	}

	return 0;
}

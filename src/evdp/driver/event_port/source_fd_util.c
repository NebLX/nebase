
#include <nebase/syslog.h>

#include <sys/socket.h>
#include <stropts.h>

int neb_evdp_source_fd_get_sockerr(const void *context, int *sockerr)
{
	const int *fdp = context;

	socklen_t len = sizeof(int);
	if (getsockopt(*fdp, SOL_SOCKET, SO_ERROR, sockerr, &len) == -1) {
		neb_syslogl(LOG_ERR, "getsockopt(SO_ERROR): %m");
		return -1;
	}

	return 0;
}

int neb_evdp_source_fd_get_nread(const void *context, int *nbytes)
{
	const int *fdp = context;

	if (ioctl(*fdp, I_NREAD, nbytes) == -1) {
		neb_syslogl(LOG_ERR, "ioctl(I_NREAD): %m");
		return -1;
	}

	return 0;
}

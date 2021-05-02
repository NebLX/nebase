
#include <nebase/syslog.h>

#include "freebsd.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socketvar.h>
#include <sys/param.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int sysctl_local_pcblist_loop_get(const char *mib, const char *path, kvaddr_t *sockptr, int *type)
{
	size_t sz = 0;
	if (sysctlbyname(mib, NULL, &sz, NULL, 0) == -1) {
		if (errno == ENOENT)
			return 0;
		neb_syslog(LOG_ERR, "Failed to get buffer size for sysctlbyname: %m");
		return -1;
	}
	void *v = malloc(sz);
	if (!v) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return -1;
	}

	/* NOTE according to sysctl(3), the previous returned buffer size will be large enough  */
	if (sysctlbyname(mib, v, &sz, NULL, 0) == -1) {
		neb_syslog(LOG_ERR, "Failed to get data(buflen: %zu) via sysctlbyname: %m", sz);
		free(v);
		return -1;
	}

	const struct xunpgen *xug = (struct xunpgen *)v;
	const struct xunpgen *exug = (struct xunpgen *)((char *)v + sz - sizeof(*exug));
	if (xug->xug_len != sizeof(*xug) ||
	    exug->xug_len != sizeof(*exug)) {
		neb_syslog(LOG_ERR, "sysctlbyname: returned struct xunpgen size mismatch");
		free(v);
		return -1;
	}

	const struct xunpcb *xup = (struct xunpcb *)((char *)xug + xug->xug_len);
	for (; (char *)xup < (char *)exug; xup = (struct xunpcb *)((char *)xup + xup->xu_len)) {
		const char *this_path = xup->xu_addr.sun_path;
		if (!this_path[0])
			continue;
		if (strcmp(path, this_path) == 0) {
			*sockptr = (kvaddr_t)xup->xu_socket.xso_so;
			*type = xup->xu_socket.so_type;
			break;
		}
	}

	free(v);
	return 0;
}

int neb_sock_unix_get_sockptr(const char *path, kvaddr_t *sockptr, int *type)
{
	*sockptr = 0;
	*type = 0;
	const char *local_mibs[] = {
		"net.local.stream.pcblist",
		"net.local.seqpacket.pcblist",
		"net.local.dgram.pcblist",
		NULL};
	for (int i = 0; local_mibs[i]; i++) {
		if (sysctl_local_pcblist_loop_get(local_mibs[i], path, sockptr, type) != 0) {
			neb_syslog(LOG_ERR, "Failed to query mib %s", local_mibs[i]);
			return -1;
		}
		if (*sockptr)
			break;
	}
	return 0;
}

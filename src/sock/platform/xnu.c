
#include <nebase/syslog.h>

#include "xnu.h"

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#define PRIVATE 1
#include <sys/unpcb.h>
#undef PRIVATE
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int sysctl_local_pcblist_loop_get(const char *mib, const char *path, so_type_t *sockptr, int *type)
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

	const struct xunpgen *oxug = (struct xunpgen *)v;
	const struct xunpgen *xug = oxug;

	for (xug = (struct xunpgen *)((char *)xug + xug->xug_len);
	     xug->xug_len > sizeof(struct xunpgen);
	     xug = (struct xunpgen *)((char *)xug + xug->xug_len)) {
#if !TARGET_OS_EMBEDDED
		const struct xunpcb64 *xup = (struct xunpcb64 *)xug;
		/* Ignore PCBs which were freed during copyout. */
		if (xup->xunp_gencnt > oxug->xug_gen)
			continue;
		const char *this_path = xup->xunp_addr.sun_path;
#else
		const struct xunpcb *xup = (struct xunpcb *)xug;
		/* Ignore PCBs which were freed during copyout. */
		if (xup->xu_unp.unp_gencnt > oxug->xug_gen)
			continue;
		const char *this_path = xup->xu_addr.sun_path;
#endif

		if (!this_path[0])
			continue;
		if (strcmp(path, this_path) == 0) {
			*sockptr = (so_type_t)xup->xu_socket.xso_so;
			*type = xup->xu_socket.so_type;
			break;
		}
	}

	free(v);
	return 0;
}

int neb_sock_unix_get_sockptr(const char *path, so_type_t *sockptr, int *type)
{
	*sockptr = 0;
	*type = 0;
#if !TARGET_OS_EMBEDDED
	const char *local_mibs[] = {
		"net.local.stream.pcblist64",
		"net.local.seqpacket.pcblist64",
		"net.local.dgram.pcblist64",
		NULL};
#else
	const char *local_mibs[] = {
		"net.local.stream.pcblist",
		"net.local.seqpacket.pcblist",
		"net.local.dgram.pcblist",
		NULL};
#endif
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

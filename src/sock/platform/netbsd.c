
#include <nebase/syslog.h>

#include "netbsd.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int sysctl_local_pcblist_loop_get(const char *mib, const char *path, uint64_t *sockptr, int *type)
{
	size_t sz = CTL_MAXNAME;
	int name[CTL_MAXNAME];

	if (sysctlnametomib(mib, name, &sz) == -1) {
		if (errno == ENOENT)
			return 0;
		neb_syslogl(LOG_ERR, "sysctlnametomib(%s): %m", mib);
		return -1;
	}
	u_int namelen = sz;

	name[namelen++] = PCB_ALL;
	name[namelen++] = 0;                        /* XXX all pids */
	name[namelen++] = sizeof(struct kinfo_pcb);
	name[namelen++] = INT_MAX;                  /* all of them */

	/* first, get the real buffer size we needed */
	sz = 0;
	if (sysctl(name, namelen, NULL, &sz, NULL, 0) == -1) {
		neb_syslog(LOG_ERR, "Failed to get buffer size for sysctl: %m");
		return -1;
	}
	void *v = malloc(sz);
	if (!v) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return -1;
	}

	/* NOTE according to sysctl(3), the previous returned buffer size will be large enough  */
	if (sysctl(name, namelen, v, &sz, NULL, 0) == -1) {
		neb_syslog(LOG_ERR, "Failed to get data(buflen: %zu) via sysctl: %m", sz);
		free(v);
		return -1;
	}

	int n = sz / sizeof(struct kinfo_pcb);
	for (int i = 0; i < n; i++) {
		const struct kinfo_pcb *kp = (struct kinfo_pcb *)v + i;
		const char *this_path = ((struct sockaddr_un *)&kp->ki_src)->sun_path;
		if (!this_path[0])
			continue;
		if (strcmp(path, this_path) == 0) {
			*sockptr = kp->ki_sockaddr;
			*type = kp->ki_type;
			break;
		}
	}

	free(v);
	return 0;
}

int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
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

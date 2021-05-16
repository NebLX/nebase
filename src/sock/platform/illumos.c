
#include <nebase/syslog.h>

#include "illumos.h"

#include <sys/socket.h>
#include <sys/socketvar.h>
#include <string.h>
#include <stdlib.h>

#include <kstat.h>

/**
 * \note the sockinfo used here is not reliable and may change in future
 */
int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
{
	kstat_ctl_t *kc = kstat_open();
	if (!kc) {
		neb_syslogl(LOG_ERR, "kstat_open: %m");
		return -1;
	}

	kstat_t *ksp = kstat_lookup(kc, "sockfs", 0, "sock_unix_list");
	if (!ksp) {
		neb_syslogl(LOG_ERR, "kstat_lookup: %m");
		kstat_close(kc);
		return -1;
	}

	if (kstat_read(kc, ksp, NULL) == -1) {
		neb_syslogl(LOG_ERR, "kstat_read: %m");
		kstat_close(kc);
		return -1;
	}
	if (!ksp->ks_ndata) {
		kstat_close(kc);
		return 0;
	}

	char *data = (char *)ksp->ks_data;
	for (uint i = 0; i < ksp->ks_ndata; i++) {
		struct sockinfo *psi = (struct sockinfo *)data;
		data += psi->si_size;

		if (psi->si_family != AF_UNIX)
			continue;
		const char *this_path = psi->si_laddr_sun_path;
		if (!this_path[0])
			continue;
		if (strcmp(path, this_path) == 0) {
			*sockptr = strtoull(psi->si_son_straddr, NULL, 16);
			*type = psi->si_type;
			break;
		}
	}

	kstat_close(kc);
	return 0;
}

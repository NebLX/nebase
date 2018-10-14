
#include <nebase/syslog.h>

#include "openbsd.h"

#include <kvm.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <limits.h>
#include <fcntl.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL
#include <string.h>

int neb_sock_unix_get_sockptr(const char *path, uint64_t *sockptr, int *type)
{
	*sockptr = 0;
	*type = 0;
	char errbuf[_POSIX2_LINE_MAX];
	kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, KVM_NO_FILES, errbuf);
	if (!kd) {
		neb_syslog(LOG_ERR, "kvm_openfiles: %s", errbuf);
		return -1;
	}

	int cnt;
	//NOTE process info will not be filled as we do query by file
	struct kinfo_file *ikf = kvm_getfiles(kd, KERN_FILE_BYFILE, DTYPE_SOCKET, sizeof(*ikf), &cnt);
	if (!ikf) {
		neb_syslog(LOG_ERR, "kvm_getfiles: %s", kvm_geterr(kd));
		kvm_close(kd);
		return -1;
	}

	for (int i = 0; i < cnt; i++) {
		const struct kinfo_file *kf = ikf + i;
		if (kf->so_family != AF_UNIX)
			continue;
		// the fsid and fileid is fake, so we have to use path to match
		const char *this_path = kf->unp_path;
		if (!this_path[0])
			continue;
		if (strcmp(path, this_path) == 0) {
			*sockptr = kf->v_un;
			*type = kf->so_type;
			break;
		}
	}

	kvm_close(kd);
	return 0;
}

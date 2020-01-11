
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/net/device.h>

#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>

#if defined(OSTYPE_SUN)
# include <sys/sockio.h>
#endif

bool neb_net_device_is_up(const char *ifname)
{
#if defined(OSTYPE_SUN)
	struct lifreq lir;
	strncpy(lir.lifr_name, ifname, IFNAMSIZ);
	lir.lifr_name[IFNAMSIZ - 1] = '\0';
#else
	struct ifreq ir;
	strncpy(ir.ifr_name, ifname, IFNAMSIZ);
	ir.ifr_name[IFNAMSIZ - 1] = '\0';
#endif

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1) {
		neb_syslogl(LOG_ERR, "socket: %m");
		return false;
	}

#if defined(OSTYPE_SUN)
	if (ioctl(fd, SIOCGLIFFLAGS, &lir) == -1) {
#else
	if (ioctl(fd, SIOCGIFFLAGS, &ir) == -1) {
#endif
		neb_syslogl(LOG_ERR, "ioctl: %m");
		close(fd);
		return false;
	}
	close(fd);

#if defined(OSTYPE_SUN)
	if (lir.lifr_flags & IFF_UP)
#else
	if (ir.ifr_flags & IFF_UP)
#endif
		return true;

	return false;
}

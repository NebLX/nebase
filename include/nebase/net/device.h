
#ifndef NEB_NET_DEVICE_H
#define NEB_NET_DEVICE_H 1

#include <nebase/cdefs.h>

#include <stdbool.h>

extern bool neb_net_device_is_up(const char *ifname)
	_nattr_nonnull((1));

#endif

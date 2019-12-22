
#ifndef NEB_PLUGIN_H
#define NEB_PLUGIN_H 1

#include "cdefs.h"

extern void *neb_plugin_open(const char *filename)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_plugin_close(void *handle)
	_nattr_nonnull((1));

extern int neb_plugin_get_symbol(void *handle, const char *symbol, void **addr)
	_nattr_nonnull((1, 2, 3));

#endif

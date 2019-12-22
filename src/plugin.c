
#include <nebase/syslog.h>
#include <nebase/plugin.h>

#include <dlfcn.h>

void *neb_plugin_open(const char *filename)
{
	void *h = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
	if (!h)
		neb_syslogl(LOG_ERR, "dlopen(%s): %m", filename);
	return h;
}

void neb_plugin_close(void *handle)
{
	int ret = dlclose(handle);
	if (ret != 0)
		neb_syslogl(LOG_ERR, "dlclose failed with ret %d", ret);
}

int neb_plugin_get_symbol(void *handle, const char *symbol, void **addr)
{
	dlerror(); // clear error first
	void *s = dlsym(handle, symbol);
	if (!s) {
		char *err = dlerror();
		if (err) {
			neb_syslogl(LOG_ERR, "dlsym(%s): %s", symbol, dlerror());
			return -1;
		}
	}
	*addr = s;
	return 0;
}

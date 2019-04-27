
#include <nebase/cdefs.h>

#include "sock.h"

static void neb_lib_init(void) __attribute__((constructor));

static void neb_lib_init()
{
	neb_sock_init();
}

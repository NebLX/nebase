
#include "version.h"

static const char neb_version[] = NEBASE_VERSION_STRING;

const char *neb_version_str(void)
{
	return neb_version;
}

int neb_version_code(void)
{
	return NEBASE_VERSON_CODE;
}

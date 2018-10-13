
#include <nebase/cdefs.h>
#include <nebase/syslog.h>

#include <stddef.h>
#include <errno.h>

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	neb_syslog_max_priority = LOG_DEBUG;
	neb_syslog_init(NEB_LOG_STDIO, NULL);
	neb_syslog(LOG_DEBUG, "Debug message OK");
	return 0;
}


#include <nebase/cdefs.h>
#include <nebase/syslog.h>

#include <stddef.h>
#include <errno.h>

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	neb_syslog_init(NEB_LOG_STDIO, NULL);
	errno = EINTR;
	neb_syslog(LOG_NOTICE, "Message for EINTR: %m");
	errno = EEXIST;
	neb_syslog(LOG_INFO, "Message for EEXIST: %m");
	return 0;
}

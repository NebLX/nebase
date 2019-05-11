
#include <nebase/syslog.h>

#include <stddef.h>
#include <errno.h>

int main(void)
{
	neb_syslog_init(NEB_LOG_STDIO, NULL);
	errno = EINTR;
	neb_syslog(LOG_NOTICE, "Message for EINTR: %%%m");
	return 0;
}

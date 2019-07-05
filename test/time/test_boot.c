
#include <nebase/time.h>

#include <stdio.h>

int main(void)
{
	time_t boot = neb_time_boot();
	if (!boot)
		return -1;
	fprintf(stdout, "boottime: %lld seconds since epoch time\n", (long long)boot);
	return 0;
}

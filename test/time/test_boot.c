
#include <nebase/cdefs.h>
#include <nebase/time.h>

#include <stdio.h>

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	time_t boot = neb_time_boot();
	if (!boot)
		return -1;
	fprintf(stdout, "boottime: %ld seconds since epoch time\n", boot);
	return 0;
}

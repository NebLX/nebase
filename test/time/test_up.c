
#include <nebase/time.h>

#include <stdio.h>

int main(void)
{
	time_t up = neb_time_up();
	if (!up)
		return -1;
	fprintf(stdout, "uptime: %ld seconds\n", up);
	return 0;
}

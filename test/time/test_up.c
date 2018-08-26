
#include <nebase/cdefs.h>
#include <nebase/time.h>

#include <stdio.h>

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	time_t up = neb_time_up();
	if (!up)
		return -1;
	fprintf(stdout, "uptime: %ld seconds\n", up);
	return 0;
}


#include <nebase/io.h>

#include <stdio.h>

int main(void)
{
	if (neb_io_redirect_stdout(-1) != 0) {
		fprintf(stderr, "Failed to redirect stdout to null\n");
		return -1;
	}

	fprintf(stdout, "message to null\n");
	return 0;
}


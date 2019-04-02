
#include <nebase/io.h>

#include <stdio.h>

const char msg[] = "message to null\n";

int main(void)
{
	if (neb_io_redirect_stdout(-1) != 0) {
		fprintf(stderr, "Failed to redirect stdout to null\n");
		return -1;
	}

	int np = fprintf(stdout, "%s", msg);
	if (np != (int)sizeof(msg) - 1) {
		fprintf(stderr, "Failed to send msg to stdout\n");
		return -1;
	} else {
		fprintf(stderr, "%d chars sent\n", np);
	}
	return 0;
}


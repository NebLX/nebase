
#include <nebase/sock.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>

const char *path = "/tmp/.nebase.test";

static int test_for(int type)
{
	int ret = 0;
	//no resource clean in this function, so we clean first
	unlink(path);

	int fd = neb_sock_unix_new_binded(type, path);
	if (fd == -1) {
		fprintf(stderr, "Failed to create socket bind to path %s\n", path);
		return -1;
	}

	int in_use = 0, otype = 0;
	switch (neb_sock_unix_path_in_use(path, &in_use, &otype)) {
	case 0:
		break;
	case 1:
		fprintf(stderr, "unix sock check is unsupported on this platform\n");
		return -1;
		break;
	default:
		fprintf(stderr, "failed to check unix sock\n");
		return -1;
		break;
	}
	if (!in_use) {
		fprintf(stderr, "Stage1: in_use is not set\n");
		ret = -1;
	}
	if (otype != type) {
		fprintf(stderr, "Stage1: type mismatch: real %d exp %d\n", otype, type);
		ret = -1;
	}

	close(fd);

	in_use = 0;
	if (neb_sock_unix_path_in_use(path, &in_use, &otype) != 0) {
		fprintf(stderr, "Stage2: check failed\n");
		ret = -1;
	} else {
		if (in_use) {
			fprintf(stderr, "Stage2: in_use is set\n");
			ret = -1;
		}
	}

	unlink(path);

	if (ret == 0)
		fprintf(stdout, "All check OK\n");
	return ret;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <stream|seqpacket|dgram>\n", argv[0]);
		return -1;
	}

	const char *type = argv[1];
	if (strcmp(type, "stream") == 0)
		return test_for(SOCK_STREAM);
	else if (strcmp(type, "seqpacket") == 0)
		return test_for(SOCK_SEQPACKET);
	else if (strcmp(type, "dgram") == 0)
		return test_for(SOCK_DGRAM);
	else
		fprintf(stderr, "Unsupported socket type\n");
	return -1;
}

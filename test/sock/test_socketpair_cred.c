
#include <nebase/sock.h>

#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

static int test_unix_sock_cred(int type)
{
	int fds[2];
	if (socketpair(AF_UNIX, type, 0, fds) == -1) {
		perror("socketpair");
		return -1;
	}

	int rfd = fds[0], wfd = fds[1];

	if (neb_sock_unix_enable_recv_cred(rfd) != 0) {
		fprintf(stderr, "Failed to enable recv of cred\n");
		return -1;
	}

# define BUFLEN 4
	char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
	char rbuf[BUFLEN] = NEB_STRUCT_INITIALIZER;

	int ret = neb_sock_unix_send_with_cred(wfd, wbuf, BUFLEN, NULL, 0);
	if (ret != BUFLEN) {
		fprintf(stderr, "Failed to send with cred\n");
		return -1;
	}

	struct neb_ucred u;
	ret = neb_sock_unix_recv_with_cred(rfd, rbuf, BUFLEN, &u);
	if (ret != BUFLEN) {
		fprintf(stderr, "Failed to recv with cred\n");
		return -1;
	}
	pid_t rpid = getpid();
	uid_t ruid = getuid();
	gid_t rgid = getgid();
	fprintf(stdout, "Running  pid: %d, uid: %d, gid: %d\n", rpid, ruid, rgid);
	fprintf(stdout, "Received pid: %d, uid: %d, gid: %d\n", u.pid, u.uid, u.gid);
	if (u.pid != rpid || u.uid != ruid || u.gid != rgid) {
		fprintf(stderr, "cred not match\n");
		return -1;
	}
	if (memcmp(wbuf, rbuf, BUFLEN) != 0) {
		fprintf(stderr, "wbuf != rbuf\n");
		return -1;
	}

	close(rfd);
	close(wfd);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <stream|seqpacket|dgram>\n", argv[0]);
		return -1;
	}

	const char *type = argv[1];
	if (strcmp(type, "stream") == 0)
		return test_unix_sock_cred(SOCK_STREAM);
	else if (strcmp(type, "seqpacket") == 0)
		return test_unix_sock_cred(SOCK_SEQPACKET);
	else if (strcmp(type, "dgram") == 0)
		return test_unix_sock_cred(SOCK_DGRAM);
	else
		fprintf(stderr, "Unsupported socket type\n");
	return -1;
}

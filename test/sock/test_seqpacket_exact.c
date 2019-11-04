
#include <nebase/sock.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

int main(void)
{
	int ret = 0;
	int fds[2];
	if (socketpair(AF_UNIX, SOCK_SEQPACKET, 0, fds) == -1) {
		perror("socketpair");
		return -1;
	}

	int rfd = fds[0], wfd = fds[1];
# define BUFLEN 4
	char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
	char rbuf[BUFLEN] = NEB_STRUCT_INITIALIZER;

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		return -1;
	} else if (cpid == 0) {
		close(rfd);
		rfd = -1;
		if (neb_sock_send_exact(wfd, wbuf, sizeof(wbuf)) != 0) {
			fprintf(stderr, "send failed\n");
			ret = -1;
		}
	} else {
		int wstatus = 0;
		close(wfd);
		wfd = -1;

		int hup = 0;
		if (!neb_sock_timed_read_ready(rfd, 1000, &hup)) {
			if (hup)
				fprintf(stderr, "read hup\n");
			else
				fprintf(stderr, "read timeout\n");
			ret = -1;
			goto exit_wait;
		}

		if (neb_sock_recv_exact(rfd, rbuf, sizeof(rbuf)) != 0) {
			fprintf(stderr, "read failed\n");
			ret = -1;
			goto exit_wait;
		}
		if (memcmp(wbuf, rbuf, sizeof(wbuf)) != 0) {
			fprintf(stderr, "data mismatch\n");
			ret = -1;
		}

exit_wait:
		waitpid(cpid, &wstatus, 0);
		if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "Child exit with error\n");
			return -1;
		}
		close(rfd);
	}

	if (rfd >= 0)
		close(rfd);
	if (wfd >= 0)
		close(wfd);
	return ret;
}

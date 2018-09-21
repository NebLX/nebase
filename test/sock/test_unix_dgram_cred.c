
#include <nebase/cdefs.h>
#include <nebase/sock.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

static struct sockaddr_un addr = {
	.sun_family = AF_UNIX,
	.sun_path = "/tmp/.nebase.test"
};

static int test_unix_sock_cred(void)
{
	int sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}
	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		close(sfd);
		return -1;
	}

# define BUFLEN 4
	char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
	char rbuf[BUFLEN] = {};

	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
		close(sfd);
		return -1;
	}
	if (cpid == 0) {
		close(sfd);
		int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
		if (fd == -1) {
			perror("socket");
			return -1;
		}

		int ret = neb_sock_unix_send_with_cred(fd, wbuf, BUFLEN, &addr, sizeof(addr));
		if (ret != BUFLEN) {
			fprintf(stderr, "Failed to send with cred\n");
			close(fd);
			return -1;
		}

		close(fd);
	} else {
		if (neb_sock_unix_enable_recv_cred(sfd) != 0) {
			fprintf(stderr, "Failed to enable recv of cred\n");
			close(sfd);
			return -1;
		}

		struct pollfd pfd = {
			.fd = sfd,
			.events = POLLIN
		};
		int ret = poll(&pfd, 1, 500);
		if (ret == -1) {
			perror("poll");
			close(sfd);
			return -1;
		} else if (ret == 0) {
			fprintf(stderr, "No connect request received\n");
			close(sfd);
			return -1;
		}

		struct neb_ucred u;
		ret = neb_sock_unix_recv_with_cred(sfd, rbuf, BUFLEN, &u);
		if (ret != BUFLEN) {
			fprintf(stderr, "Failed to recv with cred\n");
			close(sfd);
			return -1;
		}
		close(sfd);

		uid_t ruid = getuid();
		gid_t rgid = getgid();
		fprintf(stdout, "Children pid: %d, uid: %d, gid: %d\n", cpid, ruid, rgid);
		fprintf(stdout, "Received pid: %d, uid: %d, gid: %d\n", u.pid, u.uid, u.gid);
		if (u.pid != cpid || u.uid != ruid || u.gid != rgid) {
			fprintf(stderr, "cred not match\n");
			return -1;
		}
		if (memcmp(wbuf, rbuf, BUFLEN) != 0) {
			fprintf(stderr, "wbuf != rbuf\n");
			return -1;
		}

		int wstatus;
		waitpid(cpid, &wstatus, 0);
		if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "Child exit with error\n");
			return -1;
		}
	}

	return 0;
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret = test_unix_sock_cred();
	unlink(addr.sun_path);
	return ret;
}


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
	.sun_path = {'\0', 't', 'e', 's', 't', '\0'}
};

static int test_unix_sock_cred(int type)
{
	int sfd = socket(AF_UNIX, type, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}
	if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("bind");
		close(sfd);
		return -1;
	}
	if (listen(sfd, 1) == -1) {
		perror("listen");
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
		int fd = socket(AF_UNIX, type, 0);
		if (fd == -1) {
			perror("socket");
			return -1;
		}

		if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
			perror("connect");
			close(fd);
			return -1;
		}

		int ret = neb_sock_unix_send_with_cred(fd, wbuf, BUFLEN, NULL, 0);
		if (ret != BUFLEN) {
			fprintf(stderr, "Failed to send with cred\n");
			close(fd);
			return -1;
		}

		close(fd);
	} else {
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

		int fd = accept(sfd, NULL, NULL);
		if (fd == -1) {
			perror("accept");
			close(sfd);
			return -1;
		}
		close(sfd);

		if (neb_sock_unix_enable_recv_cred(fd) != 0) {
			fprintf(stderr, "Failed to enable recv of cred\n");
			close(fd);
			return -1;
		}

		struct neb_ucred u;
		ret = neb_sock_unix_recv_with_cred(fd, rbuf, BUFLEN, &u);
		if (ret != BUFLEN) {
			fprintf(stderr, "Failed to recv with cred\n");
			close(fd);
			return -1;
		}
		close(fd);

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

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <stream|seqpacket>\n", argv[0]);
		return -1;
	}

	const char *type = argv[1];
	if (strcmp(type, "stream") == 0)
		return test_unix_sock_cred(SOCK_STREAM);
	else if (strcmp(type, "seqpacket") == 0)
		return test_unix_sock_cred(SOCK_SEQPACKET);
	else
		fprintf(stderr, "Unsupported socket type\n");
	return -1;
}
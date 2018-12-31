
#include <nebase/sock.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <poll.h>

static int test_unix_sock_fd(int type)
{
	int ret = 0;
	int fds[2];
	if (socketpair(AF_UNIX, type, 0, fds) == -1) {
		perror("socketpair");
		return -1;
	}

	int rfd = fds[0], wfd = fds[1];
# define BUFLEN 4
	char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
	char rbuf[BUFLEN] = NEB_STRUCT_INITIALIZER;

	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
		return -1;
	} else if (cpid == 0) {
		close(rfd);
		int pipefd[2];
		if (pipe(pipefd) == -1) {
			perror("pipe");
			close(wfd);
			return -1;
		}

		int prfd = pipefd[0];
		int pwfd = pipefd[1];
		int nw = neb_sock_unix_send_with_fds(wfd, wbuf, sizeof(wbuf), &prfd, 1, NULL, 0);
		if (nw != (int)sizeof(wbuf)) {
			fprintf(stderr, "Failed to send data along with fd %d\n", prfd);
			close(prfd);
			close(pwfd);
			close(wfd);
			return -1;
		}
		close(prfd);

		nw = write(pwfd, wbuf, sizeof(wbuf));
		if (nw != (int)sizeof(wbuf)) {
			fprintf(stderr, "Failed to write data through pipefd\n");
			ret = -1;
		}

		close(pwfd);
		close(wfd);
	} else {
		int wstatus;
		close(wfd);

		struct pollfd pfd = {
			.fd = rfd,
			.events = POLLIN
		};
		switch (poll(&pfd, 1, 1000)) {
		case -1:
			perror("poll");
			ret =  -1;
			goto exit_wait;
			break;
		case 0:
			fprintf(stderr, "poll timeout\n");
			ret = -1;
			goto exit_wait;
			break;
		default:
			if (!(pfd.events & POLLIN)) {
				fprintf(stderr, "peer fd closed with no data to read\n");
				ret = -1;
				goto exit_wait;
			}
			break;
		}

		int prfd = -1;
		int fd_num = 1;
		int nr = neb_sock_unix_recv_with_fds(rfd, rbuf, sizeof(rbuf), &prfd, &fd_num);
		if (nr != (int)sizeof(rbuf)) {
			fprintf(stderr, "not all data read in through unix socket\n");
			ret = -1;
			goto exit_close_pipe;
		}
		if (memcmp(wbuf, rbuf, sizeof(wbuf)) != 0) {
			fprintf(stderr, "data not match through unix socket\n");
			ret = -1;
			goto exit_close_pipe;
		}
		if (fd_num != 1 || prfd < 0) {
			fprintf(stderr, "No fd received\n");
			ret = -1;
			goto exit_close_pipe;
		}

		pfd.fd = prfd;
		pfd.events = POLLIN;
		switch (poll(&pfd, 1, 1000)) {
		case -1:
			perror("poll pipefd");
			ret =  -1;
			goto exit_close_pipe;
			break;
		case 0:
			fprintf(stderr, "poll pipefd timeout\n");
			ret = -1;
			goto exit_close_pipe;
			break;
		default:
			if (!(pfd.events & POLLIN)) {
				fprintf(stderr, "peer pipefd closed with no data to read\n");
				ret = -1;
				goto exit_close_pipe;
			}
			break;
		}

		nr = read(prfd, rbuf, sizeof(rbuf));
		if (nr != (int)sizeof(rbuf)) {
			fprintf(stderr, "Failed to read data through pipe\n");
			ret = -1;
			goto exit_close_pipe;
		}
		if (memcmp(wbuf, rbuf, sizeof(wbuf)) != 0) {
			fprintf(stderr, "data not match through pipe\n");
			ret = -1;
		}

exit_close_pipe:
		if (prfd >= 0)
			close(prfd);
exit_wait:
		waitpid(cpid, &wstatus, 0);
		if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "Child exit with error\n");
			return -1;
		}
		close(rfd);
	}

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
		return test_unix_sock_fd(SOCK_STREAM);
	else if (strcmp(type, "seqpacket") == 0)
		return test_unix_sock_fd(SOCK_SEQPACKET);
	else if (strcmp(type, "dgram") == 0)
		return test_unix_sock_fd(SOCK_DGRAM);
	else
		fprintf(stderr, "Unsupported socket type\n");
	return -1;
}


#include <nebase/sem.h>
#include <nebase/sock.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>

// TODO use 2 sem

int main(void)
{
	int ret = 0;

	int sv[2];
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
		perror("socketpair");
		return -1;
	}

	char tmp_file[] = "/tmp/.nebase.test.sock-XXXXXX";
	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		perror("mkstemp");
		return -1;
	}
	close(fd);

	int semid = neb_sem_proc_create(tmp_file, 1);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem on file %s", tmp_file);
		unlink(tmp_file);
		return -1;
	}

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
		return -1;
	} else if (cpid == 0) {
		close(sv[0]);
		int fd = sv[1];

		if (neb_sock_wait_peer_closed(fd, 2)) {
			fprintf(stderr, "peer closed unexpectedly\n");
			ret = -1;
		}

		if (neb_sem_proc_post(semid, 0) != 0) {
			fprintf(stderr, "Failed to do sem post\n");
			return -1;
		}

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_zerod(semid, 0, &ts) != 0) {
			if (errno == ETIMEDOUT)
				fprintf(stderr, "operation timedout\n");
			fprintf(stderr, "Failed to wait sem to be zero\n");
			ret = -1;
		}

		if (ret == 0) {
			if (!neb_sock_wait_peer_closed(fd, 200)) {
				fprintf(stderr, "peer is not closed\n");
				ret = -1;
			}
		}

		fprintf(stdout, "child quit with code %d\n", ret);
	} else {
		close(sv[1]);
		int fd = sv[0];
		int wstatus;

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, 0, 1, &ts) != 0) {
			if (errno == ETIMEDOUT)
				fprintf(stderr, "operation timedout\n");
			fprintf(stderr, "Failed to wait sem to count by 1\n");
			ret = -1;
			goto exit_wait;
		}

		close(fd);

exit_wait:
		for (int i = 0; i < 500; i++) {
			int nc = waitpid(cpid, &wstatus, WNOHANG);
			if (nc == -1) {
				perror("waitpid");
				ret = -1;
				goto exit_unlink;
			}

			if (nc == 0) {
				usleep(10000);
				continue;
			}

			if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
				fprintf(stdout, "waited child exit code 0\n");
			} else {
				fprintf(stderr, "child exit with error\n");
				ret = -1;
			}

			break;
		}

exit_unlink:
		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
	}

	return ret;
}

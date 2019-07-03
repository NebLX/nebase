
#include <nebase/sem.h>
#include <nebase/sock.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <errno.h>

enum {
	SEMID_0 = 0,
	SEMID_1 = 1,
	SEMID_ALL = 2,
};

char buf[4] = NEB_STRUCT_INITIALIZER;

static int check_read_eof(int fd, int poll_revents _nattr_unused, void *udata)
{
	int *read_ok = udata;

	int nr = read(fd, buf, sizeof(buf));
	if (nr != (int)sizeof(buf)) {
		fprintf(stderr, "read %d of %lu\n", nr, sizeof(buf));
		*read_ok = 0;
		return 1;
	}
	*read_ok = 1;
	return 0;
}

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

	int semid = neb_sem_proc_create(tmp_file, SEMID_ALL);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem on file %s\n", tmp_file);
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

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, SEMID_1, 1, &ts) != 0) {
			fprintf(stderr, "Failed to wait sem1 to count by 1\n");
			ret = -1;
		}

		int read_ok = 0;
		if (neb_sock_check_peer_closed(fd, 2, check_read_eof, &read_ok)) {
			fprintf(stderr, "peer closed unexpectedly\n");
			ret = -1;
		}
		if (!read_ok)
			ret = -1;

		neb_sem_proc_post(semid, SEMID_0);

		if (neb_sem_proc_wait_count(semid, SEMID_1, 1, &ts) != 0) {
			fprintf(stderr, "Failed to wait sem1 to count by 1\n");
			ret = -1;
		}

		if (ret == 0) {
			if (!neb_sock_check_peer_closed(fd, 200, NULL, NULL)) {
				fprintf(stderr, "peer is not closed\n");
				ret = -1;
			}
		}

		fprintf(stdout, "child quit with code %d\n", ret);
	} else {
		close(sv[1]);
		int fd = sv[0];
		int wstatus;

		int nw = write(fd, buf, sizeof(buf));
		if (nw != (int)sizeof(buf)) {
			perror("write");
			ret = -1;
		}
		neb_sem_proc_post(semid, SEMID_1);

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, SEMID_0, 1, &ts) != 0) {
			if (errno == ETIMEDOUT)
				fprintf(stderr, "operation timedout\n");
			fprintf(stderr, "Failed to wait sem0 to count by 1\n");
			ret = -1;
			goto exit_wait;
		}

		close(fd);

		neb_sem_proc_post(semid, SEMID_1);

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

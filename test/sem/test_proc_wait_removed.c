
#include <nebase/sem.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>

#define SEM_SUBID_SYNC 0
#define SEM_SUBID_IDRM 1
#define SEM_SUBID_COUNT 2

int main(void)
{
	int ret = 0;

	char tmp_file[] = "/tmp/.nebase.test.sem-XXXXXX";
	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		perror("mkstemp");
		return -1;
	}
	close(fd);

	int semid = neb_sem_proc_create(tmp_file, SEM_SUBID_COUNT);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem on file %s\n", tmp_file);
		unlink(tmp_file);
		return -1;
	}
	fprintf(stdout, "semid: %d\n", semid);

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
		return -1;
	} else if (cpid == 0) {
		neb_sem_proc_post(semid, SEM_SUBID_SYNC);

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_removed(semid, SEM_SUBID_IDRM, &ts) != 0) {
			fprintf(stderr, "Failed to wait sem to be removed\n");
			ret = -1;
		} else {
			fprintf(stdout, "sem removed ok\n");
		}

		fprintf(stdout, "child quit with code %d\n", ret);
	} else {
		int wstatus;

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, SEM_SUBID_SYNC, 1, &ts) != 0) {
			fprintf(stderr, "Failed to wait child to be ready\n");
			ret = -1;
		}

		neb_sem_proc_destroy(semid);
		unlink(tmp_file);

		for (int i = 0; i < 500; i++) {
			int nc = waitpid(cpid, &wstatus, WNOHANG);
			if (nc == -1) {
				perror("waitpid");
				ret = -1;
				goto exit_return;
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
	}

exit_return:
	return ret;
}

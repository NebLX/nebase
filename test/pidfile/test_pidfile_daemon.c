
#include <nebase/cdefs.h>
#include <nebase/events.h>
#include <nebase/sem.h>
#include <nebase/proc.h>
#include <nebase/pidfile.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

const char pidfile[] = "/tmp/.nebase.test.pid";
static char tmp_file[] = "/tmp/.nebase.test.pdfileXXXXXX";
static int semid = -1;
static pid_t daemon_pid = 0;

static int test_round1(void)
{
	pid_t locker = 0;
	int fd = neb_pidfile_open(pidfile, &locker);
	if (fd < 0) {
		fprintf(stderr, "Failed to open pidfile, locker: %d\n", locker);
		return -1;
	}
	fprintf(stdout, "open ok\n");

	daemon_pid = 0;
	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_pidfile_close(fd);
		return -1;
	} else if (cpid == 0) {
		locker = neb_pidfile_write(fd);
		if (locker < 0) {
			fprintf(stderr, "failed to write pidfile\n");
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else if (locker > 0) {
			fprintf(stderr, "pidfile is locked by %d\n", locker);
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else {
			fprintf(stdout, "write ok\n");
			// leave an empty pidfile
			if (ftruncate(fd, 0) == -1) {
				perror("ftruncate");
				neb_pidfile_close(fd);
				neb_proc_child_exit(-1);
			}
			neb_pidfile_close(fd);
			neb_proc_child_exit(0);
		}
	} else {
		neb_pidfile_close(fd);
		daemon_pid = cpid;
		fprintf(stdout, "daemon running as pid %d\n", daemon_pid);
		int wstatus;
		if (waitpid(daemon_pid, &wstatus, 0) == -1) {
			perror("waitpid");
			return -1;
		}

		if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
			return 0;
		} else {
			fprintf(stderr, "daemon exit with error\n");
			return -1;
		}
	}
}

static int test_round2(void)
{
	pid_t locker = 0;
	int fd = neb_pidfile_open(pidfile, &locker);
	if (fd < 0) {
		fprintf(stderr, "Failed to open pidfile, locker: %d\n", locker);
		return -1;
	}
	fprintf(stdout, "open ok\n");

	daemon_pid = 0;
	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_pidfile_close(fd);
		return -1;
	} else if (cpid == 0) {
		locker = neb_pidfile_write(fd);
		if (locker < 0) {
			fprintf(stderr, "failed to write pidfile\n");
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else if (locker > 0) {
			fprintf(stderr, "pidfile is locked by %d\n", locker);
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else {
			fprintf(stdout, "write ok\n");
			// leave an obsolete pidfile
			neb_pidfile_close(fd);
			neb_proc_child_exit(0);
		}
	} else {
		neb_pidfile_close(fd);
		daemon_pid = cpid;
		fprintf(stdout, "daemon running as pid %d\n", daemon_pid);
		int wstatus;
		if (waitpid(daemon_pid, &wstatus, 0) == -1) {
			perror("waitpid");
			return -1;
		}

		if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
			return 0;
		} else {
			fprintf(stderr, "daemon exit with error\n");
			return -1;
		}
	}
}

static int test_round3(void)
{
	pid_t locker = 0;
	int fd = neb_pidfile_open(pidfile, &locker);
	if (fd < 0) {
		fprintf(stderr, "Failed to open pidfile, locker: %d\n", locker);
		return -1;
	}
	fprintf(stdout, "open ok\n");

	daemon_pid = 0;
	pid_t cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_pidfile_close(fd);
		return -1;
	} else if (cpid == 0) {
		locker = neb_pidfile_write(fd);
		if (locker < 0) {
			fprintf(stderr, "failed to write pidfile\n");
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else if (locker > 0) {
			fprintf(stderr, "pidfile is locked by %d\n", locker);
			neb_pidfile_close(fd);
			neb_proc_child_exit(-1);
		} else {
			fprintf(stdout, "write ok\n");
			sigset_t m;
			sigemptyset(&m);
			sigaddset(&m, SIGTERM);
			if (sigprocmask(SIG_BLOCK, &m, NULL) == -1) {
				perror("sigprocmask");
				neb_pidfile_close(fd);
				neb_proc_child_exit(-1);
			}
			neb_sem_proc_post(semid, 0);
			int term_received = 0;
			// TODO use sigtimedwait after all platforms (OpenBSD, Darwin) support it
			for (int i = 0; i < 400; i++) {
				sigemptyset(&m);
				if (sigpending(&m) == -1) {
					perror("sigpending");
					neb_pidfile_close(fd);
					neb_proc_child_exit(-1);
				}
				if (sigismember(&m, SIGTERM)) {
					term_received = 1;
					break;
				}
				usleep(10000);
			}
			if (term_received) {
				neb_pidfile_close(fd);
				neb_proc_child_exit(0);
			} else {
				fprintf(stderr, "No term signal received\n");
				neb_pidfile_close(fd);
				neb_proc_child_exit(-1);
			}
		}
	} else {
		neb_pidfile_close(fd);
		daemon_pid = cpid;
		fprintf(stdout, "daemon running as pid %d\n", daemon_pid);

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0};
		if (neb_sem_proc_wait_count(semid, 0, 1, &ts) != 0) {
			fprintf(stderr, "Failed to wait sem count by 1\n");
			return -1;
		}
		return 0;
	}
}

static int test_round4(void)
{
	pid_t locker = 0;
	int fd = neb_pidfile_open(pidfile, &locker);
	if (fd < 0) {
		if (!locker) {
			fprintf(stderr, "Failed to open pidfile\n");
			return -1;
		} else if (locker != daemon_pid) {
			fprintf(stderr, "returned locker is %d while we expect %d\n", locker, daemon_pid);
			return -1;
		} else {
			fprintf(stdout, "block test ok\n");
			return 0;
		}
	} else {
		fprintf(stderr, "pidfile open should be blocked by locker %d\n", daemon_pid);
		return -1;
	}
}

static void close_sem(void)
{
	fprintf(stderr, "Closing sem\n");
	neb_sem_proc_destroy(semid);
	unlink(tmp_file);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret = 0;
	unlink(pidfile);
	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		perror("mkstemp");
		return -1;
	}
	close(fd);

	semid = neb_sem_proc_create(tmp_file, 1);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem on file %s\n", tmp_file);
		unlink(tmp_file);
		return -1;
	}

	fprintf(stdout, "Round 1: no old pidfile\n");
	if (test_round1() != 0) {
		fprintf(stderr, "Round 1 failed\n");
		ret = -1;
		goto exit_unlink;
	}

	fprintf(stdout, "Round 2: daemon exist and leave empty pidfile\n");
	if (test_round2() != 0) {
		fprintf(stderr, "Round 2 failed\n");
		ret = -1;
		goto exit_unlink;
	}

	fprintf(stdout, "Round 3: daemon exist and leave obsolete pidfile\n");
	if (test_round3() != 0) {
		fprintf(stderr, "Round 3 failed\n");
		ret = -1;
		goto exit_unlink;
	}

	fprintf(stdout, "Round 4: daemon is still running\n");
	if (test_round4() != 0) {
		fprintf(stderr, "Round 4 failed\n");
		ret = -1;
	}

	fprintf(stdout, "Killing child %d\n", daemon_pid);
	if (kill(daemon_pid, SIGTERM) == -1) {
		perror("kill");
		ret = -1;
		goto exit_unlink;
	}
	int wstatus;
	if (waitpid(daemon_pid, &wstatus, 0) == -1) {
		perror("waitpid");
		ret = -1;
	} else {
		if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
			;
		} else {
			fprintf(stderr, "daemon exit with error\n");
			ret = -1;
		}
	}

exit_unlink:
	unlink(pidfile);
	close_sem();
	return ret;
}

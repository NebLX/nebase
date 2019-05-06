
#include <nebase/cdefs.h>
#include <nebase/io.h>
#include <nebase/sem.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

static char tmp_file[] = "/tmp/.nebase.test.ioXXXXXX";
static int semid = -1;

static void close_sem(void)
{
	fprintf(stderr, "Closing sem\n");
	neb_sem_proc_destroy(semid);
	unlink(tmp_file);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret = 0;

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

	int fdm = neb_io_pty_open_master();
	if (fdm < 0) {
		fprintf(stderr, "Failed to open terminal master side\n");
		return -1;
	}

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		return -1;
	} else if (cpid == 0) {
		if (setsid() == -1) {
			perror("setsid");
			exit(-1);
		}

		int fds = neb_io_pty_open_slave(fdm);
		if (fds < 0) {
			fprintf(stderr, "Failed to open terminal slave side\n");
			exit(-1);
		}

		close(fdm);

		if (neb_io_pty_associate(fds) != 0) {
			fprintf(stderr, "Failed to attach terminal as controlling terminal\n");
			close(fds);
			exit(-1);
		}

		if (neb_io_redirect_stdin(fds) != 0) {
			fprintf(stderr, "Failed to redirect stdin to terminal fd %d\n", fds);
			close(fds);
			exit(-1);
		}
		close(fds);

		sigset_t m;
		sigemptyset(&m);
		sigaddset(&m, SIGINT);
		if (sigprocmask(SIG_BLOCK, &m, NULL) == -1) {
			perror("sigprocmask");
			exit(-1);
		}
		neb_sem_proc_post(semid, 0);
		int sig_received = 0;
		// TODO use sigtimedwait after all platforms (OpenBSD, Darwin) support it
		for (int i = 0; i < 400; i++) {
			sigemptyset(&m);
			if (sigpending(&m) == -1) {
				perror("sigpending");
				exit(-1);
			}
			if (sigismember(&m, SIGINT)) {
				sig_received = 1;
				break;
			}
			usleep(10000);
		}
		if (sig_received) {
			fprintf(stdout, "received sigint\n");
			exit(0);
		} else {
			fprintf(stderr, "No sigint received\n");
			exit(-1);
		}
	} else {
		int wstatus;

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, 0, 1, &ts) != 0) {
			if (errno == ETIMEDOUT)
				fprintf(stderr, "operation timedout\n");
			fprintf(stderr, "Failed to wait sem to count by 1\n");
			ret = -1;
			goto exit_wait;
		}

		char ctrl_c = '\03';
		ssize_t nw = write(fdm, &ctrl_c, 1);
		if (nw < 1) {
			perror("write");
			ret = -1;
			goto exit_unlink;
		}

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
	}

exit_unlink:
	close_sem();
	return ret;
}

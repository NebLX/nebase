
#include <nebase/cdefs.h>
#include <nebase/io.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

const char semname[] = "/nebase.t.sem"; // NetBSD requires to be less than 14 bytes
static sem_t *sync_sem = SEM_FAILED;

static void close_sem(void)
{
	if (sync_sem != SEM_FAILED)
		sem_close(sync_sem);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret = 0;

	sem_unlink(semname);
	sync_sem = sem_open(semname, O_CREAT | O_EXCL, 0600, 0);
	if (sync_sem == SEM_FAILED) {
		perror("sem_open");
		return -1;
	}
	atexit(close_sem);

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
		if (sem_post(sync_sem) == -1) {
			perror("sem_post");
			exit(-1);
		}
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
		// TODO use sem_timedwait after all platforms (Darwin) support it
		for (int i = 0; i < 400; i++) {
			if (sem_trywait(sync_sem) == -1) {
				if (errno == EAGAIN) {
					usleep(10000);
					continue;
				}
				perror("sem_trywait");
				ret = -1;
				goto exit_unlink;
			}

			char ctrl_c = '\03';
			ssize_t nw = write(fdm, &ctrl_c, 1);
			if (nw < 1) {
				perror("write");
				ret = -1;
				goto exit_unlink;
			}

			int wstatus;
			for (int i = 0; i < 400; i++) {
				int nc = waitpid(cpid, &wstatus, 0);
				if (nc == -1) {
					perror("waitpid");
					return -1;
				}

				if (nc == 0) {
					usleep(10000);
					continue;
				}

				if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
					ret = 0;
				} else {
					fprintf(stderr, "child exit with error\n");
					ret = -1;
				}

				goto exit_unlink;
			}

			fprintf(stderr, "wait child quit timeout\n");
			ret = -1;
			goto exit_unlink;
		}

		fprintf(stderr, "wait sync_sem timeout\n");
		ret = -1;
	}

exit_unlink:
	sem_unlink(semname);
	return ret;
}

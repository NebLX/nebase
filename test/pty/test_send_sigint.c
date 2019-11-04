
#include <nebase/pty.h>
#include <nebase/sem.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include <sys/wait.h>

static int semid = -1;

static void close_sem(void)
{
	fprintf(stderr, "Closing sem\n");
	neb_sem_proc_destroy(semid);
}

int main(void)
{
	int ret = 0;

	semid = neb_sem_proc_create(NULL, 1);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem\n");
		return -1;
	}

	int pty_master = -1, pty_slave = -1;
	if (neb_pty_openpty(&pty_master, &pty_slave) == -1) {
		fprintf(stderr, "failed to openpty\n");
		close_sem();
		return -1;
	}

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		close(pty_master);
		close(pty_slave);
		close_sem();
		return -1;
	} else if (cpid == 0) {
		close(pty_master);

		if (neb_pty_login_tty(pty_slave) == -1) {
			fprintf(stderr, "child: failed to set login tty\n");
			exit(-1);
		}

		sigset_t m;
		sigemptyset(&m);
		sigaddset(&m, SIGINT);
		if (sigprocmask(SIG_BLOCK, &m, NULL) == -1) {
			perror("sigprocmask");
			exit(-1);
		}

		neb_sem_proc_post(semid, 0);

		fprintf(stdout, "waiting sigint\n");
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
			fprintf(stdout, "received sigint, child quit\n");
			exit(0);
		} else {
			fprintf(stderr, "No sigint received, child quit\n");
			exit(-1);
		}
	} else {
		close(pty_slave);
		atexit(close_sem);

		struct timespec ts = {.tv_sec = 4, .tv_nsec = 0}; // 4s
		if (neb_sem_proc_wait_count(semid, 0, 1, &ts) != 0) {
			if (errno == ETIMEDOUT)
				fprintf(stderr, "operation timedout\n");
			fprintf(stderr, "Failed to wait sem to count by 1\n");
			exit(-1);
		}

		char ctrl_c = '\03';
		ssize_t nw = write(pty_master, &ctrl_c, 1);
		if (nw < 1) {
			perror("write");
			exit(-1);
		}

		struct pollfd pfd = {.fd = pty_master, .events = POLLIN};
		for (;;) {
			int n = poll(&pfd, 1, 4000);
			if (n == -1) {
				perror("poll");
				exit(-1);
			} else if (n == 0) {
				fprintf(stderr, "No data received from child\n");
				exit(-1);
			}

			if (pfd.revents & POLLIN) {
				char buf[1024];
				// NOTE sizeof(buf) is enough for this testcase,
				// but may be not for all condition
				ssize_t nr = read(pty_master, buf, sizeof(buf)-1);
				if (nr == -1) {
					perror("read");
					exit(-1);
				} else if (nr == 0) {
					break;
				} else {
					buf[nr] = '\0';
					fprintf(stdout, "CHILD: %s\n", buf);
				}
			}
			if (pfd.revents & POLLHUP)
				break;
		}
		close(pty_master);

		int wstatus = 0;
		int nc = waitpid(cpid, &wstatus, WNOHANG);
		if (nc == -1) {
			perror("waitpid");
			exit(-1);
		}

		if (WIFSIGNALED(wstatus)) {
			fprintf(stderr, "child is killed by signal %d\n", WTERMSIG(wstatus));
			ret = -1;
		} else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) == 0) {
			fprintf(stdout, "waited child exit code 0\n");
			ret = 0;
		} else {
			fprintf(stderr, "child exit with error code %d\n", WEXITSTATUS(wstatus));
			ret = -1;
		}
	}

	return ret;
}

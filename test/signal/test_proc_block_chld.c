
#include <nebase/signal.h>

#include <stdio.h>
#include <signal.h>

int main(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGQUIT);
	sigprocmask(SIG_SETMASK, &set, NULL);

	if (neb_signal_proc_block_chld() != 0) {
		fprintf(stderr, "failed to block chld signal\n");
		return -1;
	}

	sigemptyset(&set);
	sigprocmask(0, NULL, &set);
	if (!sigismember(&set, SIGCHLD)) {
		fprintf(stderr, "SIGCHLD is not really blocked\n");
		return -1;
	}
	if (!sigismember(&set, SIGQUIT)) {
		fprintf(stderr, "Old blocked SIGQUIT get unblocked\n");
		return -1;
	}

	if (neb_signal_proc_unblock_chld() != 0) {
		fprintf(stderr, "failed to block chld signal\n");
		return -1;
	}

	sigemptyset(&set);
	sigprocmask(0, NULL, &set);
	if (sigismember(&set, SIGCHLD)) {
		fprintf(stderr, "SIGCHLD is not really unblocked\n");
		return -1;
	}
	if (!sigismember(&set, SIGQUIT)) {
		fprintf(stderr, "Old blocked SIGQUIT get unblocked\n");
		return -1;
	}

	return 0;
}

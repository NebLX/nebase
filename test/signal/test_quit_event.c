
#include <nebase/cdefs.h>
#include <nebase/signal.h>
#include <nebase/events.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	struct sigaction sa = {
		.sa_handler = neb_sigterm_handler,
	};
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("sigaction");
		return -1;
	}

	int ret = 0;
	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		ret = -1;
	} else if (cpid == 0) {
		ret = kill(getppid(), SIGTERM);
		if (ret == -1)
			perror("kill");
	} else {
		for (int i = 0; i < 1000; i++) {
			if (thread_events & T_E_QUIT)
				break;
			usleep(1000);
		}

		if (!(thread_events & T_E_QUIT)) {
			fprintf(stderr, "No quit event set after timeout\n");
			ret = -1;
		} else {
			fprintf(stdout, "quit event set\n");
		}

		int wstatus;
		if (waitpid(cpid, &wstatus, 0) == -1) {
			perror("waitpid");
			ret = -1;
		} else if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "child exit with error\n");
			ret = -1;
		}
	}

	return ret;
}


#include <nebase/signal.h>
#include <nebase/events.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>

int main(void)
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
	if (raise(SIGTERM) != 0) {
		perror("raise");
		return -1;
	}

	if (!(thread_events & T_E_QUIT)) {
		fprintf(stderr, "No quit event set after timeout\n");
		ret = -1;
	} else {
		fprintf(stdout, "quit event set\n");
	}

	return ret;
}

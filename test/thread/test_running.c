
#include <nebase/cdefs.h>
#include <nebase/thread.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

static volatile int run_error = 0;

static void *chld_exec(void *arg __attribute_unused__)
{
	if (neb_thread_register() != 0) {
		fprintf(stderr, "Register failed\n");
		run_error = 1;
		pthread_exit("failed");
	}

	fprintf(stdout, "child begin sleep\n");
	sleep(5);
	fprintf(stderr, "child is not canceled after timeout\n");
	run_error = 1;
	pthread_exit(NULL);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret, exit_code = 0;

	if (neb_thread_init() != 0) {
		fprintf(stderr, "thread init failed\n");
		return -1;
	}

	pthread_t chld;
	ret = pthread_create(&chld, NULL, chld_exec, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(ret));
		neb_thread_deinit();
		return -1;
	}

	void *retval = NULL;
	int running = 0;
	for (int i = 0; i < 4000; i++) {
		if (neb_thread_is_running(chld)) {
			running = 1;
			break;
		}
		usleep(1000);
	}
	if (!running) {
		fprintf(stderr, "wait running timeout\n");
		exit_code = -1;
		goto exit_join;
	} else {
		fprintf(stdout, "ok: child is running\n");
	}

	pthread_cancel(chld);

	for (int i = 0; i < 400; i++) {
		if (!neb_thread_is_running(chld)) {
			running = 0;
			break;
		}
		usleep(10000);
	}
	if (running) {
		fprintf(stderr, "wait to be not running timeout\n");
		exit_code = -1;
		goto exit_join;
	} else {
		fprintf(stdout, "ok: child is not running\n");
	}

exit_join:
	ret = pthread_join(chld, &retval);
	if (ret != 0) {
		fprintf(stderr, "pthread_join: %s\n", strerror(ret));
		exit_code = -1;
	} else if (retval != PTHREAD_CANCELED) {
		fprintf(stderr, "the child thread is not canceled\n");
		exit_code = -1;
	}

	if (run_error)
		exit_code = -1;

	neb_thread_deinit();
	return exit_code;
}

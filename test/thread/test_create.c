
#include <nebase/cdefs.h>
#include <nebase/thread.h>

#include <stdio.h>
#include <pthread.h>

static const char failed[] = "failed";

static void *chld_exec(void *arg _nattr_unused)
{
	if (neb_thread_set_ready() != 0) {
		fprintf(stderr, "Failed to set ready\n");
		pthread_exit((void *)failed);
	}
	pthread_exit(NULL);
}

int main(void)
{
	if (neb_thread_init() != 0) {
		fprintf(stderr, "thread init failed\n");
		return -1;
	}

	pthread_t chld;
	if (neb_thread_create(&chld, NULL, chld_exec, NULL) != 0) {
		fprintf(stderr, "failed to create child thread\n");
		return -1;
	}

	int ret = 0;
	void *retval = NULL;
	ret = pthread_join(chld, &retval);
	if (ret != 0) {
		fprintf(stderr, "join child failed\n");
		ret = -1;
	} else if (retval != NULL) {
		fprintf(stderr, "child thread exit with error\n");
		ret = -1;
	}

	neb_thread_deinit();
	return ret;
}

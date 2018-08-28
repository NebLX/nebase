
#include <nebase/cdefs.h>
#include <nebase/thread.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

static void *chld_exec(void *arg)
{
	pid_t *chld_tid = arg;
	*chld_tid = neb_thread_getid();
	pthread_exit(NULL);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	pid_t pid = getpid();
	pid_t main_tid = neb_thread_getid();
	pid_t chld_tid = 0;

	pthread_t chld;
	int ret = pthread_create(&chld, NULL, chld_exec, &chld_tid);
	if (ret != 0) {
		fprintf(stderr, "pthread_create: %s\n", strerror(ret));
		return -1;
	}
	ret = pthread_join(chld, NULL);
	if (ret != 0) {
		fprintf(stderr, "pthread_join: %s\n", strerror(ret));
		return -1;
	}

	fprintf(stdout, "PID: %d\nMain TID: %d\nCHLD TID: %d\n", pid, main_tid, chld_tid);
	if (!main_tid || !chld_tid || main_tid == chld_tid)
		return -1;

	return 0;
}

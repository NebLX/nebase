
#include <nebase/cdefs.h>
#include <nebase/dispatch.h>
#include <nebase/events.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

# define BUFLEN 4
char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
char rbuf[BUFLEN] = {};

int hup_ok = 0, read_ok = 0, timeout = 0;

static dispatch_cb_ret_t timeout_handler(unsigned int ident __attribute_unused__, void *udata __attribute_unused__)
{
	timeout = 1;
	fprintf(stdout, "timeout occured\n");
	return DISPATCH_CB_BREAK;
}

static dispatch_cb_ret_t hup_handler(int fd, void *udata __attribute_unused__)
{
	fprintf(stdout, "peer of fd %d closed\n", fd);
	thread_events |= T_E_QUIT;
	hup_ok = 1;
	return DISPATCH_CB_REMOVE;
}

static dispatch_cb_ret_t read_handler(int fd, void *udata __attribute_unused__)
{
	ssize_t nr = read(fd, rbuf, sizeof(rbuf));
	if (nr == -1) {
		perror("read");
		return DISPATCH_CB_BREAK;
	}
	if (nr == 0)
		return DISPATCH_CB_CONTINUE;
	if (nr != BUFLEN) { // we will recv all or none, as it's atomic write
		fprintf(stderr, "not all data read\n");
		return DISPATCH_CB_BREAK;
	}
	fprintf(stdout, "read in %lld bytes from fd %d\n", (long long int)nr, fd);
	read_ok = 1;
	return DISPATCH_CB_CONTINUE;
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int pipefd[2];
	if (pipe2(pipefd, O_NONBLOCK) == -1) {
		perror("pipe2");
		return -1;
	}

	int ret = 0;
	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		return -1;
	} else if (cpid == 0) {
		close(pipefd[0]);
		int fd = pipefd[1];
		if (write(fd, wbuf, sizeof(wbuf)) == -1) {
			perror("write");
			close(fd);
			return -1;
		}
		close(fd);
	} else {
		close(pipefd[1]);
		int fd = pipefd[0];
		dispatch_queue_t dq = NULL;
		dispatch_source_t ds = NULL;
		dispatch_source_t dst = NULL;

		dq = neb_dispatch_queue_create(NULL, NULL);
		if (!dq) {
			fprintf(stderr, "failed to create dispatch queue\n");
			return -1;
		}

		ds = neb_dispatch_source_new_fd_read(fd, read_handler, hup_handler);
		if (!ds) {
			fprintf(stderr, "failed to create fd source\n");
			ret = -1;
			goto exit_clean;
		}
		if (neb_dispatch_queue_add(dq, ds) != 0) {
			fprintf(stderr, "failed to add fd source to queue\n");
			ret = -1;
			goto exit_clean;
		}

		dst = neb_dispatch_source_new_itimer_msec(1, 500, timeout_handler);
		if (!dst) {
			fprintf(stderr, "failed to create timer source");
			ret = -1;
			goto exit_clean;
		}
		if (neb_dispatch_queue_add(dq, dst) != 0) {
			fprintf(stderr, "failed to add timer source to queue\n");
			ret = -1;
			goto exit_clean;
		}

		if (neb_dispatch_queue_run(dq, NULL, NULL) != 0) {
			fprintf(stderr, "failed to run queue\n");
			ret = -1;
		}

exit_clean:
		if (dst) {
			if (neb_dispatch_queue_rm(dq, dst) !=0)
				fprintf(stderr, "failed to remove timer source from queue\n");
			neb_dispatch_source_del(dst);
		}
		if (ds) {
			if (neb_dispatch_queue_rm(dq, ds) !=0)
				fprintf(stderr, "failed to remove fd source from queue\n");
			neb_dispatch_source_del(ds);
		}
		neb_dispatch_queue_destroy(dq);

		int wstatus;
		waitpid(cpid, &wstatus, 0);
		if (!WIFEXITED(wstatus) || WEXITSTATUS(wstatus) != 0) {
			fprintf(stderr, "Child exit with error\n");
			ret = -1;
		}

		if (timeout) {
			ret = -1;
		} else if (!read_ok || !hup_ok) {
			fprintf(stderr, "read_ok: %d, hup_ok: %d\n", read_ok, hup_ok);
			ret = -1;
		} else if (memcmp(wbuf, rbuf, BUFLEN) != 0) {
			fprintf(stderr, "wbuf != rbuf\n");
			ret = -1;
		}
	}

	return ret;
}


#include <nebase/cdefs.h>
#include <nebase/evdp.h>
#include <nebase/pipe.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

# define BUFLEN 4
char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
char rbuf[BUFLEN] = NEB_STRUCT_INITIALIZER;

int hup_ok = 0, read_ok = 0, timeout = 0;

static neb_evdp_cb_ret_t wakeup_handler(unsigned int ident _nattr_unused, long overrun _nattr_unused, void *udata _nattr_unused)
{
	timeout = 1;
	fprintf(stdout, "timeout occured\n");
	return NEB_EVDP_CB_BREAK_EXP;
}

static neb_evdp_cb_ret_t hup_handler(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	fprintf(stdout, "peer of fd %d closed\n", fd);
	hup_ok = 1;
	return NEB_EVDP_CB_BREAK_EXP;
}

static neb_evdp_cb_ret_t read_handler(int fd, void *udata _nattr_unused, const void *context)
{
	int nbytes = 0;
	if (neb_evdp_source_fd_get_nread(context, &nbytes) != 0) {
		fprintf(stderr, "failed to get nread\n");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	fprintf(stdout, "nread: %d\n", nbytes);
	if (nbytes != sizeof(rbuf)) {
		fprintf(stderr, "nread is %d, we expect %lu\n", nbytes, sizeof(rbuf));
		return NEB_EVDP_CB_BREAK_ERR;
	}

	ssize_t nr = read(fd, rbuf, sizeof(rbuf));
	if (nr == -1) {
		perror("read");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	if (nr == 0) {
		if (read_ok)
			hup_ok = 1;
		return NEB_EVDP_CB_BREAK_EXP;
	}
	if (nr != BUFLEN) { // we will recv all or none, as it's atomic write
		fprintf(stderr, "not all data read\n");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	fprintf(stdout, "read in %lld bytes from fd %d\n", (long long int)nr, fd);
	read_ok = 1;
	return NEB_EVDP_CB_CONTINUE;
}

int main(void)
{
	int pipefd[2];
	if (neb_pipe_new(pipefd) == -1) {
		perror("Failed to create pipe");
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
		neb_evdp_queue_t dq = NULL;
		neb_evdp_source_t ds = NULL;
		neb_evdp_source_t dst = NULL;

		dq = neb_evdp_queue_create(0);
		if (!dq) {
			fprintf(stderr, "failed to create evdp queue\n");
			return -1;
		}

		ds = neb_evdp_source_new_ro_fd(fd, read_handler, hup_handler);
		if (!ds) {
			fprintf(stderr, "failed to create ro_fd evdp source\n");
			ret = -1;
			goto exit_clean;
		}
		if (neb_evdp_queue_attach(dq, ds) != 0) {
			fprintf(stderr, "failed to attach ro_fd source to queue\n");
			ret = -1;
			goto exit_clean;
		}

		dst = neb_evdp_source_new_itimer_ms(1, 500, wakeup_handler);
		if (!dst) {
			fprintf(stderr, "failed to create itimer_ms evdp source");
			ret = -1;
			goto exit_clean;
		}
		if (neb_evdp_queue_attach(dq, dst) != 0) {
			fprintf(stderr, "failed to add itimer_ms source to queue\n");
			ret = -1;
			goto exit_clean;
		}

		if (neb_evdp_queue_run(dq) != 0) {
			fprintf(stderr, "failed to run evdp queue\n");
			ret = -1;
		}

exit_clean:
		if (dst) {
			if (neb_evdp_queue_detach(dq, dst, 0) != 0)
				fprintf(stderr, "failed to detach dst\n");
			neb_evdp_source_del(dst);
		}
		if (ds) {
			if (neb_evdp_queue_detach(dq, ds, 1) != 0)
				fprintf(stderr, "failed to detach ds\n");
			close(fd);
			neb_evdp_source_del(ds);
		}
		neb_evdp_queue_destroy(dq);

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

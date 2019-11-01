
/*
 * Both wr and rd handlers are set at benging, wr is triggered first,
 * and we didn't re-attach rd handler in wr handler, then rd is
 * triggered, rd handler should be called.
 */

#include <nebase/evdp.h>
#include <nebase/sem.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

# define BUFLEN 4
char wbuf[BUFLEN] = {0x01, 0x02, 0x03, 0x04};
char rbuf[BUFLEN] = NEB_STRUCT_INITIALIZER;

int hup_ok = 0, read_ok = 0, write_ok = 0, timeout = 0;
int semid;

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
	fprintf(stdout, "in read handler\n");
	neb_evdp_source_t s = udata;
	int nread;
	if (neb_evdp_source_fd_get_nread(context, &nread) != 0) {
		fprintf(stderr, "failed to get nread\n");
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
	fprintf(stdout, "nread: %d, real read: %lld, fd: %d\n", nread, (long long int)nr, fd);
	if (nread <= nr)
		read_ok = 1;
	if (neb_evdp_source_os_fd_next_read(s, read_handler) != 0) {
		fprintf(stderr, "failed to set next rd handler\n");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	return NEB_EVDP_CB_CONTINUE;
}

static neb_evdp_cb_ret_t write_handler(int fd, void *udata _nattr_unused, const void *context _nattr_unused)
{
	fprintf(stdout, "in write handler\n");
	ssize_t nw = write(fd, wbuf, sizeof(wbuf));
	if (nw == -1) {
		perror("write");
		return NEB_EVDP_CB_BREAK_ERR;
	}
	neb_sem_proc_post(semid, 0);
	fprintf(stdout, "write %lld bytes to fd %d\n", (long long int)nw, fd);
	write_ok = 1;
	return NEB_EVDP_CB_CONTINUE;
}

int main(void)
{
	char tmp_file[] = "/tmp/.nebase.test.sem-XXXXXX";
	int fd = mkstemp(tmp_file);
	if (fd == -1) {
		perror("mkstemp");
		return -1;
	}
	close(fd);

	semid = neb_sem_proc_create(tmp_file, 1);
	if (semid < 0) {
		fprintf(stderr, "failed to create sem on file %s\n", tmp_file);
		unlink(tmp_file);
		return -1;
	}

	int sv[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
		perror("socketpair");
		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
		return -1;
	}

	int ret = 0;

	int cpid = fork();
	if (cpid == -1) {
		perror("fork");
		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
		return -1;
	} else if (cpid == 0) {
		close(sv[1]);
		int fd = sv[0];

		struct timespec ts = {.tv_sec = 0, .tv_nsec = 200000000}; // 200ms
		if (neb_sem_proc_wait_count(semid, 0, 1, &ts) != 0) {
			fprintf(stderr, "failed to wait parent sem\n");
			close(fd);
			return -1;
		}

		if (write(fd, wbuf, sizeof(wbuf)) == -1) {
			perror("write");
			close(fd);
			return -1;
		}
		fprintf(stdout, "child: write finished\n");

		ssize_t nr = read(fd, rbuf, sizeof(rbuf));
		if (nr == -1) {
			perror("read");
			close(fd);
			return -1;
		}
		fprintf(stdout, "child: read in %lld bytes from fd %d\n", (long long int)nr, fd);

		close(fd);
	} else {
		close(sv[0]);
		int fd = sv[1];

		neb_evdp_queue_t dq = NULL;
		neb_evdp_source_t ds = NULL;
		neb_evdp_source_t dst = NULL;

		dq = neb_evdp_queue_create(0);
		if (!dq) {
			fprintf(stderr, "failed to create evdp queue\n");
			return -1;
		}

		ds = neb_evdp_source_new_os_fd(fd, hup_handler);
		if (!ds) {
			fprintf(stderr, "failed to create os_fd evdp source\n");
			ret = -1;
			goto exit_clean;
		}
		neb_evdp_source_set_udata(ds, ds);
		if (neb_evdp_source_os_fd_next_read(ds, read_handler) != 0) // before attach
			fprintf(stderr, "Failed to set rd handler\n");
		if (neb_evdp_queue_attach(dq, ds) != 0) {
			fprintf(stderr, "failed to attach ro_fd source to queue\n");
			ret = -1;
			goto exit_clean;
		}
		if (neb_evdp_source_os_fd_next_write(ds, write_handler) != 0) // after attach
			fprintf(stderr, "failed to set wr handler\n");

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
		} else if (!write_ok || !read_ok || !hup_ok) {
			fprintf(stderr, "write_ok: %d, read_ok: %d, hup_ok: %d\n", write_ok, read_ok, hup_ok);
			ret = -1;
		} else {
			fprintf(stdout, "All check OK\n");
		}

		neb_sem_proc_destroy(semid);
		unlink(tmp_file);
	}

	return ret;
}

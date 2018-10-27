
#include <nebase/syslog.h>
#include <nebase/pidfile.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define PIDFILE_CREATE_MODE 0644
#define PIDFILE_PIDBUF_SIZE 32

static int set_pid(int fd)
{
	char pidbuf[PIDFILE_PIDBUF_SIZE];
	int len = snprintf(pidbuf, sizeof(pidbuf), "%d", getpid());
	ssize_t nw = write(fd, pidbuf, len);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "write: %m");
		return -1;
	} else if (nw != len) {
		neb_syslog(LOG_ERR, "not all data written");
		return -1;
	}
	return 0;
}

static pid_t get_pid(int fd)
{
	char pidbuf[PIDFILE_PIDBUF_SIZE];
	ssize_t nr = read(fd, pidbuf, sizeof(pidbuf));
	if (nr == -1) {
		neb_syslog(LOG_ERR, "read: %m");
		return -1;
	} else if (nr == sizeof(pidbuf) && pidbuf[nr - 1] != '\0') {
		neb_syslog(LOG_ERR, "Invalid pid value: size %ld overflow", nr);
		return -1;
	}
	pidbuf[nr] = '\0';
	pid_t pid = atoi(pidbuf);
	if (!pid) {
		neb_syslog(LOG_ERR, "Invalid pid value: not an interger");
		return -1;
	} else {
		return pid;
	}
}

int neb_pidfile_open(const char *path, pid_t *locker)
{
	*locker = 0;
	int fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, PIDFILE_CREATE_MODE);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "open(%s): %m", path);
		return -1;
	}
	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};
	if (fcntl(fd, F_GETLK, &lock) == -1) {
		neb_syslog(LOG_ERR, "fcntl(F_GETLK): %m");
		close(fd);
		return -1;
	}
	if (lock.l_type != F_UNLCK) {
		pid_t pid = get_pid(fd);
		*locker = pid;
		close(fd);
		return -1;
	}
	return fd;
}

pid_t neb_pidfile_write(int fd)
{
	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};
	if (fcntl(fd, F_SETLK, &lock) == -1) {
		switch (errno) {
		case EACCES:
		case EAGAIN:
			break;
		default:
			neb_syslog(LOG_ERR, "fcntl(F_SETLK): %m");
			return -1;
			break;
		}
		return get_pid(fd);
	} else {
		if (set_pid(fd) != 0)
			return -1;
		else
			return 0;
	}
}

void neb_pidfile_close(int fd)
{
	close(fd);
}

void neb_pidfile_remove(const char *path)
{
	unlink(path);
}

int neb_pidlock(int dirfd, const char *filename, pid_t *locker)
{
	*locker = 0;
	int fd = openat(dirfd, filename, O_RDWR | O_CREAT | O_CLOEXEC, PIDFILE_CREATE_MODE);
	if (fd == -1) {
		neb_syslog(LOG_ERR, "openat: %m");
		return -1;
	}

	struct flock lock = {
		.l_type = F_WRLCK,
		.l_whence = SEEK_SET,
		.l_start = 0,
		.l_len = 0,
	};
	if (fcntl(fd, F_SETLK, &lock) == -1) {
		switch (errno) {
		case EACCES:
		case EAGAIN:
			break;
		default:
			neb_syslog(LOG_ERR, "fcntl(F_SETLK): %m");
			close(fd);
			return -1;
			break;
		}
		pid_t pid = get_pid(fd);
		*locker = pid;
		close(fd);
		return -1;
	} else {
		if (set_pid(fd) != 0) {
			close(fd);
			return -1;
		} else {
			return fd;
		}
	}
}

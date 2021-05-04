
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/file/copy.h>

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <unistd.h>
#include <errno.h>

#if defined(OS_LINUX)
# include <sys/sendfile.h>
# define USE_SENDFILE
#elif defined(OS_SOLARIS)
# include <sys/sendfile.h>
# define USE_SENDFILE
#endif

#if defined(OS_LINUX)
# include <linux/version.h>
# if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 27) || __GLIBC__ > 2
#  define USE_COPY_FILE_RANGE
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
#  include <sys/syscall.h>
#  ifdef __NR_copy_file_range
#   define USE_COPY_FILE_RANGE
static inline ssize_t copy_file_range(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out, size_t len, unsigned int flags)
{
	return syscall(__NR_copy_file_range, fd_in, off_in, fd_out, off_out, len, flags);
}
#  endif
# endif
#elif defined(OS_FREEBSD)
# if __FreeBSD__ >= 13
#  define USE_COPY_FILE_RANGE
# endif
#else
// copy_file_range is not available
#endif

#define NEB_FILE_COPY_BUFFER_SIZE 262144 // 256KB

static ssize_t neb_file_buffered_copy(int out_fd, int in_fd, size_t count) {
	u_char buf[NEB_FILE_COPY_BUFFER_SIZE];

	size_t left = count;
	while (left > 0) {
		size_t tr = MIN(left, NEB_FILE_COPY_BUFFER_SIZE);
		ssize_t nr = read(in_fd, buf, tr);
		switch (nr) {
		case -1:
			neb_syslogl(LOG_ERR, "read: %m");
			/* fall through */
		case 0:
			return count - left;
			break;
		default:
			left -= nr;
			break;
		}

		off_t offset = 0;
		while (nr > 0) {
			ssize_t nw = write(out_fd, buf + offset, (size_t)nr);
			switch (nw) {
			case -1:
				neb_syslogl(LOG_ERR, "write: %m");
				return -1;
			case 0:
				neb_syslogl(LOG_ERR, "write: unexpected written of zero bytes");
				return -1;
			default:
				nr -= nw;
				offset += nw;
				break;
			}
		}
	}

	return count;
}

static ssize_t neb_file_buffered_copy_with_offset(int out_fd, int in_fd, off_t *offset, size_t count) {
	if (offset) {
		off_t old_offset = lseek(in_fd, 0, SEEK_CUR);
		if (old_offset == -1) {
			neb_syslogl(LOG_ERR, "lseek(SEEK_CUR): %m");
		}

		if (lseek(in_fd, *offset, SEEK_SET) == -1) {
			neb_syslogl(LOG_ERR, "lseek(SEEK_SET): %m");
			return -1;
		}

		ssize_t nc = neb_file_buffered_copy(out_fd, in_fd, count);
		if (nc > 0)
			*offset += nc;

		// restore old in file offset
		if (lseek(in_fd, old_offset, SEEK_SET) == -1) {
			neb_syslogl(LOG_ERR, "lseek(SEEK_SET): %m");
			return -1;
		}

		return nc;
	} else {
		return neb_file_buffered_copy(out_fd, in_fd, count);
	}
}

ssize_t neb_file_sys_copy(int out_fd, int in_fd, off_t *offset, size_t count) {
	ssize_t ret = 0;
#ifdef USE_SENDFILE
	ret = sendfile(out_fd, in_fd, offset, count);
	if (ret == -1) {
		switch (errno) {
		case ENOSYS: // syscall not available
		case EINVAL:
			ret = neb_file_buffered_copy_with_offset(out_fd, in_fd, offset, count);
			break;
		default:
			neb_syslogl(LOG_ERR, "sendfile: %m");
			break;
		}
	}
#else
	if (offset) {
		ret = neb_file_buffered_copy_with_offset(out_fd, in_fd, offset, count);
	} else {
		ret = neb_file_buffered_copy(out_fd, in_fd, count);
	}
#endif
	return ret;
}

ssize_t neb_file_fs_copy(int out_fd, int in_fd, off_t *offset, size_t count) {
	ssize_t ret = 0;
#ifdef USE_COPY_FILE_RANGE
	ret = copy_file_range(in_fd, offset, out_fd, NULL, count, 0);
	if (ret == -1) {
		switch (errno) {
		case ENOSYS: // syscall not available
		case EINVAL:
		case EXDEV: // not on the same filesystem
			ret = neb_file_sys_copy(out_fd, in_fd, offset, count);
			break;
		default:
			break;
		}
	}
#else
	ret = neb_file_sys_copy(out_fd, in_fd, offset, count);
#endif
	return ret;
}


#include "options.h"

#include <nebase/syslog.h>
#include <nebase/file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#if defined(OS_LINUX)
# include <linux/version.h>
# if (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 28) || __GLIBC__ > 2
#  define USE_STATX
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
#  include <sys/syscall.h>
#  ifdef __NR_statx
#   define USE_STATX
#   include <linux/stat.h>
static inline ssize_t statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf)
{
	return syscall(__NR_statx, dirfd, pathname, flags, mask, statxbuf);
}
#  endif
# endif
# ifndef USE_STATX
#  include <sys/sysmacros.h>
# endif
#elif defined(OS_SOLARIS)
# include <sys/mkdev.h>
#endif

neb_ftype_t neb_file_get_type(const char *path)
{
	mode_t fmod;
#if defined(USE_STATX)
	struct statx s;
	if (statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_TYPE, &s) == -1) {
		if (errno == ENOENT)
			return NEB_FTYPE_NOENT;
		neb_syslog(LOG_ERR, "statx(%s): %m", path);
		return NEB_FTYPE_UNKNOWN;
	}
	fmod = s.stx_mode;
#else
	struct stat s;
	if (fstatat(AT_FDCWD, path, &s, AT_SYMLINK_NOFOLLOW) == -1) {
		if (errno == ENOENT)
			return NEB_FTYPE_NOENT;
		neb_syslog(LOG_ERR, "fstatat(%s): %m", path);
		return NEB_FTYPE_UNKNOWN;
	}
	fmod = s.st_mode;
#endif

	switch (fmod & S_IFMT) {
	case S_IFREG:
		return NEB_FTYPE_REG;
	case S_IFDIR:
		return NEB_FTYPE_DIR;
	case S_IFSOCK:
		return NEB_FTYPE_SOCK;
	case S_IFIFO:
		return NEB_FTYPE_FIFO;
	case S_IFLNK:
		return NEB_FTYPE_LINK;
	case S_IFBLK:
		return NEB_FTYPE_BLK;
	case S_IFCHR:
		return NEB_FTYPE_CHR;
	default:
		return NEB_FTYPE_UNKNOWN;
	}
}

int neb_file_get_ino(const char *path, neb_ino_t *ni)
{
#if defined(USE_STATX)
	struct statx s;
	if (statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_INO, &s) == -1) {
		neb_syslog(LOG_ERR, "statx(%s): %m", path);
		return -1;
	}
	ni->dev_major = s.stx_dev_major;
	ni->dev_minor = s.stx_dev_minor;
	ni->ino = s.stx_ino;
#else
	struct stat s;
	if (fstatat(AT_FDCWD, path, &s, AT_SYMLINK_NOFOLLOW) == -1) {
		neb_syslog(LOG_ERR, "fstatat(%s): %m", path);
		return -1;
	}
	ni->dev_major = major(s.st_dev);
	ni->dev_minor = minor(s.st_dev);
	ni->ino = s.st_ino;
#endif
	return 0;
}

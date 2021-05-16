
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/file/stat.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>

#if defined(OS_LINUX)
# include <linux/version.h>
# if __GLIBC_PREREQ(2, 28)
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
#elif defined(OSTYPE_SUN)
# include <sys/mkdev.h>
#endif

#ifndef O_NOATIME
# define O_NOATIME 0
#endif
#ifndef O_PATH
# define O_PATH 0
#endif

neb_ftype_t neb_subfile_get_type(int dirfd, const char *name)
{
	mode_t fmod;
#if defined(USE_STATX)
	struct statx s;
	if (statx(dirfd, name, AT_SYMLINK_NOFOLLOW, STATX_TYPE, &s) == -1) {
		if (errno == ENOENT)
			return NEB_FTYPE_NOENT;
		neb_syslogl(LOG_ERR, "statx(%s): %m", name);
		return NEB_FTYPE_UNKNOWN;
	}
	fmod = s.stx_mode;
#else
	struct stat s;
	if (fstatat(dirfd, name, &s, AT_SYMLINK_NOFOLLOW) == -1) {
		if (errno == ENOENT)
			return NEB_FTYPE_NOENT;
		neb_syslogl(LOG_ERR, "fstatat(%s): %m", name);
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

neb_ftype_t neb_file_get_type(const char *path)
{
	return neb_subfile_get_type(AT_FDCWD, path);
}

int neb_file_get_ino(const char *path, neb_ino_t *ni)
{
#if defined(USE_STATX)
	struct statx s;
	if (statx(AT_FDCWD, path, AT_SYMLINK_NOFOLLOW, STATX_INO, &s) == -1) {
		neb_syslogl(LOG_ERR, "statx(%s): %m", path);
		return -1;
	}
	ni->dev_major = s.stx_dev_major;
	ni->dev_minor = s.stx_dev_minor;
	ni->ino = s.stx_ino;
#else
	struct stat s;
	if (fstatat(AT_FDCWD, path, &s, AT_SYMLINK_NOFOLLOW) == -1) {
		neb_syslogl(LOG_ERR, "fstatat(%s): %m", path);
		return -1;
	}
	ni->dev_major = major(s.st_dev);
	ni->dev_minor = minor(s.st_dev);
	ni->ino = s.st_ino;
#endif
	return 0;
}

bool neb_file_exists(const char *path)
{
	int fd = open(path, O_RDONLY | O_PATH); //O_RDONLY will be ignored
	if (fd == -1) {
		return false;
	} else {
		close(fd);
		return true;
	}
}

int neb_subdir_open(int dirfd, const char *name, int *enoent)
{
	int fd = openat(dirfd, name, O_RDONLY | O_DIRECTORY | O_NOATIME);
	if (fd == -1) {
		if (enoent && errno == ENOENT)
			*enoent = 1;
		else
			neb_syslogl(LOG_ERR, "openat(%s): %m", name);
		return -1;
	}
	return fd;
}

int neb_dir_open(const char *path, int *enoent)
{
	return neb_subdir_open(AT_FDCWD, path, enoent);
}

bool neb_dir_exists(const char *path)
{
#if defined(O_SEARCH) // Open directory for search only, on Solaris and Illumos
	int fd = open(path, O_RDONLY | O_SEARCH | O_PATH);
	if (fd == -1) {
		return false;
	} else {
		close(fd);
		return true;
	}
#else
	int fd = open(path, O_RDONLY | O_DIRECTORY | O_PATH); //O_RDONLY will be ignored
	if (fd == -1) {
		return false;
	} else {
		close(fd);
		return true;
	}
#endif
}

int neb_dirfd_get_permission(int dirfd, neb_file_permission_t *perm)
{
#if defined(USE_STATX)
	struct statx s;
	if (statx(dirfd, "", AT_EMPTY_PATH, STATX_UID | STATX_GID | STATX_MODE, &s) == -1) {
		neb_syslogl(LOG_ERR, "statx: %m");
		return -1;
	}
	perm->uid = s.stx_uid;
	perm->gid = s.stx_gid;
	perm->mode = s.stx_mode & ~S_IFMT;
#else
	struct stat s;
# if defined(OS_LINUX)
	if (fstatat(dirfd, "", &s, AT_EMPTY_PATH) == -1) {
		neb_syslogl(LOG_ERR, "fstatat: %m");
# elif defined(OSTYPE_SUN)
	if (fstatat(dirfd, NULL, &s, 0) == -1) {
		neb_syslogl(LOG_ERR, "fstatat: %m");
# else
	// for OSTYPE_BSD, fstatat(".") may also be used
	if (fstat(dirfd, &s) == -1) {
		neb_syslogl(LOG_ERR, "fstat: %m");
# endif
		return -1;
	}
	perm->uid = s.st_uid;
	perm->gid = s.st_gid;
	perm->mode = s.st_mode & ~S_IFMT;
#endif
	return 0;
}

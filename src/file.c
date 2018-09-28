
#include <nebase/syslog.h>
#include <nebase/file.h>

#include <sys/stat.h>

neb_ftype_t neb_file_get_type(const char *path)
{
	struct stat s;
	if (stat(path, &s) == -1) {
		neb_syslog(LOG_ERR, "");
		return NEB_FTYPE_UNKNOWN;
	}

	switch (s.st_mode & S_IFMT) {
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
	struct stat s;
	if (stat(path, &s) == -1) {
		neb_syslog(LOG_ERR, "stat(%s): %m", path);
		return -1;
	}
	ni->dev = s.st_dev;
	ni->ino = s.st_ino;
	return 0;
}

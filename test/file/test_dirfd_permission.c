
#include <nebase/file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

const char t_dirname[] = "/tmp/.neb.testdir";
mode_t dirmode = 0640;

int main(void)
{
	int ret = 0;

	if (mkdir(t_dirname, dirmode) == -1) {
		perror("mkdir");
		return -1;
	}

	int fd = neb_dir_open(t_dirname, NULL);
	if (fd == -1) {
		fprintf(stderr, "Failed to open %s\n", t_dirname);
		ret = -1;
		goto exit_rmdir;
	}

	neb_file_permission_t perm;
	if (neb_dirfd_get_permission(fd, &perm) != 0) {
		fprintf(stderr, "Failed to get dir permission for %s\n", t_dirname);
		ret = -1;
		goto exit_close_fd;
	}

	uid_t uid = getuid();
	fprintf(stdout, "Exp  UID: %u, Mode: %x\n", uid, dirmode);
	fprintf(stdout, "Real UID: %u, Mode: %x\n", perm.uid, perm.mode);
	if (perm.uid != uid || perm.mode != dirmode)
		ret = -1;

exit_close_fd:
	close(fd);
exit_rmdir:
	if (rmdir(t_dirname) == -1) {
		perror("rmdir");
		ret = -1;
	}
	return ret;
}

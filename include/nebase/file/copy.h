
#ifndef NEB_FILE_COPY_H
#define NEB_FILE_COPY_H 1

#include <nebase/cdefs.h>

/**
 * Copy a range of data from one file to another
 *
 * \param[in,out] offset The offset of the in_fd
 *   If offset is not null, the file offset of in_fd will be restored, and the
 *   data pointed to by offset will be updated.
 *   If offset is null, then the file offset of in_fd will be updated.
 * \return -1 for error, or the real count of bytes copied.
 * \note this is a blocking function only for blocking IOs
 */
ssize_t neb_file_sys_copy(int out_fd, int in_fd, off_t *offset, size_t count)
	_nattr_warn_unused_result;

/**
 * Copy a range of data from one file to another on the same filesystem
 *
 * The params and the return value has the same meaning with `neb_file_sys_copy`
 * , and it will fall back to `neb_file_sys_copy` if not on the same filesystem.
 *
 * \note this is a blocking function only for blocking IOs
 */
ssize_t neb_file_fs_copy(int out_fd, int in_fd, off_t *offset, size_t count)
	_nattr_warn_unused_result;

#endif

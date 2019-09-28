
#ifndef NEB_FILE_H
#define NEB_FILE_H 1

#include "cdefs.h"

#include <sys/types.h>
#include <stdbool.h>

typedef enum {
	NEB_FTYPE_UNKNOWN = 0,
	NEB_FTYPE_NOENT,       // not exist
	NEB_FTYPE_REG,         // regular file
	NEB_FTYPE_DIR,         // directory
	NEB_FTYPE_SOCK,        // socket
	NEB_FTYPE_FIFO,        // fifo
	NEB_FTYPE_LINK,        // symbolic link
	NEB_FTYPE_BLK,         // block special
	NEB_FTYPE_CHR,         // character special
} neb_ftype_t;

/**
 * \param[in] path absolute path or relative path to pwd
 */
extern neb_ftype_t neb_file_get_type(const char *path)
	_nattr_nonnull((1));

/*
 * File Functions
 */

typedef struct {
	uint dev_major;
	uint dev_minor;
	ino_t ino;
} neb_ino_t;

/**
 * \param[in] path absolute path or relative path to pwd
 */
extern int neb_file_get_ino(const char *path, neb_ino_t *ni)
	_nattr_nonnull((1, 2));

/**
 * \note symlink will be followed
 */
extern bool neb_file_exists(const char *path)
	_nattr_warn_unused_result _nattr_nonnull((1));

/*
 * Directory Functions
 */

/**
 * \brief Get a dirfd
 * \param[in] path absolute path or relative path to pwd
 * \param[out] enoent if not null, it will be set if ENOENT
 *                    if null, error will be logged if ENOENT
 */
extern int neb_dir_open(const char *path, int *enoent)
	_nattr_nonnull((1));
/**
 * \brief Get a dirfd like neb_dir_open, but for a subdir
 * \param[in] dirfd parent directory
 */
extern int neb_subdir_open(int dirfd, const char *name, int *enoent)
	_nattr_nonnull((2));

/**
 * \note symlink will be followed
 */
extern bool neb_dir_exists(const char *path)
	_nattr_warn_unused_result _nattr_nonnull((1));

typedef struct {
	uid_t uid;
	gid_t gid;
	mode_t mode;
} neb_file_permission_t;

extern int neb_dirfd_get_permission(int dirfd, neb_file_permission_t *perm)
	_nattr_nonnull((2));

#endif

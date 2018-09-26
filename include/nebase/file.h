
#ifndef NEB_FILE_H
#define NEB_FILE_H 1

#include "cdefs.h"

#include <sys/types.h>

typedef enum {
	NEB_FTYPE_UNKNOWN = 0,
	NEB_FTYPE_REG,         // regular file
	NEB_FTYPE_DIR,         // directory
	NEB_FTYPE_SOCK,        // socket
	NEB_FTYPE_FIFO,        // fifo
	NEB_FTYPE_LINK,        // symbolic link
	NEB_FTYPE_BLK,         // block special
	NEB_FTYPE_CHR,         // character special
} neb_ftype_t;

extern neb_ftype_t neb_file_get_type(const char *path)
	neb_attr_nonnull((1));

typedef struct {
	dev_t dev;
	ino_t ino;
} neb_ino_t;

extern int neb_file_get_ino(const char *path, neb_ino_t *ni)
	neb_attr_nonnull((1, 2));

#endif
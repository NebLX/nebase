
#ifndef NEB_IO_H
#define NEB_IO_H 1

#include "cdefs.h"

/**
 * \param[in] fd -1 if /dev/null should be used
 */
extern int neb_io_redirect_stdin(int fd);
extern int neb_io_redirect_stdout(int fd);
extern int neb_io_redirect_stderr(int fd);
/**
 * \param[in] slave_fd should be the real slave fd
 */
extern int neb_io_redirect_pty(int slave_fd);

#endif

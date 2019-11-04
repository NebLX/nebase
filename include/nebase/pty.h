
#ifndef NEB_PTY_H
#define NEB_PTY_H 1

#include "cdefs.h"

#include <stddef.h>
#include <termios.h>

struct neb_pty_winsize {
	unsigned short int ws_row;
	unsigned short int ws_col;
	unsigned short int ws_xpixel;
	unsigned short int ws_ypixel;
};

extern int neb_pty_openpty(int *amaster, int *aslave)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
extern int neb_pty_ptsname(int master_fd, char *buf, size_t buflen)
	_nattr_warn_unused_result _nattr_nonnull((2));
extern int neb_pty_ttyname(int slave_fd, char *buf, size_t buflen)
	_nattr_warn_unused_result _nattr_nonnull((2));
extern int neb_pty_change_winsz(int master_fd, const struct neb_pty_winsize *winp)
	_nattr_warn_unused_result _nattr_nonnull((2));

extern int neb_pty_login_tty(int slave_fd)
	_nattr_warn_unused_result;
extern int neb_pty_make_ctty(int slave_fd);
extern void neb_pty_disconnect_ctty(void);

#endif

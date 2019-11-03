
#ifndef NEB_PTY_H
#define NEB_PTY_H 1

#include "cdefs.h"

#include <termios.h>

struct neb_pty_winsize {
	unsigned short int ws_row;
	unsigned short int ws_col;
	unsigned short int ws_xpixel;
	unsigned short int ws_ypixel;
};

extern int neb_pty_openpty(int *amaster, int *aslave, char *name,
                           struct termios *termp, struct neb_pty_winsize *winp)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

extern int neb_pty_login_tty(int slave_fd)
	_nattr_warn_unused_result;

#endif

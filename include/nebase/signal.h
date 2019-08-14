
#ifndef NEB_SIGNAL_H
#define NEB_SIGNAL_H 1

#include "cdefs.h"
#include "events.h"

#include <signal.h>

extern void neb_signal_proc_block_all(void);

/*
 * Signal handlers
 */

typedef struct {
	union {
		struct {
			int refresh; // filled only if this is set
			pid_t pid;
			int wstatus;
		} _sichld;
	} _sifields;
} neb_siginfo_t;

extern neb_siginfo_t neb_sigchld_info;

extern void neb_sigterm_handler(int signo);

extern void neb_sigchld_action(int sig, siginfo_t *info, void *ucontext);

#endif

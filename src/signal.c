
#include <nebase/signal.h>
#include <nebase/syslog.h>

#include <stddef.h>

neb_siginfo_t neb_sigchld_info = NEB_STRUCT_INITIALIZER;

void neb_signal_proc_block_all(void)
{
	sigset_t set;
	sigfillset(&set);
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
		neb_syslogl(LOG_CRIT, "sigprocmask(BLOCK_ALL): %m");
}

int neb_signal_proc_block_chld(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
		neb_syslogl(LOG_CRIT, "sigprocmask(BLOCK SIGCHLD): %m");
		return -1;
	}
	return 0;
}

int neb_signal_proc_unblock_chld(void)
{
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGCHLD);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
		neb_syslogl(LOG_CRIT, "sigprocmask(UNBLOCK SIGCHLD): %m");
		return -1;
	}
	return 0;
}

void neb_sigterm_handler(int sig _nattr_unused)
{
	thread_events |= T_E_QUIT;
}

void neb_sigchld_action(int sig _nattr_unused, siginfo_t *info, void *ucontext _nattr_unused)
{
	thread_events |= T_E_CHLD;
	if (neb_sigchld_info._sifields._sichld.refresh) {
		neb_sigchld_info._sifields._sichld.refresh = 0;
		neb_sigchld_info._sifields._sichld.pid = info->si_pid;
		neb_sigchld_info._sifields._sichld.wstatus = info->si_status;
	}
}

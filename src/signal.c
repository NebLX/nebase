
#include <nebase/signal.h>
#include <nebase/syslog.h>

#include <stddef.h>

neb_siginfo_t neb_sigchld_info = NEB_STRUCT_INITIALIZER;

void neb_signal_proc_block_all(void)
{
	sigset_t set;
	sigfillset(&set);
	if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
		neb_syslog(LOG_CRIT, "");
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

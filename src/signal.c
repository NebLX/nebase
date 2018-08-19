
#include <nebase/signal.h>

neb_siginfo_t neb_sigchld_info = {};

void neb_sigterm_handler(int sig __attribute_unused__)
{
	thread_events |= T_E_QUIT;
}

void neb_sigchld_action(int sig __attribute_unused__, siginfo_t *info, void *ucontext __attribute_unused__)
{
	thread_events |= T_E_CHLD;
	if (neb_sigchld_info._sifields._sichld.refresh) {
		neb_sigchld_info._sifields._sichld.refresh = 0;
		neb_sigchld_info._sifields._sichld.pid = info->si_pid;
		neb_sigchld_info._sifields._sichld.wstatus = info->si_status;
	}
}

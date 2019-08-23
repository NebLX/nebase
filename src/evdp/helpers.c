
#include <nebase/syslog.h>
#include <nebase/evdp.h>

neb_evdp_cb_ret_t neb_evdp_sock_log_on_hup(int fd, void *udata _nattr_unused, const void *context)
{
	int sockerr = 0;
	if (neb_evdp_source_fd_get_sockerr(context, &sockerr) != 0)
		neb_syslog(LOG_ERR, "Failed to get sockerr for fd %d", fd);
	if (sockerr != 0)
		neb_syslog(LOG_ERR, "Socket fd %d hup: %m", fd);
	return NEB_EVDP_CB_CLOSE;
}

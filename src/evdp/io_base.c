
#include <nebase/syslog.h>

#include "core.h"
#include "io_base.h"

#include <stdlib.h>

neb_evdp_cb_ret_t neb_evdp_sock_log_on_hup(int fd, void *udata _nattr_unused, const void *context)
{
	int sockerr = 0;
	if (neb_evdp_sock_get_sockerr(context, &sockerr) != 0)
		neb_syslog(LOG_ERR, "Failed to get sockerr for fd %d", fd);
	if (sockerr != 0)
		neb_syslog(LOG_ERR, "Socket fd %d hup: %m", fd);
	return NEB_EVDP_CB_CLOSE;
}

neb_evdp_source_t neb_evdp_source_new_ro_fd(int fd, neb_evdp_io_handler_t rf, neb_evdp_io_handler_t hf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_RO_FD;

	struct evdp_conf_ro_fd *conf = calloc(1, sizeof(struct evdp_conf_ro_fd));
	if (!conf) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->fd = fd;
	conf->do_read = rf;
	conf->do_hup = hf;
	s->conf = conf;

	s->context = evdp_create_source_ro_fd_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

neb_evdp_source_t neb_evdp_source_new_os_fd(int fd, neb_evdp_io_handler_t hf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_OS_FD;

	struct evdp_conf_fd *conf = calloc(1, sizeof(struct evdp_conf_fd));
	if (!conf) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->fd = fd;
	conf->do_hup = hf;
	s->conf = conf;

	s->context = evdp_create_source_os_fd_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

int neb_evdp_source_os_fd_reset(neb_evdp_source_t s, int fd)
{
	if (s->q_in_use) {
		neb_syslog(LOG_CRIT, "It's not allowed to reset attached source");
		return -1;
	}
	if (s->type != EVDP_SOURCE_OS_FD) {
		neb_syslog(LOG_CRIT, "It's not allowed to reset unknown source to os_fd");
		return -1;
	}
	if (!s->conf || !s->context) {
		neb_syslog(LOG_CRIT, "Incomplete os_fd source %p", s);
		return -1;
	}

	struct evdp_conf_fd *conf = s->conf;
	conf->fd = fd;
	evdp_reset_source_os_fd_context(s);

	return 0;
}

int neb_evdp_source_os_fd_next_read(neb_evdp_source_t s, neb_evdp_io_handler_t rf)
{
	struct evdp_conf_fd *conf = s->conf;
	conf->do_read = rf;
	if (!s->q_in_use) {
		evdp_source_os_fd_init_read(s, rf);
		return 0;
	} else if (rf) {
		return evdp_source_os_fd_reset_read(s);
	} else {
		return evdp_source_os_fd_unset_read(s);
	}
}

int neb_evdp_source_os_fd_next_write(neb_evdp_source_t s, neb_evdp_io_handler_t wf)
{
	struct evdp_conf_fd *conf = s->conf;
	conf->do_write = wf;
	if (!s->q_in_use) {
		evdp_source_os_fd_init_write(s, wf);
		return 0;
	} else if (wf) {
		return evdp_source_os_fd_reset_write(s);
	} else {
		return evdp_source_os_fd_unset_write(s);
	}
}

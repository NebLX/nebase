
#include <nebase/syslog.h>
#include <nebase/evdp/sys_timer.h>
#include <nebase/time.h>

#include "core.h"
#include "sys_timer.h"

#include <stdlib.h>

neb_evdp_source_t neb_evdp_source_new_itimer_s(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ITIMER_SEC;

	struct evdp_conf_itimer *conf = calloc(1, sizeof(struct evdp_conf_itimer));
	if (!conf) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->sec = val;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_itimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

neb_evdp_source_t neb_evdp_source_new_itimer_ms(unsigned int ident, int val, neb_evdp_wakeup_handler_t tf)
{
	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ITIMER_MSEC;

	struct evdp_conf_itimer *conf = calloc(1, sizeof(struct evdp_conf_itimer));
	if (!conf) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->msec = val;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_itimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	return s;
}

neb_evdp_source_t neb_evdp_source_new_abstimer(unsigned int ident, int sec_of_day, neb_evdp_wakeup_handler_t tf)
{
	if (sec_of_day < 0 || sec_of_day >= TOTAL_DAY_SECONDS) {
		neb_syslog(LOG_ERR, "Invalid sec_of_day value: %d", sec_of_day);
		return NULL;
	}

	neb_evdp_source_t s = calloc(1, sizeof(struct neb_evdp_source));
	if (!s) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	s->type = EVDP_SOURCE_ABSTIMER;

	struct evdp_conf_abstimer *conf = calloc(1, sizeof(struct evdp_conf_abstimer));
	if (!conf) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		neb_evdp_source_del(s);
		return NULL;
	}
	conf->ident = ident;
	conf->sec_of_day = sec_of_day;
	conf->do_wakeup = tf;
	s->conf = conf;

	s->context = evdp_create_source_abstimer_context(s);
	if (!s->context) {
		neb_evdp_source_del(s);
		return NULL;
	}

	if (evdp_source_abstimer_regulate(s) != 0) {
		neb_syslog(LOG_ERR, "Failed to set initial wakeup time");
		return NULL;
	}

	return s;
}

int neb_evdp_source_abstimer_regulate(neb_evdp_source_t s, int sec_of_day)
{
	if (s->type != EVDP_SOURCE_ABSTIMER) {
		neb_syslog(LOG_ERR, "Invalid evdp_source type %d to regulate abstime", s->type);
		return -1;
	}
	if (sec_of_day >= 0) {
		if (sec_of_day >= TOTAL_DAY_SECONDS) {
			neb_syslog(LOG_ERR, "Invalid sec_of_day value: %d", sec_of_day);
			return -1;
		}
		struct evdp_conf_abstimer *conf = s->conf;
		conf->sec_of_day = sec_of_day;
	}
	return evdp_source_abstimer_regulate(s);
}

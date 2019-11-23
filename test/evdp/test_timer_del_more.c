
#include <nebase/cdefs.h>
#include <nebase/evdp/base.h>
#include <nebase/events.h>

#include <stdio.h>

static int run_into_sys_timer = 0;
static int run_into_cb2 = 0;

static neb_evdp_timer_point tp_to_del = NULL;

static neb_evdp_timeout_ret_t timer_cb3(void *udata _nattr_unused)
{
	thread_events |= T_E_QUIT;
	fprintf(stdout, "ok: running into cb3\n");
	return NEB_EVDP_TIMEOUT_KEEP;
}

static neb_evdp_timeout_ret_t timer_cb2(void *udata _nattr_unused)
{
	fprintf(stderr, "error: running into cb2, which should be deleted\n");
	run_into_cb2 = 1;
	return NEB_EVDP_TIMEOUT_KEEP;
}

static neb_evdp_timeout_ret_t timer_cb1(void *udata)
{
	neb_evdp_timer_t t = udata;
	if (tp_to_del) {
		fprintf(stderr, "ok: running del more timer\n");
		neb_evdp_timer_del_point(t, tp_to_del);
	} else {
		fprintf(stderr, "error: not running del more timer\n");
	}
	return NEB_EVDP_TIMEOUT_KEEP;
}

static neb_evdp_cb_ret_t sys_timer_cb(unsigned int id _nattr_unused, long overrun _nattr_unused, void *data _nattr_unused)
{
	fprintf(stderr, "run into sys timer callback\n");
	run_into_sys_timer = 1;
	return NEB_EVDP_CB_BREAK_EXP;
}

int main(void)
{
	neb_evdp_queue_t q = neb_evdp_queue_create(0);
	if (!q) {
		fprintf(stderr, "failed to create evdp queue\n");
		return -1;
	}

	int ret = 0;
	neb_evdp_source_t s = neb_evdp_source_new_itimer_ms(0, 100, sys_timer_cb);
	if (!s) {
		fprintf(stderr, "failed to create sys timer evdp source\n");
		ret = -1;
		goto exit_destroy_queue;
	}
	if (neb_evdp_queue_attach(q, s) != 0) {
		fprintf(stderr, "failed to attach sys timer source to queue\n");
		ret = -1;
		goto exit_destroy_sys_timer;
	}

	neb_evdp_timer_t t = neb_evdp_timer_create(1, 1);
	if (!t) {
		fprintf(stderr, "failed to create evdp timer\n");
		ret = -1;
		goto exit_detach_sys_timer;
	}
	neb_evdp_queue_set_timer(q, t);

	neb_evdp_timer_point tp1 = neb_evdp_timer_new_point(t, 1, timer_cb1, t);
	if (!tp1) {
		fprintf(stderr, "failed to add internal timer 1\n");
		ret = -1;
		goto exit_destroy_timer;
	}

	tp_to_del = neb_evdp_timer_new_point(t, 2, timer_cb2, t);
	if (!tp_to_del) {
		fprintf(stderr, "failed to add internal timer 2");
		ret = -1;
		goto exit_del_tp1;
	}

	neb_evdp_timer_point tp3 = neb_evdp_timer_new_point(t, 2, timer_cb3, t);
	if (!tp3) {
		fprintf(stderr, "failed to add internal timer 3");
		ret = -1;
		neb_evdp_timer_del_point(t, tp_to_del);
		goto exit_del_tp1;
	}

	if (neb_evdp_queue_run(q) != 0) {
		fprintf(stderr, "error occured while running evdp queue\n");
		ret = -1;
	}

	if (run_into_cb2 || run_into_sys_timer)
		ret = -1;

	neb_evdp_timer_del_point(t, tp3);
exit_del_tp1:
	neb_evdp_timer_del_point(t, tp1);
exit_destroy_timer:
	neb_evdp_timer_destroy(t);
exit_detach_sys_timer:
	if (neb_evdp_queue_detach(q, s, 0) != 0)
		fprintf(stderr, "failed to detach sys timer source\n");
exit_destroy_sys_timer:
	neb_evdp_source_del(s);
exit_destroy_queue:
	neb_evdp_queue_destroy(q);
	return ret;
}

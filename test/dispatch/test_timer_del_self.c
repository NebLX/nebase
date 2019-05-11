
#include <nebase/cdefs.h>
#include <nebase/dispatch.h>
#include <nebase/events.h>

#include <stdio.h>

static int run_into_sys_timer = 0;

static void *timer_point = NULL;

static void timer_cb(void *udata)
{
	dispatch_timer_t t = udata;
	if (timer_point) {
		fprintf(stderr, "ok: running del self timer\n");
		neb_dispatch_timer_del(t, timer_point);
		thread_events |= T_E_QUIT;
	} else {
		fprintf(stderr, "error: not running del self timer\n");
	}
}

static dispatch_cb_ret_t sys_timer_cb(unsigned int id _nattr_unused, void *data _nattr_unused)
{
	fprintf(stderr, "run into sys timer callback\n");
	run_into_sys_timer = 1;
	return DISPATCH_CB_BREAK;
}

int main(void)
{
	dispatch_queue_t q = neb_dispatch_queue_create(0);
	if (!q) {
		fprintf(stderr, "failed to create dispatch queue\n");
		return -1;
	}

	int ret = 0;
	dispatch_source_t s = neb_dispatch_source_new_itimer_msec(0, 100, sys_timer_cb);
	if (!s) {
		fprintf(stderr, "failed to create sys timer source\n");
		ret = -1;
		goto exit_destroy_queue;
	}
	if (neb_dispatch_queue_add(q, s) != 0) {
		fprintf(stderr, "failed to add sys timer source to queue\n");
		ret = -1;
		goto exit_destroy_sys_timer;
	}

	dispatch_timer_t t = neb_dispatch_timer_create(1, 1);
	if (!t) {
		fprintf(stderr, "failed to create dispatch timer\n");
		ret = -1;
		goto exit_detach_sys_timer;
	}
	neb_dispatch_queue_set_timer(q, t);

	timer_point = neb_dispatch_timer_add(t, 1, timer_cb, t);
	if (!timer_point) {
		fprintf(stderr, "failed to add internal timer\n");
		ret = -1;
		goto exit_destroy_timer;
	}

	neb_dispatch_queue_run(q);

	if (run_into_sys_timer)
		ret = -1;

	neb_dispatch_timer_del(t, timer_point);
exit_destroy_timer:
	neb_dispatch_timer_destroy(t);
exit_detach_sys_timer:
	neb_dispatch_queue_rm(q, s);
exit_destroy_sys_timer:
	neb_dispatch_source_del(s);
exit_destroy_queue:
	neb_dispatch_queue_destroy(q);
	return ret;
}

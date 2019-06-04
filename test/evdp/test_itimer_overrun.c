
#include <nebase/cdefs.h>
#include <nebase/evdp.h>
#include <nebase/time.h>

#include <stdio.h>
#include <unistd.h>

struct wakeup_udata {
	int count;
	int error;
};

static neb_evdp_cb_ret_t on_wakeup(unsigned int ident _nattr_unused, long overrun, void *udata)
{
	struct wakeup_udata *u = udata;

	struct timespec ts;
	if (neb_time_gettime_fast(&ts) != 0)
		return NEB_EVDP_CB_BREAK;
	fprintf(stdout, "Time: %lds %09ldns, ", (long)ts.tv_sec, (long)ts.tv_nsec);

	switch (u->count) {
	case 0:
		fprintf(stdout, "wakeup %d: overrun is %ld\n", u->count, overrun);
		u->count += 1;
		return NEB_EVDP_CB_CONTINUE;
		break;
	case 1:
		fprintf(stdout, "wakeup %d: overrun is %ld\n", u->count, overrun);
		u->count += 1;
		usleep(20000);
		return NEB_EVDP_CB_CONTINUE;
		break;
	case 2:
		fprintf(stdout, "wakeup %d: overrun is %ld\n", u->count, overrun);
		u->count += 1;
		if (overrun < 2) {
			fprintf(stderr, "overrun should >= 2\n");
			u->error = 1;
		}
		return NEB_EVDP_CB_BREAK;
		break;
	default:
		return NEB_EVDP_CB_BREAK;
		break;
	}
}

int main(void)
{
	int ret = 0;
	neb_evdp_queue_t dq = neb_evdp_queue_create(0);
	if (!dq) {
		fprintf(stderr, "failed to create evdp queue\n");
		return -1;
	}

	struct wakeup_udata u = {
		.count = 0,
		.error = 0,
	};

	neb_evdp_source_t ds = neb_evdp_source_new_itimer_ms(1, 10, on_wakeup);
	if (!ds) {
		fprintf(stderr, "failed to create itimer evdp source\n");
		ret = -1;
		goto exit_destroy_dq;
	}
	neb_evdp_source_set_udata(ds, &u);
	if (neb_evdp_queue_attach(dq, ds) != 0) {
		fprintf(stderr, "failed to attach itimer evdp source\n");
		ret = -1;
		goto exit_destroy_ds;
	}

	if (neb_evdp_queue_run(dq) != 0) {
		fprintf(stderr, "error occured while running evdp queue\n");
		ret = -1;
	}

	if (u.count != 3) {
		fprintf(stderr, "final count should be 3\n");
		ret = -1;
	}
	if (u.error)
		ret = -1;

	neb_evdp_queue_detach(dq, ds);
exit_destroy_ds:
	neb_evdp_source_del(ds);
exit_destroy_dq:
	neb_evdp_queue_destroy(dq);

	return ret;
}

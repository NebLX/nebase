
#include <nebase/evdp.h>
#include <nebase/events.h>

#include <stdio.h>
#include <stdlib.h>

static neb_evdp_queue_t q = NULL;
static neb_evdp_timer_t t = NULL;
static neb_evdp_timer_point tp = NULL;

static neb_evdp_timeout_ret_t timer_cb(void *udata)
{
	int *countp = udata;

	fprintf(stdout, "count: %d\n", *countp);
	switch (*countp) {
	case 0:
		if (neb_evdp_timer_point_reset(t, tp, neb_evdp_queue_get_abs_timeout(q, 1)) != 0) {
			fprintf(stderr, "failed to reset timer point\n");
			thread_events |= T_E_QUIT;
		}
		break;
	case 1:
		thread_events |= T_E_QUIT;
		break;
	}
	*countp += 1;

	return NEB_EVDP_TIMEOUT_KEEP;
}

int main(void)
{
	q = neb_evdp_queue_create(0);
	t = neb_evdp_timer_create(1, 1);
	neb_evdp_queue_set_timer(q, t);

	int count = 0;
	tp = neb_evdp_timer_new_point(t, 1, timer_cb, &count);

	if (neb_evdp_queue_run(q) != 0) {
		fprintf(stderr, "Failed to run queue\n");
		exit(-1);
	}

	int ret = 0;
	if (count != 2) {
		fprintf(stderr, "count should be equal to 2, but not %d\n", count);
		ret = -1;
	}

	neb_evdp_queue_destroy(q);
	neb_evdp_timer_destroy(t);
	return ret;
}


#include <nebase/evdp.h>

#include "ipv4.h"
#include "ipv6.h"

#include <stdio.h>

static neb_evdp_queue_t evdp_queue = NULL;
static neb_evdp_timer_t evdp_timer = NULL;

int main(void)
{
	int ret = 0;

	evdp_queue = neb_evdp_queue_create(0);
	if (!evdp_queue) {
		fprintf(stderr, "failed to create evdp queue\n");
		goto exit_fail;
	}

	evdp_timer = neb_evdp_timer_create(10, 10);
	if (!evdp_timer) {
		fprintf(stderr, "failed to create evdp timer\n");
		goto exit_fail;
	}
	neb_evdp_queue_set_timer(evdp_queue, evdp_timer);

	if (np_ipv4_init(evdp_queue) != 0) {
		fprintf(stderr, "failed to init ipv4 evdp resources\n");
		goto exit_fail;
	}
	if (np_ipv6_init(evdp_queue) != 0) {
		fprintf(stderr, "failed to init ipv6 evdp resources\n");
		goto exit_fail;
	}

	if (neb_evdp_queue_run(evdp_queue) != 0) {
		fprintf(stderr, "error occure while running evdp queue\n");
		goto exit_fail;
	}

exit_clean:
	if (evdp_timer)
		neb_evdp_timer_destroy(evdp_timer);
	if (evdp_queue)
		neb_evdp_queue_destroy(evdp_queue);
	return ret;
exit_fail:
	ret = -1;
	goto exit_clean;
}

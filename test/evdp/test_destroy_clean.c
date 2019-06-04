
#include <nebase/cdefs.h>
#include <nebase/evdp.h>

#include <stdio.h>

static int remove_called = 0;

static neb_evdp_cb_ret_t on_time(unsigned int ident _nattr_unused, long overrun _nattr_unused, void *udata _nattr_unused)
{
	return NEB_EVDP_CB_CONTINUE;
}

static int on_remove(neb_evdp_source_t s)
{
	fprintf(stdout, "on_remove called\n");
	remove_called = 1;
	neb_evdp_source_del(s);
	return 0;
}

int main(void)
{
	int ret = 0;
	neb_evdp_queue_t dq = neb_evdp_queue_create(0);
	if (!dq) {
		fprintf(stderr, "failed to create evdp queue\n");
		return -1;
	}

	neb_evdp_source_t ds = neb_evdp_source_new_abstimer(1, 7200, on_time);
	if (!ds) {
		fprintf(stderr, "failed to create abstimer source\n");
		ret = -1;
		goto exit_destroy_dq;
	}
	neb_evdp_source_set_on_remove(ds, on_remove);
	if (neb_evdp_queue_attach(dq, ds) != 0) {
		fprintf(stderr, "failed to add abstimer source to queue\n");
		ret = -1;
	}

exit_destroy_dq:
	neb_evdp_queue_destroy(dq);

	if (!remove_called) {
		fprintf(stderr, "on_remove is not called\n");
		ret = -1;
	}

	return ret;
}


#include <nebase/cdefs.h>
#include <nebase/dispatch.h>

#include <stdio.h>

static int remove_called = 0;

static dispatch_cb_ret_t on_time(unsigned int ident __attribute_unused__, void *data __attribute_unused__)
{
	return DISPATCH_CB_CONTINUE;
}

static void on_remove(dispatch_source_t s)
{
	fprintf(stdout, "on_remove called\n");
	remove_called = 1;
	neb_dispatch_source_del(s);
}

int main(int argc __attribute_unused__, char *argv[] __attribute_unused__)
{
	int ret = 0;
	dispatch_queue_t dq = neb_dispatch_queue_create(0);
	if (!dq) {
		fprintf(stderr, "failed to create dispatch queue\n");
		return -1;
	}

	dispatch_source_t ds = neb_dispatch_source_new_abstimer(1, 7200, 24, on_time);
	if (!ds) {
		fprintf(stderr, "failed to create abstimer source\n");
		ret = -1;
		goto exit_destroy_dq;
	}
	neb_dispatch_source_set_on_remove(ds, on_remove);
	if (neb_dispatch_queue_add(dq, ds) != 0) {
		fprintf(stderr, "failed to add abstimer source to queue\n");
		ret = -1;
	}

exit_destroy_dq:
	neb_dispatch_queue_destroy(dq);

	if (!remove_called) {
		fprintf(stderr, "on_remove is not called\n");
		ret = -1;
	}

	return ret;
}

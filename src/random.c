
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/random.h>
#include <nebase/obstack.h>

#if defined(OS_LINUX)
# include <bsd/stdlib.h>
#else
# include <stdlib.h>
#endif

#include <sys/queue.h>
#include <string.h>

#define RANDOM_POOL_SLICE_TOTAL_NUMBER 10
#define RANDOM_POOL_SLICE_RANDOM_NUMBER 8

struct neb_random_node {
	TAILQ_ENTRY(neb_random_node) list;
	int64_t value;
};

TAILQ_HEAD(neb_random_node_list, neb_random_node);

struct neb_random_slice {
	struct neb_random_node_list nlist;
	unsigned int count;
};

struct neb_random_bucket {
	struct neb_random_slice _slices[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	struct neb_random_slice *slices[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	unsigned int count;
};

struct neb_random_pool {
	struct neb_random_bucket *bucket;
	struct neb_random_node_list in_use;
	struct obstack obs;
};

struct neb_random_ring {
	struct neb_random_bucket *pick_bucket;
	struct neb_random_bucket *put_bucket;
	struct neb_random_node_list in_use;
	struct obstack obs;
};

uint32_t neb_random_uint32(void)
{
	return arc4random();
}

void neb_random_buf(void *buf, size_t nbytes)
{
	arc4random_buf(buf, nbytes);
}

uint32_t neb_random_uniform(uint32_t upper_bound)
{
	return arc4random_uniform(upper_bound);
}

int64_t neb_random_node_value(neb_random_node_t n)
{
	return n->value;
}

static struct neb_random_bucket *neb_random_bucket_create(void)
{
	struct neb_random_bucket *b = calloc(1, sizeof(struct neb_random_bucket));
	if (!b) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	for (int i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER; i++) {
		b->slices[i] = &b->_slices[i];
		TAILQ_INIT(&b->slices[i]->nlist);
	}

	return b;
}

static void neb_random_bucket_destroy(struct neb_random_bucket *b)
{
	free(b);
}

static void neb_random_slice_reverse(struct neb_random_slice *s)
{
	struct neb_random_node_list tl = TAILQ_HEAD_INITIALIZER(tl);
	while (!TAILQ_EMPTY(&s->nlist)) {
		struct neb_random_node *n = TAILQ_FIRST(&s->nlist);
		TAILQ_REMOVE(&s->nlist, n, list);
		TAILQ_INSERT_HEAD(&tl, n, list);
	}
	s->nlist = tl;
}

static void neb_random_bucket_confuse(struct neb_random_bucket *b)
{
	int indexes[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	for (int i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER; i++)
		indexes[i] = i;
	for (uint32_t i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER >> 1; i++) {
		uint32_t j = arc4random_uniform(RANDOM_POOL_SLICE_TOTAL_NUMBER - i - 1) + i + 1;
		int si = indexes[j];
		indexes[j] = indexes[i];
		indexes[i] = si; // could be skipped as it's no longer needed
		neb_random_slice_reverse(b->slices[si]);
	}
}

static void neb_random_bucket_put(struct neb_random_bucket *b, neb_random_node_t n)
{
	uint32_t rn;
	if (b->slices[0]->count < (b->count >> 3)) // insert to slice0 only if too small
		rn = 0;
	else
		rn = arc4random_uniform(RANDOM_POOL_SLICE_RANDOM_NUMBER);

	struct neb_random_slice *s = b->slices[rn];
	TAILQ_INSERT_TAIL(&s->nlist, n, list);
	s->count += 1;
	b->count += 1;

	// reorder
	for (uint32_t i = rn + 1; i < RANDOM_POOL_SLICE_RANDOM_NUMBER; i++) {
		struct neb_random_slice *s = b->slices[i-1];
		if (b->slices[i]->count < s->count) {
			b->slices[i-1] = b->slices[i];
			b->slices[i] = s;
		} else {
			break;
		}
	}
}

neb_random_node_t neb_random_bucket_pick(struct neb_random_bucket *b)
{
	if (!b->count)
		return NULL;

	uint32_t rn;
	if (b->slices[RANDOM_POOL_SLICE_TOTAL_NUMBER-1]->count > (b->count >> 1))
		rn = RANDOM_POOL_SLICE_TOTAL_NUMBER-1;
	else
		rn = arc4random_uniform(RANDOM_POOL_SLICE_RANDOM_NUMBER) +
			(RANDOM_POOL_SLICE_TOTAL_NUMBER - RANDOM_POOL_SLICE_RANDOM_NUMBER);

	struct neb_random_node *n = NULL;
	for (; rn < RANDOM_POOL_SLICE_TOTAL_NUMBER; rn++) {
		struct neb_random_slice *s = b->slices[rn];
		n = TAILQ_FIRST(&s->nlist);
		if (n) {
			TAILQ_REMOVE(&s->nlist, n, list);
			s->count -= 1;
			b->count -= 1;
			break;
		}
	}

	// reorder
	for (uint32_t i = rn; i > 0; i--) {
		struct neb_random_slice *s = b->slices[i];
		if (b->slices[i-1]->count > s->count) {
			b->slices[i] = b->slices[i-1];
			b->slices[i-1] = s;
		} else {
			break;
		}
	}

	return n;
}

neb_random_pool_t neb_random_pool_create(void)
{
	struct neb_random_pool *p = calloc(1, sizeof(struct neb_random_pool));
	if (!p) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	TAILQ_INIT(&p->in_use);
	obstack_init(&p->obs);

	p->bucket = neb_random_bucket_create();
	if (!p->bucket) {
		neb_random_pool_destroy(p);
		return NULL;
	}

	return p;
}

void neb_random_pool_destroy(neb_random_pool_t p)
{
	if (p->bucket) {
		neb_random_bucket_destroy(p->bucket);
		p->bucket = NULL;
	}
	obstack_free(&p->obs, NULL);
	free(p);
}

int neb_random_pool_add(neb_random_pool_t p, int64_t value)
{
	struct neb_random_node *n = obstack_alloc(&p->obs, sizeof(struct neb_random_node));
	if (!n) {
		neb_syslogl(LOG_ERR, "obstack_alloc: %m");
		return -1;
	}
	memset(n, 0, sizeof(struct neb_random_node));
	n->value = value;

	neb_random_bucket_put(p->bucket, n);

	return 0;
}

int neb_random_pool_add_range(neb_random_pool_t p, int min, int max, int step)
{
	if (step <= 0)
		step = 1;
	for (int i = min; i <= max; i += step) {
		if (neb_random_pool_add(p, i) != 0)
			return -1;
	}
	return 0;
}

void neb_random_pool_confuse(neb_random_pool_t p)
{
	neb_random_bucket_confuse(p->bucket);
}

neb_random_node_t neb_random_pool_pick(neb_random_pool_t p)
{
	neb_random_node_t n = neb_random_bucket_pick(p->bucket);
	if (n)
		TAILQ_INSERT_HEAD(&p->in_use, n, list);
	return n;
}

void neb_random_pool_put(neb_random_pool_t p, neb_random_node_t n)
{
	TAILQ_REMOVE(&p->in_use, n, list);
	neb_random_bucket_put(p->bucket, n);
}

neb_random_ring_t neb_random_ring_create(void)
{
	struct neb_random_ring *r = calloc(1, sizeof(struct neb_random_ring));
	if (!r) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	TAILQ_INIT(&r->in_use);
	obstack_init(&r->obs);

	r->pick_bucket = neb_random_bucket_create();
	if (!r->pick_bucket) {
		neb_random_ring_destroy(r);
		return NULL;
	}
	r->put_bucket = neb_random_bucket_create();
	if (!r->put_bucket) {
		neb_random_ring_destroy(r);
		return NULL;
	}
	return r;
}

void neb_random_ring_destroy(neb_random_ring_t r)
{
	if (r->put_bucket) {
		neb_random_bucket_destroy(r->put_bucket);
		r->put_bucket = NULL;
	}
	if (r->pick_bucket) {
		neb_random_bucket_destroy(r->pick_bucket);
		r->pick_bucket = NULL;
	}
	obstack_free(&r->obs, NULL);
	free(r);
}

int neb_random_ring_add(neb_random_ring_t r, int64_t value)
{
	struct neb_random_node *n = obstack_alloc(&r->obs, sizeof(struct neb_random_node));
	if (!n) {
		neb_syslogl(LOG_ERR, "obstack_alloc: %m");
		return -1;
	}
	memset(n, 0, sizeof(struct neb_random_node));
	n->value = value;

	neb_random_bucket_put(r->pick_bucket, n);

	return 0;
}

int neb_random_ring_add_range(neb_random_ring_t r, int min, int max, int step)
{
	if (step <= 0)
		step = 1;
	for (int i = min; i <= max; i += step) {
		if (neb_random_ring_add(r, i) != 0)
			return -1;
	}
	return 0;
}

void neb_random_ring_confuse(neb_random_ring_t r)
{
	neb_random_bucket_confuse(r->pick_bucket);
	neb_random_bucket_confuse(r->put_bucket);
}

neb_random_node_t neb_random_ring_pick(neb_random_ring_t r)
{
	if (!r->pick_bucket->count) {
		struct neb_random_bucket *b = r->put_bucket;
		if (!b->count)
			return NULL;
		r->put_bucket = r->pick_bucket;
		r->pick_bucket = b;
	}
	neb_random_node_t n = neb_random_bucket_pick(r->pick_bucket);
	if (n)
		TAILQ_INSERT_HEAD(&r->in_use, n, list);
	return n;
}

void neb_random_ring_put(neb_random_ring_t r, neb_random_node_t n)
{
	TAILQ_REMOVE(&r->in_use, n, list);
	neb_random_bucket_put(r->put_bucket, n);
}

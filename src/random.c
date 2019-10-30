
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

struct neb_random_pool_slice {
	struct neb_random_node_list nlist;
	unsigned int count;
};

struct neb_random_pool {
	struct neb_random_pool_slice _slices[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	struct neb_random_pool_slice *slices[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	struct neb_random_node_list in_use;
	struct obstack obs;
	unsigned int count;
};

struct neb_random_ring {
	struct neb_random_pool *pick_pool;
	struct neb_random_pool *put_pool;
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

neb_random_pool_t neb_random_pool_create(void)
{
	struct neb_random_pool *p = calloc(1, sizeof(struct neb_random_pool));
	if (!p) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	for (int i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER; i++) {
		p->slices[i] = &p->_slices[i];
		TAILQ_INIT(&p->slices[i]->nlist);
	}
	TAILQ_INIT(&p->in_use);
	obstack_init(&p->obs);

	return p;
}

void neb_random_pool_destroy(neb_random_pool_t p)
{
	obstack_free(&p->obs, NULL);
	free(p);
}

int neb_random_pool_add(neb_random_pool_t p, int64_t value)
{
	struct neb_random_node *n = obstack_alloc(&p->obs, sizeof(struct neb_random_node));
	if (!n) {
		neb_syslog(LOG_ERR, "obstack_alloc: %m");
		return -1;
	}
	memset(n, 0, sizeof(struct neb_random_node));
	n->value = value;

	TAILQ_INSERT_HEAD(&p->in_use, n, list);
	neb_random_pool_put(p, n);

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

static void random_pool_slice_reverse(struct neb_random_pool_slice *s)
{
	struct neb_random_node_list tl = TAILQ_HEAD_INITIALIZER(tl);
	while (!TAILQ_EMPTY(&s->nlist)) {
		struct neb_random_node *n = TAILQ_FIRST(&s->nlist);
		TAILQ_REMOVE(&s->nlist, n, list);
		TAILQ_INSERT_HEAD(&tl, n, list);
	}
	s->nlist = tl;
}

void neb_random_pool_confuse(neb_random_pool_t p)
{
	int indexes[RANDOM_POOL_SLICE_TOTAL_NUMBER];
	for (int i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER; i++)
		indexes[i] = i;
	for (uint32_t i = 0; i < RANDOM_POOL_SLICE_TOTAL_NUMBER >> 1; i++) {
		uint32_t j = arc4random_uniform(RANDOM_POOL_SLICE_TOTAL_NUMBER - i - 1) + i + 1;
		int si = indexes[j];
		indexes[j] = indexes[i];
		indexes[i] = si; // could be skipped as it's no longer needed
		random_pool_slice_reverse(p->slices[si]);
	}
}

void neb_random_pool_put(neb_random_pool_t p, neb_random_node_t n)
{
	uint32_t rn;
	if (p->slices[0]->count < (p->count >> 3)) // insert to slice0 only if too small
		rn = 0;
	else
		rn = arc4random_uniform(RANDOM_POOL_SLICE_RANDOM_NUMBER);

	TAILQ_REMOVE(&p->in_use, n, list);
	struct neb_random_pool_slice *s = p->slices[rn];
	TAILQ_INSERT_TAIL(&s->nlist, n, list);
	s->count += 1;
	p->count += 1;

	// reorder
	for (uint32_t i = rn + 1; i < RANDOM_POOL_SLICE_RANDOM_NUMBER; i++) {
		struct neb_random_pool_slice *s = p->slices[i-1];
		if (p->slices[i]->count < s->count) {
			p->slices[i-1] = p->slices[i];
			p->slices[i] = s;
		} else {
			break;
		}
	}
}

neb_random_node_t neb_random_pool_pick(neb_random_pool_t p)
{
	if (!p->count)
		return NULL;

	uint32_t rn;
	if (p->slices[RANDOM_POOL_SLICE_TOTAL_NUMBER-1]->count > (p->count >> 1))
		rn = RANDOM_POOL_SLICE_TOTAL_NUMBER-1;
	else
		rn = arc4random_uniform(RANDOM_POOL_SLICE_RANDOM_NUMBER) +
			(RANDOM_POOL_SLICE_TOTAL_NUMBER - RANDOM_POOL_SLICE_RANDOM_NUMBER);

	struct neb_random_node *n = NULL;
	for (; rn < RANDOM_POOL_SLICE_TOTAL_NUMBER; rn++) {
		struct neb_random_pool_slice *s = p->slices[rn];
		n = TAILQ_FIRST(&s->nlist);
		if (n) {
			TAILQ_REMOVE(&s->nlist, n, list);
			s->count -= 1;
			p->count -= 1;
			TAILQ_INSERT_HEAD(&p->in_use, n, list);
			break;
		}
	}

	// reorder
	for (uint32_t i = rn; i > 0; i--) {
		struct neb_random_pool_slice *s = p->slices[i];
		if (p->slices[i-1]->count > s->count) {
			p->slices[i] = p->slices[i-1];
			p->slices[i-1] = s;
		} else {
			break;
		}
	}

	return n;
}

neb_random_ring_t neb_random_ring_create(void)
{
	struct neb_random_ring *r = calloc(1, sizeof(struct neb_random_ring));
	if (!r) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	obstack_init(&r->obs);

	r->pick_pool = neb_random_pool_create();
	if (!r->pick_pool) {
		neb_random_ring_destroy(r);
		return NULL;
	}
	r->put_pool = neb_random_pool_create();
	if (!r->put_pool) {
		neb_random_ring_destroy(r);
		return NULL;
	}
	return r;
}

void neb_random_ring_destroy(neb_random_ring_t r)
{
	if (r->put_pool) {
		neb_random_pool_destroy(r->put_pool);
		r->put_pool = NULL;
	}
	if (r->pick_pool) {
		neb_random_pool_destroy(r->pick_pool);
		r->pick_pool = NULL;
	}
	obstack_free(&r->obs, NULL);
	free(r);
}

int neb_random_ring_add(neb_random_ring_t r, int64_t value)
{
	struct neb_random_node *n = obstack_alloc(&r->obs, sizeof(struct neb_random_node));
	if (!n) {
		neb_syslog(LOG_ERR, "obstack_alloc: %m");
		return -1;
	}
	memset(n, 0, sizeof(struct neb_random_node));
	n->value = value;

	TAILQ_INSERT_HEAD(&r->pick_pool->in_use, n, list);
	neb_random_pool_put(r->pick_pool, n);

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
	neb_random_pool_confuse(r->pick_pool);
	neb_random_pool_confuse(r->put_pool);
}

neb_random_node_t neb_random_ring_pick(neb_random_ring_t r)
{
	if (!r->pick_pool->count) {
		struct neb_random_pool *p = r->put_pool;
		if (!p->count)
			return NULL;
		r->put_pool = r->pick_pool;
		r->pick_pool = p;
	}
	return neb_random_pool_pick(r->pick_pool);
}

void neb_random_ring_put(neb_random_ring_t r, neb_random_node_t n)
{
	neb_random_pool_put(r->put_pool, n);
}

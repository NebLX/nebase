
#include <nebase/syslog.h>

#include "timer.h"

#include <stdlib.h>
#include <string.h>

static int evdp_timer_rbtree_cmp_node(void *context, const void *node1, const void *node2);
static int evdp_timer_rbtree_cmp_key(void *context, const void *node, const void *key);
static rb_tree_ops_t evdp_timer_rbtree_ops = {
	.rbto_compare_nodes = evdp_timer_rbtree_cmp_node,
	.rbto_compare_key = evdp_timer_rbtree_cmp_key,
	.rbto_node_offset = offsetof(struct evdp_timer_rbtree_node, rbtree_context),
	.rbto_context = NULL,
};

static struct evdp_timer_cblist_node *evdp_timer_cblist_node_new(neb_evdp_timeout_handler_t cb, void *udata, neb_evdp_timer_t t)
{
	struct evdp_timer_cblist_node *n;
	if (t && t->lcache.count) {
		n = t->lcache.nodes[t->lcache.count - 1];
		t->lcache.count -= 1;
		memset(n, 0, sizeof(*n));
	} else {
		n = calloc(1, sizeof(struct evdp_timer_cblist_node));
		if (!n) {
			neb_syslog(LOG_ERR, "calloc: %m");
			return NULL;
		}
	}

	n->on_timeout = cb;
	n->udata = udata;

	return n;
}

static void evdp_timer_cblist_node_free(struct evdp_timer_cblist_node *n, neb_evdp_timer_t t)
{
	if (t && t->lcache.count < t->lcache.size) {
		t->lcache.nodes[t->lcache.count] = n;
		t->lcache.count += 1;
	} else {
		free(n);
	}
}

static struct evdp_timer_rbtree_node * evdp_timer_rbtree_node_new(int64_t abs_msec, neb_evdp_timer_t t)
{
	struct evdp_timer_rbtree_node *n;
	if (t && t->tcache.count) {
		n = t->tcache.nodes[t->tcache.count - 1];
		t->tcache.count -= 1;
		memset(n, 0, sizeof(*n));
	} else {
		n = calloc(1, sizeof(struct evdp_timer_rbtree_node));
		if (!n) {
			neb_syslog(LOG_ERR, "calloc: %m");
			return NULL;
		}
	}

	n->msec = abs_msec;
	LIST_INIT(&n->cblist);

	return n;
}

static void evdp_timer_rbtree_node_free(struct evdp_timer_rbtree_node *n, neb_evdp_timer_t t)
{
	struct evdp_timer_cblist_node *node, *next;
	LIST_FOREACH_SAFE(node, &n->cblist, list, next) {
		evdp_timer_cblist_node_free(node, t);
	}
	LIST_INIT(&n->cblist);
	if (t && t->tcache.count < t->tcache.size) {
		t->tcache.nodes[t->tcache.count] = n;
		t->tcache.count += 1;
	} else {
		free(n);
	}
}

static int evdp_timer_rbtree_cmp_node(void *context _nattr_unused, const void *node1, const void *node2)
{
	const struct evdp_timer_rbtree_node *e = node1;
	const struct evdp_timer_rbtree_node *p = node2;
	if (e->msec < p->msec)
		return -1;
	else if (e->msec == p->msec)
		return 0;
	else
		return 1;
}

static int evdp_timer_rbtree_cmp_key(void *context _nattr_unused, const void *node, const void *key)
{
	const struct evdp_timer_rbtree_node *e = node;
	int64_t msec = *(int64_t *)key;
	if (e->msec < msec)
		return -1;
	else if (e->msec == msec)
		return 0;
	else
		return 1;
}

neb_evdp_timer_t neb_evdp_timer_create(int tcache_size, int lcache_size)
{
	struct neb_evdp_timer *dt = calloc(1, sizeof(struct neb_evdp_timer));
	if (!dt) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

	rb_tree_init(&dt->rbtree, &evdp_timer_rbtree_ops);

	dt->tcache.size = tcache_size;
	dt->tcache.nodes = malloc(tcache_size * sizeof(struct evdp_timer_rbtree_node *));
	if (!dt->tcache.nodes) {
		neb_syslog(LOG_ERR, "malloc: %m");
		neb_evdp_timer_destroy(dt);
		return NULL;
	}
	dt->tcache.count = 0;

	dt->lcache.size = lcache_size;
	dt->lcache.nodes = malloc(lcache_size * sizeof(struct evdp_timer_cblist_node *));
	if (!dt->lcache.nodes) {
		neb_syslog(LOG_ERR, "malloc: %m");
		neb_evdp_timer_destroy(dt);
		return NULL;
	}
	dt->lcache.count = 0;

	// allocate new intmax node and set ref
	struct evdp_timer_rbtree_node *max_node = evdp_timer_rbtree_node_new(INT64_MAX, NULL);
	if (!max_node) {
		neb_syslog(LOG_ERR, "Failed to allocate max node");
		neb_evdp_timer_destroy(dt);
		return NULL;
	}
	rb_tree_insert_node(&dt->rbtree, max_node);
	dt->ref_min_node = max_node;

	return dt;
}

void neb_evdp_timer_destroy(neb_evdp_timer_t t)
{
	struct evdp_timer_rbtree_node *node, *next;
	RB_TREE_FOREACH_SAFE(node, &t->rbtree, next) {
		rb_tree_remove_node(&t->rbtree, node);
		evdp_timer_rbtree_node_free(node, NULL);
	}

	if (t->lcache.nodes) {
		for (int i = 0; i < t->lcache.count; i++) {
			if (t->lcache.nodes[i])
				free(t->lcache.nodes[i]);
		}
		free(t->lcache.nodes);
	}

	if (t->tcache.nodes) {
		for (int i = 0; i < t->tcache.count; i++) {
			if (t->tcache.nodes[i])
				free(t->tcache.nodes[i]);
		}
		free(t->tcache.nodes);
	}

	free(t);
}

void* neb_evdp_timer_add(neb_evdp_timer_t t, int64_t abs_msec, neb_evdp_timeout_handler_t cb, void* udata)
{
	struct evdp_timer_rbtree_node *tn = evdp_timer_rbtree_node_new(abs_msec, t);
	if (!tn)
		return NULL;

	struct evdp_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
		evdp_timer_rbtree_node_free(tn, t);
		tn = tmp;
	} else { // inserted
		// Update ref min node
		if (evdp_timer_rbtree_cmp_node(NULL, tn, t->ref_min_node) < 0)
			t->ref_min_node = tn;
	}

	struct evdp_timer_cblist_node *ln = evdp_timer_cblist_node_new(cb, udata, t);
	if (!ln)
		return NULL;
	ln->ref_tnode = tn;

	LIST_INSERT_HEAD(&tn->cblist, ln, list);
	return ln;
}

void neb_evdp_timer_del(neb_evdp_timer_t t, void* n)
{
	struct evdp_timer_cblist_node *ln = n;
	struct evdp_timer_rbtree_node *tn = ln->ref_tnode;

	if (ln->running) // do not delete ourself in our cb
		return;

	if (tn) // maybe still in a pending list
		LIST_REMOVE(ln, list);
	evdp_timer_cblist_node_free(ln, t);

	if (tn && LIST_EMPTY(&tn->cblist)) {
		if (t->ref_min_node == tn) // Update ref min node
			t->ref_min_node = rb_tree_iterate(&t->rbtree, tn, RB_DIR_RIGHT);

		rb_tree_remove_node(&t->rbtree, tn);
		evdp_timer_rbtree_node_free(tn, t);
	}
}

int evdp_timer_get_min(neb_evdp_timer_t t, int64_t cur_msec)
{
	if (t->ref_min_node->msec == INT64_MAX)
		return -1;
	else if (t->ref_min_node->msec <= cur_msec)
		return 0;
	else
		return t->ref_min_node->msec - cur_msec;
}

int evdp_timer_run_until(neb_evdp_timer_t t, int64_t abs_msec)
{
	int count = 0;
	for (;;) {
		struct evdp_timer_rbtree_node *tn = t->ref_min_node;
		if (tn->msec <= abs_msec) {
			struct evdp_timer_cblist_node *ln, *next;
			for (ln = LIST_FIRST(&tn->cblist); ln; ln = next) {
				ln->running = 1;
				ln->on_timeout(ln->udata);
				ln->running = 0;
				// the cb here may remove following ln and tn,
				// so we need re-get the next node for them
				count += 1;
				next = LIST_NEXT(ln, list);
				LIST_REMOVE(ln, list);
				// do not free this node, user should do it, just clear ref
				ln->ref_tnode = NULL;
			}
		} else {
			break;
		}
		t->ref_min_node = rb_tree_iterate(&t->rbtree, tn, RB_DIR_RIGHT);
		rb_tree_remove_node(&t->rbtree, tn);
		evdp_timer_rbtree_node_free(tn, t);
	}
	return count;
}
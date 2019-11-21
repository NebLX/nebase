
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
	n->no_auto_del = 0;

	return n;
}

static void evdp_timer_rbtree_node_del(struct evdp_timer_rbtree_node *n, neb_evdp_timer_t t)
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

static void evdp_timer_rbtree_node_check_del(struct evdp_timer_rbtree_node *n, neb_evdp_timer_t t)
{
	if (n->no_auto_del)
		return;

	if (LIST_EMPTY(&n->cblist)) {
		if (t->ref_min_node == n) // Update ref min node
			t->ref_min_node = rb_tree_iterate(&t->rbtree, n, RB_DIR_RIGHT);

		rb_tree_remove_node(&t->rbtree, n);
		evdp_timer_rbtree_node_del(n, t);
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

	LIST_INIT(&dt->keeplist);

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
	struct evdp_timer_rbtree_node *tnode, *tnext;
	RB_TREE_FOREACH_SAFE(tnode, &t->rbtree, tnext) {
		rb_tree_remove_node(&t->rbtree, tnode);
		evdp_timer_rbtree_node_del(tnode, NULL);
	}
	struct evdp_timer_cblist_node *lnode, *lnext;
	LIST_FOREACH_SAFE(lnode, &t->keeplist, list, lnext) {
		evdp_timer_cblist_node_free(lnode, t);
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

neb_evdp_timer_point neb_evdp_timer_new_point(neb_evdp_timer_t t, int64_t abs_msec, neb_evdp_timeout_handler_t cb, void *udata)
{
	struct evdp_timer_rbtree_node *tn = evdp_timer_rbtree_node_new(abs_msec, t);
	if (!tn)
		return NULL;

	struct evdp_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
		evdp_timer_rbtree_node_del(tn, t);
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
	return (neb_evdp_timer_point)ln;
}

void neb_evdp_timer_del_point(neb_evdp_timer_t t, neb_evdp_timer_point p)
{
	struct evdp_timer_cblist_node *ln = p;
	struct evdp_timer_rbtree_node *tn = ln->ref_tnode;

	if (ln->running) {
		ln->ref_tnode = NULL; // indicate we want to be deleted
		return;
	}

	// maybe still in a pending list or in keeplist
	LIST_REMOVE(ln, list);
	evdp_timer_cblist_node_free(ln, t);

	if (tn)
		evdp_timer_rbtree_node_check_del(tn, t);
}

int neb_evdp_timer_point_reset(neb_evdp_timer_t t, neb_evdp_timer_point p, int64_t abs_msec)
{
	struct evdp_timer_rbtree_node *tn = evdp_timer_rbtree_node_new(abs_msec, t);
	if (!tn)
		return -1;

	struct evdp_timer_cblist_node *ln = p;
	struct evdp_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
		evdp_timer_rbtree_node_del(tn, t);
		tn = tmp;
		if (ln->ref_tnode == tn) // reset but still on the same tn
			return 0;
	} else { // inserted
		// Update ref min node
		if (evdp_timer_rbtree_cmp_node(NULL, tn, t->ref_min_node) < 0)
			t->ref_min_node = tn;
	}

	if (ln->running) {
		ln->ref_tnode = tn; // The insert will happen after the callback
	} else {
		LIST_REMOVE(ln, list); // either in keeplist or cblist

		struct evdp_timer_rbtree_node *old_tn = ln->ref_tnode;
		if (old_tn)
			evdp_timer_rbtree_node_check_del(old_tn, t);

		ln->ref_tnode = tn;
		LIST_INSERT_HEAD(&tn->cblist, ln, list);
	}
	return 0;
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
			tn->no_auto_del = 1;
			struct evdp_timer_cblist_node *ln;
			for (ln = LIST_FIRST(&tn->cblist); ln; ln = LIST_FIRST(&tn->cblist)) {
				LIST_REMOVE(ln, list); // keep ref_tnode
				ln->running = 1;
				neb_evdp_timeout_ret_t tret = ln->on_timeout(ln->udata);
				ln->running = 0;
				count += 1;
				if (!ln->ref_tnode) { // del_point is called in cb
					evdp_timer_cblist_node_free(ln, t);
				} else {
					switch (tret) {
					case NEB_EVDP_TIMEOUT_FREE: // free even if reset in cb
						ln->ref_tnode = NULL;
						evdp_timer_cblist_node_free(ln, t);
						continue;
						break;
					case NEB_EVDP_TIMEOUT_KEEP:
					default:
						if (ln->ref_tnode == tn) {
							ln->ref_tnode = NULL;
							LIST_INSERT_HEAD(&t->keeplist, ln, list);
						} else { // reset in cb
							LIST_INSERT_HEAD(&ln->ref_tnode->cblist, ln, list);
						}
						break;
					}
				}
			}
			tn->no_auto_del = 0;
		} else {
			break;
		}
		t->ref_min_node = rb_tree_iterate(&t->rbtree, tn, RB_DIR_RIGHT);
		rb_tree_remove_node(&t->rbtree, tn);
		evdp_timer_rbtree_node_del(tn, t);
	}
	return count;
}


#include <nebase/syslog.h>
#include <nebase/time.h>

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
			neb_syslogl(LOG_ERR, "calloc: %m");
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

static struct evdp_timer_rbtree_node *evdp_timer_rbtree_node_new(struct timespec *abs_ts, neb_evdp_timer_t t)
{
	struct evdp_timer_rbtree_node *n;
	if (t && t->tcache.count) {
		n = t->tcache.nodes[t->tcache.count - 1];
		t->tcache.count -= 1;
		memset(n, 0, sizeof(*n));
	} else {
		n = calloc(1, sizeof(struct evdp_timer_rbtree_node));
		if (!n) {
			neb_syslogl(LOG_ERR, "calloc: %m");
			return NULL;
		}
	}

	n->ts = *abs_ts;
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
		rb_tree_remove_node(&t->rbtree, n);
		evdp_timer_rbtree_node_del(n, t);
	}
}

#define CMP(v1, v2)     \
    if (v1 < v2)        \
        return -1;      \
    else if (v1 == v2)  \
        return 0;       \
    else                \
        return 1;       \

static int evdp_timer_rbtree_cmp_node(void *context _nattr_unused, const void *node1, const void *node2)
{
	const struct evdp_timer_rbtree_node *e = node1;
	const struct evdp_timer_rbtree_node *p = node2;

	if (e->ts.tv_sec == p->ts.tv_sec) {
		CMP(e->ts.tv_nsec, p->ts.tv_nsec)
	} else {
		CMP(e->ts.tv_sec, p->ts.tv_sec)
	}
}

static int evdp_timer_rbtree_cmp_key(void *context _nattr_unused, const void *node, const void *key)
{
	const struct evdp_timer_rbtree_node *e = node;
	const struct timespec *ts = key;

	if (e->ts.tv_sec == ts->tv_sec) {
		CMP(e->ts.tv_nsec, ts->tv_nsec)
	} else {
		CMP(e->ts.tv_sec, ts->tv_sec)
	}
}

neb_evdp_timer_t neb_evdp_timer_create(int tcache_size, int lcache_size)
{
	struct neb_evdp_timer *dt = calloc(1, sizeof(struct neb_evdp_timer));
	if (!dt) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	rb_tree_init(&dt->rbtree, &evdp_timer_rbtree_ops);

	dt->tcache.size = tcache_size;
	dt->tcache.nodes = malloc(tcache_size * sizeof(struct evdp_timer_rbtree_node *));
	if (!dt->tcache.nodes) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		neb_evdp_timer_destroy(dt);
		return NULL;
	}
	dt->tcache.count = 0;

	dt->lcache.size = lcache_size;
	dt->lcache.nodes = malloc(lcache_size * sizeof(struct evdp_timer_cblist_node *));
	if (!dt->lcache.nodes) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		neb_evdp_timer_destroy(dt);
		return NULL;
	}
	dt->lcache.count = 0;

	LIST_INIT(&dt->keeplist);

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

neb_evdp_timer_point neb_evdp_timer_new_point(neb_evdp_timer_t t, struct timespec* abs_ts, neb_evdp_timeout_handler_t cb, void* udata)
{
	struct evdp_timer_rbtree_node *tn = evdp_timer_rbtree_node_new(abs_ts, t);
	if (!tn)
		return NULL;

	struct evdp_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
		evdp_timer_rbtree_node_del(tn, t);
		tn = tmp;
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

int neb_evdp_timer_point_reset(neb_evdp_timer_t t, neb_evdp_timer_point p, struct timespec *abs_ts)
{
	struct evdp_timer_rbtree_node *tn = evdp_timer_rbtree_node_new(abs_ts, t);
	if (!tn)
		return -1;

	struct evdp_timer_cblist_node *ln = p;
	struct evdp_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
		evdp_timer_rbtree_node_del(tn, t);
		tn = tmp;
		if (ln->ref_tnode == tn) // reset but still on the same tn
			return 0;
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

void evdp_timer_fetch_neareast_ts(neb_evdp_timer_t t, struct timespec *cur_ts)
{
	struct evdp_timer_rbtree_node *min_node = RB_TREE_MIN(&t->rbtree);

	if (min_node == NULL) {
		neb_timespecclear(cur_ts);
	} else if (!neb_timespeccmp(&min_node->ts, cur_ts, >)) {
		cur_ts->tv_sec = 0;
		cur_ts->tv_nsec = 1;
	} else {
		neb_timespecsub(&min_node->ts, cur_ts, cur_ts);
	}
}

int evdp_timer_run_until(neb_evdp_timer_t t, struct timespec *abs_ts)
{
	int count = 0;
	struct evdp_timer_rbtree_node *tn, *nxt;
	for (tn = RB_TREE_MIN(&t->rbtree); tn; tn = nxt) {
		if (neb_timespeccmp(&tn->ts, abs_ts, >))
			break;

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

		nxt = RB_TREE_NEXT(&t->rbtree, tn);
		rb_tree_remove_node(&t->rbtree, tn);
		evdp_timer_rbtree_node_del(tn, t);
	}

	return count;
}

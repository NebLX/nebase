
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/dispatch.h>

#include "timer.h"

#include <stdlib.h>
#include <string.h>

static struct dispatch_timer_cblist_node *dispatch_timer_cblist_node_new(timer_cb_t cb, void *udata, dispatch_timer_t t);
static void dispatch_timer_cblist_node_free(struct dispatch_timer_cblist_node *n, dispatch_timer_t t);

static struct dispatch_timer_rbtree_node *dispatch_timer_rbtree_node_new(int64_t abs_msec, dispatch_timer_t t);
static void dispatch_timer_rbtree_node_free(struct dispatch_timer_rbtree_node *n, dispatch_timer_t t);

#if defined(HAVE_BSD_RBTREE)

static int dispatch_timer_rbtree_cmp_node(void *context, const void *node1, const void *node2);
static int dispatch_timer_rbtree_cmp_key(void *context, const void *node, const void *key);
static rb_tree_ops_t dispatch_timer_rbtree_ops = {
	.rbto_compare_nodes = dispatch_timer_rbtree_cmp_node,
	.rbto_compare_key = dispatch_timer_rbtree_cmp_key,
	.rbto_node_offset = offsetof(struct dispatch_timer_rbtree_node, node),
	.rbto_context = NULL,
};

#elif defined(HAVE_BSD_TREE)

static int dispatch_timer_rbtree_node_cmp(struct dispatch_timer_rbtree_node *e, struct dispatch_timer_rbtree_node *p);
# if defined(OS_DFLYBSD)
RB_PROTOTYPE_STATIC(dispatch_timer_rbtree, dispatch_timer_rbtree_node, node, dispatch_timer_rbtree_node_cmp);
# else
RB_PROTOTYPE_STATIC(dispatch_timer_rbtree, dispatch_timer_rbtree_node, node, dispatch_timer_rbtree_node_cmp)
# endif

#else
# error "fix me"
#endif

static struct dispatch_timer_cblist_node *dispatch_timer_cblist_node_new(timer_cb_t cb, void *udata, dispatch_timer_t t)
{
	struct dispatch_timer_cblist_node *n;
	if (t && t->lcache.count) {
		n = t->lcache.nodes[t->lcache.count - 1];
		t->lcache.count -= 1;
		memset(n, 0, sizeof(*n));
	} else {
		n = calloc(1, sizeof(struct dispatch_timer_cblist_node));
		if (!n) {
			neb_syslog(LOG_ERR, "calloc: %m");
			return NULL;
		}
	}

	n->cb = cb;
	n->udata = udata;

	return n;
}

static void dispatch_timer_cblist_node_free(struct dispatch_timer_cblist_node *n, dispatch_timer_t t)
{
	if (t && t->lcache.count < t->lcache.size) {
		t->lcache.nodes[t->lcache.count] = n;
		t->lcache.count += 1;
	} else {
		free(n);
	}
}

static struct dispatch_timer_rbtree_node * dispatch_timer_rbtree_node_new(int64_t abs_msec, dispatch_timer_t t)
{
	struct dispatch_timer_rbtree_node *n;
	if (t && t->tcache.count) {
		n = t->tcache.nodes[t->tcache.count - 1];
		t->tcache.count -= 1;
		memset(n, 0, sizeof(*n));
	} else {
		n = calloc(1, sizeof(struct dispatch_timer_rbtree_node));
		if (!n) {
			neb_syslog(LOG_ERR, "calloc: %m");
			return NULL;
		}
	}

	n->msec = abs_msec;
	LIST_INIT(&n->cblist);

	return n;
}

static void dispatch_timer_rbtree_node_free(struct dispatch_timer_rbtree_node *n, dispatch_timer_t t)
{
	struct dispatch_timer_cblist_node *node, *next;
# if defined(OS_DFLYBSD)
	LIST_FOREACH_MUTABLE(node, &n->cblist, node, next) {
# else
	LIST_FOREACH_SAFE(node, &n->cblist, node, next) {
# endif
		dispatch_timer_cblist_node_free(node, t);
	}
	LIST_INIT(&n->cblist);
	if (t && t->tcache.count < t->tcache.size) {
		t->tcache.nodes[t->tcache.count] = n;
		t->tcache.count += 1;
	} else {
		free(n);
	}
}

#if defined(HAVE_BSD_RBTREE)

static int dispatch_timer_rbtree_cmp_node(void *context __attribute_unused__, const void *node1, const void *node2)
{
	const struct dispatch_timer_rbtree_node *e = node1;
	const struct dispatch_timer_rbtree_node *p = node2;
	if (e->msec < p->msec)
		return -1;
	else if (e->msec == p->msec)
		return 0;
	else
		return 1;
}

static int dispatch_timer_rbtree_cmp_key(void *context __attribute_unused__, const void *node, const void *key)
{
	const struct dispatch_timer_rbtree_node *e = node;
	int64_t msec = *(int64_t *)key;
	if (e->msec < msec)
		return -1;
	else if (e->msec == msec)
		return 0;
	else
		return 1;
}

#elif defined(HAVE_BSD_TREE)

static int dispatch_timer_rbtree_node_cmp(struct dispatch_timer_rbtree_node *e, struct dispatch_timer_rbtree_node *p)
{
	if (e->msec < p->msec)
		return -1;
	else if (e->msec == p->msec)
		return 0;
	else
		return 1;
}

RB_GENERATE_STATIC(dispatch_timer_rbtree, dispatch_timer_rbtree_node, node, dispatch_timer_rbtree_node_cmp)

#else
# error "fix me"
#endif

dispatch_timer_t neb_dispatch_timer_create(int tcache_size, int lcache_size)
{
	struct dispatch_timer *dt = calloc(1, sizeof(struct dispatch_timer));
	if (!dt) {
		neb_syslog(LOG_ERR, "calloc: %m");
		return NULL;
	}

#if defined(HAVE_BSD_RBTREE)
	rb_tree_init(&dt->rbtree, &dispatch_timer_rbtree_ops);
#elif defined(HAVE_BSD_TREE)
	RB_INIT(&dt->rbtree);
#else
# error "fix me"
#endif

	dt->tcache.size = tcache_size;
	dt->tcache.nodes = malloc(tcache_size * sizeof(struct dispatch_timer_rbtree_node *));
	if (!dt->tcache.nodes) {
		neb_syslog(LOG_ERR, "malloc: %m");
		neb_dispatch_timer_destroy(dt);
		return NULL;
	}
	dt->tcache.count = 0;

	dt->lcache.size = lcache_size;
	dt->lcache.nodes = malloc(lcache_size * sizeof(struct dispatch_timer_cblist_node *));
	if (!dt->lcache.nodes) {
		neb_syslog(LOG_ERR, "malloc: %m");
		neb_dispatch_timer_destroy(dt);
		return NULL;
	}
	dt->lcache.count = 0;

	// allocate new intmax node and set ref
	struct dispatch_timer_rbtree_node *max_node = dispatch_timer_rbtree_node_new(INT64_MAX, NULL);
	if (!max_node) {
		neb_syslog(LOG_ERR, "Failed to allocate max node");
		neb_dispatch_timer_destroy(dt);
		return NULL;
	}
#if defined(HAVE_BSD_RBTREE)
	rb_tree_insert_node(&dt->rbtree, max_node);
#elif defined(HAVE_BSD_TREE)
	RB_INSERT(dispatch_timer_rbtree, &dt->rbtree, max_node);
#else
# error "fix me"
#endif
	dt->ref_min_node = max_node;

	return dt;
}

void neb_dispatch_timer_destroy(dispatch_timer_t t)
{
	struct dispatch_timer_rbtree_node *node, *next;
#if defined(HAVE_BSD_RBTREE)
	RB_TREE_FOREACH_SAFE(node, &t->rbtree, next) {
		rb_tree_remove_node(&t->rbtree, node);
#elif defined(HAVE_BSD_TREE)
	RB_FOREACH_SAFE(node, dispatch_timer_rbtree, &t->rbtree, next) {
		RB_REMOVE(dispatch_timer_rbtree, &t->rbtree, node);
#else
# error "fix me"
#endif
		dispatch_timer_rbtree_node_free(node, NULL);
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

void* neb_dispatch_timer_add(dispatch_timer_t t, int64_t abs_msec, timer_cb_t cb, void* udata)
{
	struct dispatch_timer_rbtree_node *tn = dispatch_timer_rbtree_node_new(abs_msec, t);
	if (!tn)
		return NULL;

#if defined(HAVE_BSD_RBTREE)
	struct dispatch_timer_rbtree_node *tmp = rb_tree_insert_node(&t->rbtree, tn);
	if (tmp != tn) { // existed
#elif defined(HAVE_BSD_TREE)
	struct dispatch_timer_rbtree_node *tmp = RB_INSERT(dispatch_timer_rbtree, &t->rbtree, tn);
	if (tmp) { // existed
#else
# error "fix me"
#endif
		dispatch_timer_rbtree_node_free(tn, t);
		tn = tmp;
	} else { // inserted
		// Update ref min node
#if defined(HAVE_BSD_RBTREE)
		if (dispatch_timer_rbtree_cmp_node(NULL, tn, t->ref_min_node) < 0)
#elif defined(HAVE_BSD_TREE)
		if (dispatch_timer_rbtree_node_cmp(tn, t->ref_min_node) < 0)
#else
# error "fix me"
#endif
			t->ref_min_node = tn;
	}

	struct dispatch_timer_cblist_node *ln = dispatch_timer_cblist_node_new(cb, udata, t);
	if (!ln)
		return NULL;
	ln->ref_tnode = tn;

	LIST_INSERT_HEAD(&tn->cblist, ln, node);
	return ln;
}

void neb_dispatch_timer_del(dispatch_timer_t t, void* n)
{
	struct dispatch_timer_cblist_node *ln = n;
	struct dispatch_timer_rbtree_node *tn = ln->ref_tnode;

	if (ln->running) // do not delete ourself in our cb
		return;

	if (tn) // still in list
		LIST_REMOVE(ln, node);
	dispatch_timer_cblist_node_free(ln, t);

	if (tn && LIST_EMPTY(&tn->cblist)) {
		if (t->ref_min_node == tn) // Update ref min node
#if defined(HAVE_BSD_RBTREE)
			t->ref_min_node = rb_tree_iterate(&t->rbtree, tn, RB_DIR_RIGHT);
#elif defined(HAVE_BSD_TREE)
			t->ref_min_node = RB_NEXT(dispatch_timer_rbtree, &t->rbtree, tn);
#else
# error "fix me"
#endif

#if defined(HAVE_BSD_RBTREE)
		rb_tree_remove_node(&t->rbtree, tn);
#elif defined(HAVE_BSD_TREE)
		RB_REMOVE(dispatch_timer_rbtree, &t->rbtree, tn);
#else
# error "fix me"
#endif
		dispatch_timer_rbtree_node_free(tn, t);
	}
}

int dispatch_timer_get_min(dispatch_timer_t t, int64_t cur_msec)
{
	if (t->ref_min_node->msec == INT64_MAX)
		return -1;
	else if (t->ref_min_node->msec <= cur_msec)
		return 0;
	else
		return t->ref_min_node->msec - cur_msec;
}

int dispatch_timer_run_until(dispatch_timer_t t, int64_t abs_msec)
{
	int count = 0;
	for (;;) {
		struct dispatch_timer_rbtree_node *tn = t->ref_min_node;
		if (tn->msec <= abs_msec) {
			struct dispatch_timer_cblist_node *ln, *next;
			for (ln = LIST_FIRST(&tn->cblist); ln; ln = next) {
				ln->running = 1;
				ln->cb(ln->udata);
				ln->running = 0;
				// the cb here may remove following ln and tn,
				// so we need re-get the next node for them
				count += 1;
				next = LIST_NEXT(ln, node);
				LIST_REMOVE(ln, node);
				// do not free this node, user should do it, just clear ref
				ln->ref_tnode = NULL;
			}
		} else {
			break;
		}
#if defined(HAVE_BSD_RBTREE)
		t->ref_min_node = rb_tree_iterate(&t->rbtree, tn, RB_DIR_RIGHT);
		rb_tree_remove_node(&t->rbtree, tn);
#elif defined(HAVE_BSD_TREE)
		t->ref_min_node = RB_NEXT(dispatch_timer_rbtree, &t->rbtree, tn);
		RB_REMOVE(dispatch_timer_rbtree, &t->rbtree, tn);
#else
# error "fix me"
#endif
		dispatch_timer_rbtree_node_free(tn, t);
	}
	return count;
}


#include <nebase/syslog.h>
#include <nebase/evdp.h>
#include <nebase/rbtree.h>
#include <nebase/resolver.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <stdio.h>

#include <ares.h>

struct resolver_source_node {
	rb_node_t rbtree_ctx;
	SLIST_ENTRY(resolver_source_node) list_ctx;

	neb_evdp_source_t s;
	neb_resolver_t ref_r;
	int ref_fd;
};

SLIST_HEAD(resolver_source_list, resolver_source_node);

struct neb_resolver_ctx {
	SLIST_ENTRY(neb_resolver_ctx) list_ctx;

	neb_resolver_t ref_r;
	void *callback;
	void *udata;
	int submitted;
	int delete_after_timeout;
	int after_timeout;
};

SLIST_HEAD(resolver_ctx_list, neb_resolver_ctx);

struct neb_resolver {
	ares_channel channel;

	neb_evdp_queue_t q;
	neb_evdp_timer_point timeout_point;

	rb_tree_t active_tree;
	struct resolver_source_list cache_list;
	struct resolver_source_list detach_list;
	struct resolver_ctx_list ctx_list;
	// TODO add counting

	int critical_error;
};

static void register_evdp_events(neb_resolver_t r) _nattr_nonnull((1));
static int resolver_rbtree_cmp_node(void *context, const void *node1, const void *node2);
static int resolver_rbtree_cmp_key(void *context, const void *node, const void *key);
rb_tree_ops_t resolver_rbtree_ops = {
	.rbto_compare_nodes = resolver_rbtree_cmp_node,
	.rbto_compare_key = resolver_rbtree_cmp_key,
	.rbto_node_offset = offsetof(struct resolver_source_node, rbtree_ctx),
};

static struct neb_resolver_ctx *resolver_ctx_node_new(void)
{
	struct neb_resolver_ctx *n = calloc(1, sizeof(struct neb_resolver_ctx));
	if (!n) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	return n;
}

static void resolver_ctx_node_del(struct neb_resolver_ctx *n)
{
	free(n);
}

static struct resolver_source_node *resolver_source_node_new(void)
{
	struct resolver_source_node *n = calloc(1, sizeof(struct resolver_source_node));
	if (!n) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	return n;
}

static void resolver_source_node_del(struct resolver_source_node *n)
{
	if (n->s) {
		neb_evdp_queue_t q = neb_evdp_source_get_queue(n->s);
		if (q)
			neb_syslog(LOG_CRIT, "resolver: you should detach source from queue first");
		neb_evdp_source_del(n->s);
	}
	free(n);
}

static int resolver_rbtree_cmp_node(void *context _nattr_unused, const void *node1, const void *node2)
{
	const struct resolver_source_node *e = node1;
	const struct resolver_source_node *p = node2;
	if (e->ref_fd < p->ref_fd)
		return -1;
	else if (e->ref_fd == p->ref_fd)
		return 0;
	else
		return 1;
}

static int resolver_rbtree_cmp_key(void *context _nattr_unused, const void *node, const void *key)
{
	const struct resolver_source_node *e = node;
	int fd = *(int *)key;
	if (e->ref_fd < fd)
		return -1;
	else if (e->ref_fd == fd)
		return 0;
	else
		return 1;
}

static neb_evdp_cb_ret_t flush_detach_list(neb_resolver_t r, int fd)
{
	neb_evdp_cb_ret_t ret = NEB_EVDP_CB_CONTINUE;
	struct resolver_source_node *n;
	for (n = SLIST_FIRST(&r->detach_list); n; n = SLIST_FIRST(&r->detach_list)) {
		SLIST_REMOVE_HEAD(&r->detach_list, list_ctx);
		SLIST_INSERT_HEAD(&r->cache_list, n, list_ctx);
		neb_evdp_queue_t q = neb_evdp_source_get_queue(n->s);
		if (q) {
			if (n->ref_fd == fd) {
				ret = NEB_EVDP_CB_CLOSE;
			} else if (neb_evdp_queue_detach(q, n->s, 1) != 0) {
				neb_syslog(LOG_CRIT, "Failed to detach source %p from queue %p", n->s, q);
				r->critical_error = 1;
				return NEB_EVDP_CB_BREAK_ERR;
			}
		}
	}
	return ret;
}

static neb_evdp_cb_ret_t on_socket_hup(int fd, void *data _nattr_unused, const void *context)
{
	// the socket error is not used in ares
	int sockerr;
	if (neb_evdp_source_fd_get_sockerr(context, &sockerr) != 0) {
		neb_syslog(LOG_CRIT, "Failed to get sockerr for hupped resolver socket %d", fd);
		return NEB_EVDP_CB_BREAK_ERR;
	}
	if (sockerr != 0)
		neb_syslog_en(sockerr, LOG_ERR, "resolver socket fd %d: %m", fd);
	return NEB_EVDP_CB_CONTINUE;
}

static neb_evdp_cb_ret_t on_socket_readable(int fd, void *data, const void *context _nattr_unused)
{
	struct resolver_source_node *n = data;
	ares_process_fd(n->ref_r->channel, fd, ARES_SOCKET_BAD);
	register_evdp_events(n->ref_r);
	if (n->ref_r->critical_error)
		return NEB_EVDP_CB_BREAK_ERR;
	return flush_detach_list(n->ref_r, fd);
}

static neb_evdp_cb_ret_t on_socket_writable(int fd, void *data, const void *context _nattr_unused)
{
	struct resolver_source_node *n = data;
	ares_process_fd(n->ref_r->channel, ARES_SOCKET_BAD, fd);
	register_evdp_events(n->ref_r);
	if (n->ref_r->critical_error)
		return NEB_EVDP_CB_BREAK_ERR;
	return flush_detach_list(n->ref_r, fd);
}

static struct resolver_source_node *fetch_or_insert_source_node(neb_resolver_t r, int fd)
{
	struct resolver_source_node *n = SLIST_FIRST(&r->cache_list);
	if (!n) {
		n = rb_tree_find_node(&r->active_tree, &fd);
		if (!n) { // not found, create a new one and insert
			n = resolver_source_node_new();
			if (!n) {
				neb_syslog(LOG_ERR, "Failed to get new resolver source node");
				return NULL;
			}
			n->s = neb_evdp_source_new_os_fd(fd, on_socket_hup);
			neb_evdp_source_set_udata(n->s, n);
			if (neb_evdp_queue_attach(r->q, n->s) != 0) {
				neb_syslog(LOG_ERR, "Failed to attach resolver source to queue");
				resolver_source_node_del(n);
				return NULL;
			}
			n->ref_fd = fd;
			n->ref_r = r;
			rb_tree_insert_node(&r->active_tree, n);
		}
	} else {
		n->ref_fd = fd;
		struct resolver_source_node *tn = rb_tree_insert_node(&r->active_tree, n);
		if (tn == n) { // inserted the cached one, reset it
			SLIST_REMOVE_HEAD(&r->cache_list, list_ctx);
			if (neb_evdp_source_os_fd_reset(n->s, fd) != 0) {
				neb_syslog(LOG_ERR, "Failed to reset resolver source to fd %d", fd);
				return NULL;
			}
		} else { // if existed, just use the old one
			n = tn;
		}
	}

	return n;
}

static int resolver_reset_timeout(neb_resolver_t r, struct timeval *maxtv)
{
	int64_t abs_timeout = INT64_MAX;
	struct timeval tv;
	struct timeval *vp = ares_timeout(r->channel, maxtv, &tv);
	if (vp) {
		time_t timeout_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
		abs_timeout = neb_evdp_queue_get_abs_timeout(r->q, timeout_ms);
	}
	neb_evdp_timer_t t = neb_evdp_queue_get_timer(r->q);
	if (neb_evdp_timer_point_reset(t, r->timeout_point, abs_timeout) != 0) {
		neb_syslog(LOG_CRIT, "Failed to reset resolver timeout point");
		return -1;
	}
	return 0;
}

static void register_evdp_events(neb_resolver_t r)
{
	ares_socket_t sockets[ARES_GETSOCK_MAXNUM];
	int bits = ares_getsock(r->channel, sockets, ARES_GETSOCK_MAXNUM);
	for (int i = 0; i < ARES_GETSOCK_MAXNUM; i++) {
		int fd = sockets[i];
		if (ARES_GETSOCK_READABLE(bits, i)) {
			struct resolver_source_node *n = fetch_or_insert_source_node(r, fd);
			if (!n) {
				r->critical_error = 1;
				return;
			}
			if (neb_evdp_source_os_fd_next_read(n->s, on_socket_readable) != 0) {
				neb_syslog(LOG_CRIT, "Failed to enable next read on fd %d", fd);
				r->critical_error = 1;
			}
		} else if (ARES_GETSOCK_WRITABLE(bits, i)) {
			struct resolver_source_node *n = fetch_or_insert_source_node(r, fd);
			if (!n) {
				r->critical_error = 1;
				return;
			}
			if (neb_evdp_source_os_fd_next_write(n->s, on_socket_writable) != 0) {
				neb_syslog(LOG_CRIT, "Failed to enable next write on fd %d", fd);
				r->critical_error = 1;
			}
		}
	}

	if (resolver_reset_timeout(r, NULL) != 0) {
		r->critical_error = 1;
		return;
	}
}

static void resolver_sock_state_on_change(void *data, ares_socket_t socket_fd, int readable, int writable)
{
	neb_resolver_t r = data;
	if (readable) {
		return;
	} else if (writable) {
		return;
	} else { // it's close
		struct resolver_source_node *n = rb_tree_find_node(&r->active_tree, &socket_fd);
		if (!n) {
			neb_syslog(LOG_CRIT, "No socket %d found in resolver %p", socket_fd, r);
			r->critical_error = 1;
			return;
		}
		rb_tree_remove_node(&r->active_tree, n);
		SLIST_INSERT_HEAD(&r->detach_list, n, list_ctx);
	}
}

neb_resolver_t neb_resolver_create(struct ares_options *options, int optmask)
{
	neb_resolver_t r = calloc(1, sizeof(struct neb_resolver));
	if (!r) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}

	SLIST_INIT(&r->ctx_list);
	SLIST_INIT(&r->cache_list);
	SLIST_INIT(&r->detach_list);
	rb_tree_init(&r->active_tree, &resolver_rbtree_ops);

	options->sock_state_cb = resolver_sock_state_on_change;
	options->sock_state_cb_data = r;
	optmask |= ARES_OPT_SOCK_STATE_CB;

	int ret = ares_init_options(&r->channel, options, optmask);
	if (ret != ARES_SUCCESS) {
		neb_syslog(LOG_ERR, "ares_init_options: %s", ares_strerror(ret));
		free(r);
		return NULL;
	}

	return r;
}

void neb_resolver_destroy(neb_resolver_t r)
{
	struct resolver_source_node *n, *t;
	RB_TREE_FOREACH_SAFE(n, &r->active_tree, t) {
		rb_tree_remove_node(&r->active_tree, n);
		SLIST_INSERT_HEAD(&r->detach_list, n, list_ctx);
	}

	for (n = SLIST_FIRST(&r->detach_list); n; n = SLIST_FIRST(&r->detach_list)) {
		SLIST_REMOVE_HEAD(&r->detach_list, list_ctx);
		neb_evdp_queue_t q = neb_evdp_source_get_queue(n->s);
		if (q) {
			if (neb_evdp_queue_detach(q, n->s, 1) != 0)
				neb_syslog(LOG_CRIT, "Failed to detach source %p from queue %p", n->s, q);
		}
		SLIST_INSERT_HEAD(&r->cache_list, n, list_ctx);
	}

	for (n = SLIST_FIRST(&r->cache_list); n; n = SLIST_FIRST(&r->cache_list)) {
		SLIST_REMOVE_HEAD(&r->cache_list, list_ctx);
		resolver_source_node_del(n);
	}

	for (struct neb_resolver_ctx *c = SLIST_FIRST(&r->ctx_list); c; c = SLIST_FIRST(&r->ctx_list)) {
		SLIST_REMOVE_HEAD(&r->ctx_list, list_ctx);
		resolver_ctx_node_del(c);
	}

	ares_destroy(r->channel);
	free(r);
}

int neb_resolver_set_bind_ip(neb_resolver_t r, const struct sockaddr *addr)
{
	switch (addr->sa_family) {
	case AF_INET:
		ares_set_local_ip4(r->channel, ((const struct sockaddr_in *)addr)->sin_addr.s_addr);
		break;
	case AF_INET6:
		ares_set_local_ip6(r->channel, ((const struct sockaddr_in6 *)addr)->sin6_addr.s6_addr);
		break;
	default:
		break;
	}
	return 0;
}

static neb_evdp_timeout_ret_t reolver_on_timeout(void *data)
{
	neb_resolver_t r = data;
	ares_process_fd(r->channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
	register_evdp_events(r);
	flush_detach_list(r, ARES_SOCKET_BAD);
	return NEB_EVDP_TIMEOUT_KEEP;
}

int neb_resolver_associate(neb_resolver_t r, neb_evdp_queue_t q)
{
	if (r->critical_error) {
		neb_syslog(LOG_ERR, "critical error found for current resolver %p", r);
		return -1;
	}
	neb_evdp_timer_t t = neb_evdp_queue_get_timer(q);
	if (!t) {
		neb_syslog(LOG_CRIT, "There is no timer set in queue %p", q);
		return -1;
	}
	r->timeout_point = neb_evdp_timer_new_point(t, INT64_MAX, reolver_on_timeout, r);
	if (!r->timeout_point) {
		neb_syslog(LOG_ERR, "Failed to get timer point");
		return -1;
	}
	r->q = q;
	return 0;
}

void neb_resolver_disassociate(neb_resolver_t r)
{
	ares_cancel(r->channel);
	if (r->timeout_point) {
		neb_evdp_timer_t t = neb_evdp_queue_get_timer(r->q);
		if (t) {
			neb_evdp_timer_del_point(t, r->timeout_point);
			r->timeout_point = NULL;
		} else {
			// no timer available, the point would be freed when destroy the timer
			neb_syslog(LOG_DEBUG, "No timer available while deleting timer point");
		}
	}
}

int neb_resolver_set_servers(neb_resolver_t r, struct ares_addr_port_node *servers)
{
	int ret = ares_set_servers_ports(r->channel, servers);
	if (ret != ARES_SUCCESS) {
		neb_syslog(LOG_ERR, "ares_set_servers_ports: %s", ares_strerror(ret));
		return -1;
	}
	return 0;
}

neb_resolver_ctx_t neb_resolver_new_ctx(neb_resolver_t r, void *udata)
{
	struct neb_resolver_ctx *c = SLIST_FIRST(&r->ctx_list);
	if (!c) {
		c = resolver_ctx_node_new();
		if (!c)
			return NULL;

		c->ref_r = r;
	} else {
		SLIST_REMOVE_HEAD(&r->ctx_list, list_ctx);
		c->delete_after_timeout = 0;
		c->submitted = 0;
	}
	c->udata = udata;

	return c;
}

void neb_resolver_del_ctx(neb_resolver_t r, neb_resolver_ctx_t c)
{
	c->udata = NULL;
	if (!c->submitted || c->after_timeout)
		SLIST_INSERT_HEAD(&r->ctx_list, c, list_ctx);
	else
		c->delete_after_timeout = 1;
}

bool neb_resolver_ctx_in_use(neb_resolver_ctx_t c)
{
	return c->callback != NULL;
}

static void gethostbyname_callback(void *arg, int status, int timeouts, struct hostent *hostent)
{
	neb_resolver_ctx_t c = arg;
	ares_host_callback cb = c->callback;
	c->after_timeout = 1;
	c->callback = NULL;

	if (c->delete_after_timeout)
		neb_resolver_del_ctx(c->ref_r, c);
	else
		cb(c->udata, status, timeouts, hostent);
}

int neb_resolver_ctx_gethostbyname(neb_resolver_ctx_t c, const char *name, int family, ares_host_callback cb)
{
	if (c->callback) {
		neb_syslog(LOG_ERR, "resolver ctx %p is already in use", c);
		return -1;
	}
	c->callback = cb;
	ares_gethostbyname(c->ref_r->channel, name, family, gethostbyname_callback, c);
	register_evdp_events(c->ref_r);
	if (c->ref_r->critical_error) {
		neb_syslog(LOG_ERR, "failed to reset resolver timeout");
		return -1;
	}
	return 0;
}

static void gethostbyaddr_callback(void *arg, int status, int timeouts, struct hostent *hostent)
{
	neb_resolver_ctx_t c = arg;
	ares_host_callback cb = c->callback;
	c->after_timeout = 1;
	c->callback = NULL;

	if (c->delete_after_timeout)
		neb_resolver_del_ctx(c->ref_r, c);
	else
		cb(c->udata, status, timeouts, hostent);
}

int neb_resolver_ctx_gethostbyaddr(neb_resolver_ctx_t c, const struct sockaddr *addr, ares_host_callback cb)
{
	if (c->callback) {
		neb_syslog(LOG_ERR, "resolver ctx %p is already in use", c);
		return -1;
	}
	c->callback = cb;
	switch (addr->sa_family) {
	case AF_INET:
		ares_gethostbyaddr(c->ref_r->channel, &((struct sockaddr_in *)addr)->sin_addr, sizeof(struct in_addr), AF_INET, gethostbyaddr_callback, c);
		break;
	case AF_INET6:
		ares_gethostbyaddr(c->ref_r->channel, &((struct sockaddr_in6 *)addr)->sin6_addr, sizeof(struct in6_addr), AF_INET6, gethostbyaddr_callback, c);
		break;
	default:
		neb_syslog(LOG_ERR, "Unsupported socket family %u", addr->sa_family);
		break;
	}
	c->submitted = 1;
	register_evdp_events(c->ref_r);
	if (c->ref_r->critical_error) {
		neb_syslog(LOG_ERR, "failed to reset resolver timeout");
		return -1;
	}
	return 0;
}

static void send_callback(void *arg, int status, int timeouts, unsigned char *abuf, int alen)
{
	neb_resolver_ctx_t c = arg;
	ares_callback cb = c->callback;
	c->after_timeout = 1;
	c->callback = NULL;

	if (c->delete_after_timeout)
		neb_resolver_del_ctx(c->ref_r, c);
	else
		cb(c->udata, status, timeouts, abuf, alen);
}

int neb_resolver_ctx_send(neb_resolver_ctx_t c, const unsigned char *qbuf, int qlen, ares_callback cb)
{
	if (c->callback) {
		neb_syslog(LOG_ERR, "resolver ctx %p is already in use", c);
		return -1;
	}
	c->callback = cb;
	ares_send(c->ref_r->channel, qbuf, qlen, send_callback, c);
	c->submitted = 1;
	register_evdp_events(c->ref_r);
	if (c->ref_r->critical_error) {
		neb_syslog(LOG_ERR, "failed to reset resolver timeout");
		return -1;
	}
	return 0;
}

static int ares_addr_port_node_set_port(struct ares_addr_port_node *n, const char *p)
{
	char *end;
	long port = strtol(p, &end, 10);
	if (end != NULL && *end != '\0')
		return -1;
	if (port <= 0 || port > UINT16_MAX)
		return -1;
	n->tcp_port = port;
	n->udp_port = port;
	return 0;
}

// max len: [<ipv6 addr>]:<port> + a trailing null byte
#define _MAX_SERVER_STR_LEN (INET6_ADDRSTRLEN + 2 + 1 + 5 + 1)

struct ares_addr_port_node *neb_resolver_new_server(const char *s)
{
	char buf[_MAX_SERVER_STR_LEN+1];
	strncpy(buf, s, _MAX_SERVER_STR_LEN);
	buf[_MAX_SERVER_STR_LEN] = '\0';
	char *server = buf;
	struct ares_addr_port_node *n = calloc(1, sizeof(struct ares_addr_port_node));
	if (!n) {
		neb_syslogl(LOG_ERR, "calloc: %m");
		return NULL;
	}
	n->tcp_port = NS_DEFAULTPORT;
	n->udp_port = NS_DEFAULTPORT;

	if (server[0] == '[') { // [<ipv6 addr>]:<port>
		server += 1;
		char *end = strchr(server, ']');
		if (!end)
			goto errfmt;
		*end = '\0';
		char *d = end + 1;
		if (*d != ':')
			goto errfmt;
		char *p = d + 1;

		if (ares_addr_port_node_set_port(n, p) != 0)
			goto errfmt;

		if (inet_pton(AF_INET6, server, &n->addr.addr6) != 1)
			goto errfmt;
		n->family = AF_INET6;
	} else if (inet_pton(AF_INET6, server, &n->addr.addr6) == 1) { // <ipv6 addr>
		n->family = AF_INET6;
	} else if (inet_pton(AF_INET, server, &n->addr.addr4) == 1) { // <ipv4 addr>
		n->family = AF_INET;
	} else { // <ipv4 addr>:<port>
		char *d = strchr(server, ':');
		if (!d)
			goto errfmt;
		*d = '\0';
		char *p = d + 1;

		if (ares_addr_port_node_set_port(n, p) != 0)
			goto errfmt;

		if (inet_pton(AF_INET, server, &n->addr.addr4) != 1)
			goto errfmt;
		n->family = AF_INET;
	}

exit:
	return n;

errfmt:
	neb_resolver_del_server(n);
	n = NULL;
	goto exit;
}

void neb_resolver_del_server(struct ares_addr_port_node *n)
{
	free(n);
}

int neb_resolver_parse_type(const char *type, int len)
{
	if (!len)
		len = strlen(type);
	switch (type[0]) {
	case 'a':
	case 'A':
		if (len == 1 && strncasecmp(type, "a", 1) == 0)
			return ns_t_a;
		else if (len == 4 && strncasecmp(type, "aaaa", 4) == 0)
			return ns_t_aaaa;
		break;
	case 'c':
	case 'C':
		if (len == 5 && strncasecmp(type, "cname", 5) == 0)
			return ns_t_cname;
		break;
	case 'm':
	case 'M':
		if (len == 2 && strncasecmp(type, "mx", 2) == 0)
			return ns_t_mx;
		break;
	case 'n':
	case 'N':
		if (len == 5 && strncasecmp(type, "naptr", 5) == 0)
			return ns_t_naptr;
		else if (len == 2 && strncasecmp(type, "ns", 2) == 0)
			return ns_t_ns;
		break;
	case 'p':
	case 'P':
		if (len == 3 && strncasecmp(type, "ptr", 3) == 0)
			return ns_t_ptr;
		break;
	case 's':
	case 'S':
		if (len == 3 && strncasecmp(type, "soa", 3) == 0)
			return ns_t_soa;
		else if (len == 3 && strncasecmp(type, "srv", 3) == 0)
			return ns_t_srv;
		break;
	case 't':
	case 'T':
		if (len == 3 && strncasecmp(type, "txt", 3) == 0)
			return ns_t_txt;
		break;
	default:
		break;
	}
	return ns_t_invalid;
}

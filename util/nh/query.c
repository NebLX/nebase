
#include <nebase/events.h>
#include <nebase/random.h>
#include <nebase/netinet.h>

#include "query.h"

#include <sys/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

struct query_data {
	STAILQ_ENTRY(query_data) stailq_ctx;

	neb_resolver_ctx_t resolver_ctx;

	int query_id;
	int type;
	int family;
	char *name;
	union {
		struct in_addr v4;
		struct in6_addr v6;
	} addr;
};

STAILQ_HEAD(query_list, query_data);

static struct query_list queries = STAILQ_HEAD_INITIALIZER(queries);
static struct query_data *next_query = NULL;

static void query_data_cancel(struct query_data *q)
{
	if (q->resolver_ctx) {
		neb_resolver_del_ctx(resolver, q->resolver_ctx);
		q->resolver_ctx = NULL;
	}
}

static void query_data_del(struct query_data *q)
{
	query_data_cancel(q);

	STAILQ_REMOVE(&queries, q, query_data, stailq_ctx);

	if (q->name) {
		free(q->name);
		q->name = NULL;
	}

	free(q);

	if (STAILQ_EMPTY(&queries))
		thread_events |= T_E_QUIT;
}

void query_data_foreach_cancel(void)
{
	struct query_data *q;
	STAILQ_FOREACH(q, &queries, stailq_ctx)
		query_data_cancel(q);
}

void query_data_foreach_del()
{
	struct query_data *q, *t;
	STAILQ_FOREACH_SAFE(q, &queries, stailq_ctx, t)
		query_data_del(q);
}

static struct query_data *query_data_new_a(const char *name, int len)
{
	struct query_data *q = calloc(1, sizeof(struct query_data));
	if (!q) {
		perror("calloc");
		return NULL;
	}

	if (len)
		q->name = strndup(name, len);
	else
		q->name = strdup(name);
	if (!q->name) {
		perror("strdup");
		free(q);
		return NULL;
	}
	q->type = ns_t_a;
	q->family = AF_INET;

	return q;
}

static struct query_data *query_data_new_aaaa(const char *name, int len)
{
	struct query_data *q = calloc(1, sizeof(struct query_data));
	if (!q) {
		perror("calloc");
		return NULL;
	}

	if (len)
		q->name = strndup(name, len);
	else
		q->name = strdup(name);
	if (!q->name) {
		perror("strdup");
		free(q);
		return NULL;
	}
	q->type = ns_t_aaaa;
	q->family = AF_INET6;

	return q;
}

static struct query_data *query_data_new_ptr(const char *addr, int len)
{
	if (len == 0)
		len = strlen(addr);
	if (len > INET6_ADDRSTRLEN)
		return NULL;

	struct query_data *q = calloc(1, sizeof(struct query_data));
	if (!q) {
		perror("calloc");
		return NULL;
	}

	char buf[len+1];
	memcpy(buf, addr, len);
	buf[len] = '\0';

	if (strchr(buf, ':')) {
		if (inet_pton(AF_INET6, buf, &q->addr.v6) != 1) {
			perror("inet_pton");
			free(q);
			return NULL;
		}
		q->family = AF_INET6;
		q->name = malloc(NEB_INET6_ARPASTRLEN);
		if (!q->name) {
			perror("malloc");
			free(q);
			return NULL;
		}
	} else {
		if (inet_pton(AF_INET, buf, &q->addr.v4) != 1) {
			perror("inet_pton");
			free(q);
			return NULL;
		}
		q->family = AF_INET;
		q->name = malloc(NEB_INET_ARPASTRLEN);
		if (!q->name) {
			perror("malloc");
			free(q);
			return NULL;
		}
	}
	neb_netinet_addr_to_arpa(q->family, (const unsigned char *)&q->addr, q->name);

	q->type = ns_t_ptr;

	return q;
}

static struct query_data *query_data_new_send(const char *name, int len, int type)
{
	struct query_data *q = calloc(1, sizeof(struct query_data));
	if (!q) {
		perror("calloc");
		return NULL;
	}

	if (len)
		q->name = strndup(name, len);
	else
		q->name = strdup(name);
	if (!q->name) {
		perror("strdup");
		free(q);
		return NULL;
	}
	q->type = type;
	q->query_id = neb_random_uniform(UINT16_MAX+1);

	return q;
}

int query_data_insert(const char *arg, int namelen, int type)
{
	const char *name = arg;
	struct query_data *q = NULL;
	switch (type) {
	case ns_t_a:
		q = query_data_new_a(name, namelen);
		break;
	case ns_t_aaaa:
		q = query_data_new_aaaa(name, namelen);
		break;
	case ns_t_ptr:
		q = query_data_new_ptr(name, namelen);
		break;
	case ns_t_cname:
	case ns_t_ns:
	case ns_t_mx:
		q = query_data_new_send(name, namelen, type);
		break;
	default:
		fprintf(stderr, "unsupported query type for %s, skipped\n", arg);
		return 0;
		break;
	}

	if (q) {
		STAILQ_INSERT_TAIL(&queries, q, stailq_ctx);
		return 0;
	} else {
		return -1;
	}
}

int query_data_init_submit()
{
	if (STAILQ_EMPTY(&queries)) {
		fprintf(stderr, "no host / address given\n");
		return -1;
	}
	next_query = STAILQ_FIRST(&queries);
	return 0;
}

static void parse_cname_abuf(unsigned char *abuf, int alen)
{
	struct hostent *h;
	int ret = ares_parse_a_reply(abuf, alen, &h, NULL, NULL);
	switch (ret) {
	case ARES_SUCCESS:
		break;
	case ARES_ENODATA:
		return;
		break;
	default:
		fprintf(stderr, "ares_parse_a_reply: %s\n", ares_strerror(ret));
		return;
		break;
	}

	if (h) {
		for (char **aliasp = h->h_aliases; *aliasp; aliasp++) {
			char *real = *(aliasp+1);
			if (!real)
				real = h->h_name;
			fprintf(stdout, "%s is an alias for %s\n", *aliasp, real);
		}
		ares_free_hostent(h);
	}
}

static void parse_ns_abuf(unsigned char *abuf, int alen)
{
	struct hostent *h;
	int ret = ares_parse_ns_reply(abuf, alen, &h);
	switch (ret) {
	case ARES_SUCCESS:
		break;
	case ARES_ENODATA:
		return;
		break;
	default:
		fprintf(stderr, "ares_parse_ns_reply: %s\n", ares_strerror(ret));
		return;
		break;
	}

	if (h) {
		for (char **aliasp = h->h_aliases; *aliasp; aliasp++)
			fprintf(stdout, "%s name server %s\n", h->h_name, *aliasp);
		ares_free_hostent(h);
	}
}

static void parse_mx_abuf(struct query_data *q, unsigned char *abuf, int alen)
{
	struct ares_mx_reply *mr;
	int ret = ares_parse_mx_reply(abuf, alen, &mr);
	switch (ret) {
	case ARES_SUCCESS:
		break;
	case ARES_ENODATA:
		return;
		break;
	default:
		fprintf(stderr, "ares_parse_mx_reply: %s\n", ares_strerror(ret));
		return;
		break;
	}

	if (mr) {
		for (struct ares_mx_reply *m = mr; m; m = m->next)
			fprintf(stdout, "%s mail is handled by %d %s\n", q->name, m->priority, m->host);
		ares_free_data(mr);
	}
}

static void handle_send_query(void *arg, int status, int timeouts, unsigned char *abuf, int alen)
{
	struct query_data *q = arg;
	fprintf(stdout, "==> %s, status: %d, timeouts: %d\n", q->name, status, timeouts);

	switch (q->type) {
	case ns_t_cname:
		parse_cname_abuf(abuf, alen);
		break;
	case ns_t_ns:
		parse_ns_abuf(abuf, alen);
		break;
	case ns_t_mx:
		parse_mx_abuf(q, abuf, alen);
		break;
	default:
		break;
	}

	query_data_del(q);
}

static void handle_byaddr_query(void *arg, int status, int timeouts, struct hostent *h)
{
	struct query_data *q = arg;

	switch (q->family) {
	case AF_INET:
	{
		char addr[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &q->addr, addr, INET_ADDRSTRLEN);

		fprintf(stdout, "==> %s, status: %d, timeouts: %d\n", addr, status, timeouts);
	}
		break;
	case AF_INET6:
	{
		char addr[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &q->addr, addr, INET6_ADDRSTRLEN);

		fprintf(stdout, "==> %s, status: %d, timeouts: %d\n", addr, status, timeouts);
	}
		break;
	default:
		break;
	}
	if (h) {
		for (char **aliasp = h->h_aliases; *aliasp; aliasp++)
			fprintf(stdout, "%s domain name pointer %s\n", q->name, *aliasp);
	}

	query_data_del(q);
}

static void handle_byname_query(void *arg, int status, int timeouts, struct hostent *h)
{
	struct query_data *q = arg;
	fprintf(stdout, "==> %s, status: %d, timeouts: %d\n", q->name, status, timeouts);

	if (h) {
		for (char **aliasp = h->h_aliases; *aliasp; aliasp++) {
			char *real = *(aliasp+1);
			if (!real)
				real = h->h_name;
			fprintf(stdout, "%s is an alias for %s\n", *aliasp, real);
		}
		switch (h->h_addrtype) {
		case AF_INET:
			for (char **addrp = h->h_addr_list; *addrp; addrp++) {
				char addr[INET_ADDRSTRLEN+1];
				if (inet_ntop(AF_INET, *addrp, addr, sizeof(addr)) == NULL) {
					perror("inet_ntop");
					continue;
				}
				fprintf(stdout, "%s has address %s\n", h->h_name, addr);
			}
			break;
		case AF_INET6:
			for (char **addrp = h->h_addr_list; *addrp; addrp++) {
				char addr[INET6_ADDRSTRLEN+1];
				if (inet_ntop(AF_INET6, *addrp, addr, sizeof(addr)) == NULL) {
					perror("inet_ntop");
					continue;
				}
				fprintf(stdout, "%s has IPv6 address %s\n", h->h_name, addr);
			}
			break;
		default:
			break;
		}
	}

	query_data_del(q);
}

int query_data_submit(struct query_data *q)
{
	struct sockaddr_storage ss;
	struct sockaddr *addr = (struct sockaddr *)&ss;
	q->resolver_ctx = neb_resolver_new_ctx(resolver, q);
	if (!q->resolver_ctx) {
		fprintf(stderr, "failed to get new resolver ctx\n");
		return -1;
	}
	unsigned char *qbuf = NULL;
	int qlen = 0, qres;
	switch (q->type) {
	case ns_t_a:
		goto do_gethostbyname;
		break;
	case ns_t_aaaa:
		goto do_gethostbyname;
		break;
	case ns_t_ptr:
		goto do_gethostbyaddr;
		break;
	case ns_t_cname:
	case ns_t_ns:
	case ns_t_mx:                                                     // TODO recursive
		qres = ares_create_query(q->name, ns_c_in, q->type, q->query_id, 1, &qbuf, &qlen, 0);
		if (qres != ARES_SUCCESS) {
			fprintf(stderr, "ares_create_query: %s\n", ares_strerror(qres));
			return -1;
		}
		goto do_send;
		break;
	default:
		break;
	}

	return 0;

do_gethostbyname:
	if (neb_resolver_ctx_gethostbyname(q->resolver_ctx, q->name, q->family, handle_byname_query) != 0) {
		fprintf(stderr, "failed to do gethostbyname\n");
		return -1;
	}
	return 0;

do_gethostbyaddr:
	ss.ss_family = q->family;
	switch (ss.ss_family) {
	case AF_INET:
		memcpy(&((struct sockaddr_in *)addr)->sin_addr, &q->addr.v4, sizeof(struct in_addr));
		break;
	case AF_INET6:
		memcpy(&((struct sockaddr_in6 *)addr)->sin6_addr, &q->addr.v6, sizeof(struct in6_addr));
		break;
	default:
		fprintf(stderr, "unsupported address family %d for ptr resolve\n", ss.ss_family);
		return -1;
	}
	if (neb_resolver_ctx_gethostbyaddr(q->resolver_ctx, addr, handle_byaddr_query) != 0) {
		fprintf(stderr, "failed to do gethostbyaddr\n");
		return -1;
	}
	return 0;

do_send:
	if (neb_resolver_ctx_send(q->resolver_ctx, qbuf, qlen, handle_send_query) != 0) {
		fprintf(stderr, "failed to do send\n");
		ares_free_string(qbuf);
		return -1;
	}
	ares_free_string(qbuf);
	return 0;
}

int query_data_foreach_submit(int size)
{
	int count = 0;
	struct query_data *q = next_query;
	for (; q && count < size; q = STAILQ_NEXT(q, stailq_ctx)) {
		count++;
		if (query_data_submit(q) != 0)
			return -1;
	}

	next_query = q;
	return 0;
}

bool query_data_foreach_submit_done(void)
{
	return next_query == NULL;
}

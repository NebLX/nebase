
#include <nebase/cdefs.h>
#include <nebase/evdp.h>
#include <nebase/events.h>
#include <nebase/resolver.h>
#include <nebase/netinet.h>

#include "query.h"

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <ares.h>

neb_resolver_t resolver = NULL;

static neb_evdp_queue_t evdp_queue = NULL;
static neb_evdp_timer_t evdp_timer = NULL;

static neb_evdp_timer_point submit_tp = NULL;
static int submit_batch_size = 10;
static int submit_interval = 100;

static struct ares_options resolver_opts = {};
static int resolver_optmask = 0;
static struct sockaddr_storage resolver_ss = {};
static struct sockaddr *resolver_bind_addr = (struct sockaddr *)&resolver_ss;
static socklen_t resolver_bind_addrlen = 0;
static struct ares_addr_port_node *resolver_servers = NULL;

static int default_resolver_type = ns_t_a;

static void set_quit(void)
{
	thread_events |= T_E_QUIT;
}

static neb_evdp_timeout_ret_t submit_query(void *data _nattr_unused)
{
	if (query_data_foreach_submit(submit_batch_size) != 0) {
		set_quit();
		return NEB_EVDP_TIMEOUT_KEEP;
	}

	if (!query_data_foreach_submit_done()) {
		if (neb_evdp_timer_point_reset(evdp_timer, submit_tp, neb_evdp_queue_get_abs_timeout(evdp_queue, submit_interval)) != 0)
			fprintf(stderr, "failed to reset timer point for next submit");
	}

	return NEB_EVDP_TIMEOUT_KEEP;
}

static void deinit_args(void)
{
	query_data_foreach_del();

	struct ares_addr_port_node *t;
	for (struct ares_addr_port_node *n = resolver_servers; n; n = t) {
		t = n->next;
		neb_resolver_del_server(n);
	}
}

struct option opts[] = {
	{"bind", required_argument, NULL, 'b'},
	{NULL, no_argument, NULL, 0},
};

static int insert_server(const char *server)
{
	struct ares_addr_port_node *n = neb_resolver_new_server(server);
	if (!n)
		return -1;

	n->next = resolver_servers;
	resolver_servers = n;

	return 0;
}

static int parse_query_data(const char *name, int type)
{
	if (strchr(name, '/') != NULL) {
		if (strchr(name, ':') != NULL) {
			fprintf(stderr, "IPv6 network query is not supported, as it contains too much addresses\n");
			return -1;
		}

		struct sockaddr_in addr = {.sin_family = AF_INET};
		if (neb_netinet_net_pton(name, (struct sockaddr *)&addr) != 0) {
			fprintf(stderr, "Invalid IPv4 network address: %s\n", name);
			return -1;
		}

		int prefix = addr.sin_port;
		if (prefix < 16) {
			fprintf(stderr, "IPv4 network with prefix less than 16 is not supported\n");
			return -1;
		}

		if (type == ns_t_invalid)
			type = ns_t_ptr;

		int count = ((uint32_t)1 << (32 - prefix)) - 1;
		for (int i = 0; i < count; i++) {
			neb_netinet_addr_next((struct sockaddr *)&addr);
			char buf[INET_ADDRSTRLEN] = {};
			inet_ntop(AF_INET, &addr.sin_addr.s_addr, buf, sizeof(buf));
			if (query_data_insert(buf, 0, type) != 0)
				return -1;
		}

		return 0;
	}

	if (type == ns_t_invalid)
		type = default_resolver_type;
	return query_data_insert(name, 0, type);
}

static int parse_query_data_arg(const char *arg)
{
	int type = ns_t_invalid;
	char *saveptr;

	char *s = strdup(arg);
	if (!s) {
		perror("strdup");
		return -1;
	}

	static const char delim[] = ",";
	int ret = 0;
	char *name = strtok_r(s, delim, &saveptr);
	if (saveptr && *saveptr) {
		for (; saveptr && *saveptr;) {
			char *type_str = strtok_r(NULL, delim, &saveptr);
			type = neb_resolver_parse_type(type_str, 0);

			if (parse_query_data(name, type) != 0) {
				ret = -1;
				break;
			}
		}
	} else {
		if (parse_query_data(name, type) != 0)
			ret = -1;
	}

	free(s);
	return ret;
}

static int parse_args(int argc, char *argv[])
{
	int opt;
	const char *bind_addr = NULL;

	while ((opt = getopt_long(argc, argv, "b:t:I:N:", opts, NULL)) != -1) {
		switch (opt) {
		case 'b':
			bind_addr = optarg;
			break;
		case 't':
			default_resolver_type = neb_resolver_parse_type(optarg, 0);
			if (default_resolver_type == ns_t_invalid) {
				fprintf(stderr, "unsupported query type: %s\n", optarg);
				return -1;
			}
			break;
		case 'I':
			break;
		case 'N':
			break;
		case '?':
			return -1;
			break;
		default:
			break;
		}
	}

	if (bind_addr) {
		if (strchr(bind_addr, ':')) {
			if (inet_pton(AF_INET6, bind_addr, &((struct sockaddr_in6 *)resolver_bind_addr)->sin6_addr) != 1) {
				fprintf(stderr, "Invalid IPv6 bind address %s\n", bind_addr);
				return -1;
			}
			resolver_bind_addrlen = sizeof(struct sockaddr_in6);
		} else {
			if (inet_pton(AF_INET, bind_addr, &((struct sockaddr_in *)resolver_bind_addr)->sin_addr) != 1) {
				fprintf(stderr, "Invalid IPv4 bind address %s\n", bind_addr);
				return -1;
			}
			resolver_bind_addrlen = sizeof(struct sockaddr_in);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "no host / address given\n");
		return -1;
	}

	resolver_servers = calloc(1, sizeof(struct ares_addr_port_node));
	if (!resolver_servers) {
		perror("calloc");
		return -1;
	}

	for (int i = optind; i < argc; i++) {
		const char *arg = argv[i];

		if (arg[0] == '@') { // set server
			const char *server = arg+1;
			if (insert_server(server) != 0)
				return -1;
			continue;
		}

		if (parse_query_data_arg(arg) != 0)
			return -1;
	}

	return query_data_init_submit();
}

int main(int argc, char *argv[])
{
	int ret = 0;

	ares_library_init(ARES_LIB_INIT_ALL);

	if (parse_args(argc, argv) != 0) {
		fprintf(stderr, "failed to parse args");
		ret = -1;
		goto deinit_args;
	}

	evdp_timer = neb_evdp_timer_create(0, 0);
	if (!evdp_timer) {
		fprintf(stderr, "failed to create evdp timer\n");
		ret = -1;
		goto deinit_args;
	}
	evdp_queue = neb_evdp_queue_create(0);
	if (!evdp_queue) {
		fprintf(stderr, "failed to create evdp queue\n");
		ret = -1;
		goto deinit_timer;
	}
	neb_evdp_queue_set_timer(evdp_queue, evdp_timer);

	submit_tp = neb_evdp_timer_new_point(evdp_timer, 0, submit_query, NULL);
	if (!submit_tp) {
		fprintf(stderr, "failed to create submit timer point\n");
		ret = -1;
		goto deinit_queue;
	}

	resolver = neb_resolver_create(&resolver_opts, resolver_optmask);
	if (!resolver) {
		fprintf(stderr, "failed to create resolver\n");
		ret = -1;
		goto del_timer_point;
	}
	if (resolver_bind_addrlen != 0 && neb_resolver_set_bind_ip(resolver, resolver_bind_addr) != 0) {
		fprintf(stderr, "invalid bind paramater\n");
		ret = -1;
		goto deinit_resolver;
	}
	if (resolver_servers->family != 0 && neb_resolver_set_servers(resolver, resolver_servers) != 0) {
		fprintf(stderr, "failed to set servers");
		ret = -1;
		goto deinit_resolver;
	}
	if (neb_resolver_associate(resolver, evdp_queue) != 0) {
		fprintf(stderr, "failed to associate resolver to evdp queue\n");
		ret = -1;
		goto deinit_resolver;
	}

	if (neb_evdp_queue_run(evdp_queue) != 0) {
		fprintf(stderr, "error occured during running of evdp queue\n");
		ret = -1;
	}

	query_data_foreach_cancel();
	neb_resolver_disassociate(resolver);
deinit_resolver:
	neb_resolver_destroy(resolver);
del_timer_point:
	neb_evdp_timer_del_point(evdp_timer, submit_tp);
deinit_queue:
	neb_evdp_queue_destroy(evdp_queue);
deinit_timer:
	neb_evdp_timer_destroy(evdp_timer);
deinit_args:
	deinit_args();
	ares_library_cleanup();
	return ret;
}


#include <nebase/stats/swap.h>
#include <nebase/syslog.h>
#include <nebase/obstack.h>

#include <sys/queue.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char proc_swaps_devname[] = "/proc/swaps";
static const char proc_delimiter[] = " \t";

struct swap_stat_node {
	TAILQ_ENTRY(swap_stat_node) tailq_context;
	char *filename;
	size_t total_size;
	size_t used_size;
};

TAILQ_HEAD(swap_stat_node_list, swap_stat_node);

struct neb_stats_swap {
	struct obstack obs;
	struct swap_stat_node_list nodes;
	int count;
};

neb_stats_swap_t neb_stats_swap_load(void)
{
	struct neb_stats_swap *s = malloc(sizeof(struct neb_stats_swap));
	if (!s) {
		neb_syslogl(LOG_ERR, "malloc: %m");
		return NULL;
	}
	obstack_init(&s->obs);
	TAILQ_INIT(&s->nodes);
	s->count = 0;

	FILE *stream = fopen(proc_swaps_devname, "re");
	if (!stream) {
		neb_syslogl(LOG_ERR, "fopen(%s): %m", proc_swaps_devname);
		free(s);
		return NULL;
	}

	char *line = NULL;
	size_t line_len = 0;
	ssize_t nr = 0;
	while ((nr = getline(&line, &line_len, stream)) != -1) {
		if (line_len < 1 || line[0] != '/')
			continue;

		char *saveptr;
		char *filename = strtok_r(line, proc_delimiter, &saveptr);
		char *type = strtok_r(NULL, proc_delimiter, &saveptr);
		char *size = strtok_r(NULL, proc_delimiter, &saveptr);
		char *used = strtok_r(NULL, proc_delimiter, &saveptr);

		if (!type || !type[0]) {
			neb_syslog(LOG_ERR, "Invalid line in %s: no type field", proc_swaps_devname);
			continue;
		}

		struct swap_stat_node *n = obstack_alloc(&s->obs, sizeof(struct swap_stat_node));
		if (!n) {
			neb_syslogl(LOG_ERR, "obstack_alloc: %m");
			continue;
		}
		memset(n, 0, sizeof(struct swap_stat_node));
		n->filename = obstack_copy0(&s->obs, filename, strlen(filename));
		if (!n->filename) {
			neb_syslogl(LOG_ERR, "obstack_copy0: %m");
			continue;
		}
		size_t total_kb = strtoull(size, NULL, 10);
		size_t used_kb = strtoull(used, NULL, 10);
		n->total_size = total_kb << 10;
		n->used_size = used_kb << 10;

		TAILQ_INSERT_TAIL(&s->nodes, n, tailq_context);
		s->count++;
	}

	if (line)
		free(line);
	return s;
}

void neb_stats_swap_release(neb_stats_swap_t s)
{
	obstack_free(&s->obs, NULL);
	free(s);
}

int neb_stats_swap_device_num(const neb_stats_swap_t s)
{
	return s->count;
}

void neb_stats_swap_device_foreach(const neb_stats_swap_t s, swap_device_each_t f, void *udata)
{
	struct swap_stat_node *n;
	TAILQ_FOREACH(n, &s->nodes, tailq_context)
		f(n->filename, n->total_size, n->used_size, udata);
}

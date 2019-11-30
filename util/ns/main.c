
#include "main.h"

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

static pid_t proc_pid = 0;

static void print_item(const char *item)
{
	switch (item[0]) {
	case 's':
		print_stats_swap();
		break;
	case 'p':
		print_stats_proc(proc_pid);
		break;
	default:
		break;
	}
}

static void print_usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [options] <item> [<item> ...]\n", progname);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "  -h: show help message\n");
	fprintf(stderr, "  -p: optional, pid for proc stats\n");
	fprintf(stderr, "Items:\n");
	fprintf(stderr, "  swap, proc\n");
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt(argc, argv, "hp:")) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 0;
			break;
		case 'p':
			proc_pid = strtol(optarg, NULL, 10);
			if (proc_pid <= 0) {
				fprintf(stderr, "invalid pid %s\n", optarg);
				return -1;
			}
			break;
		case '?':
		default:
			print_usage(argv[0]);
			return -1;
			break;
		}
	}

	for (int i = optind; i < argc; i++)
		print_item(argv[i]);

	return 0;
}

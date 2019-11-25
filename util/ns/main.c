
#include "main.h"

#include <stdio.h>
#include <unistd.h>

static void print_item(const char *item)
{
	switch (item[0]) {
	case 's':
		print_stats_swap();
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
	fprintf(stderr, "Items:\n");
	fprintf(stderr, "  swap\n");
	fprintf(stderr, "\n");
}

int main(int argc, char *argv[])
{
	int opt;
	while ((opt = getopt(argc, argv, "h")) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 0;
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

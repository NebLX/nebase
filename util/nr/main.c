
#include <nebase/random.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

enum {
	RNG_TYPE_NONE = 0,
	RNG_TYPE_BINARY,
	RNG_TYPE_PRINTABLE,
	RNG_TYPE_NUMERICAL,
};

/*
 * See https://www.w3.org/Addressing/URL/url-spec.txt for ASCII class
 */

static const char ascii_alphanum2[] =
	"abcdefghijklmnop"
	"qrstuvwxyzABCDEF"
	"GHIJKLMNOPQRSTUV"
	"WXYZ0123456789-_"
	".+";
static const char ascii_printable[] =
	" !\"#$%&'()*+,-./"
	"0123456789:;<=>?"
	"@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmno"
	"pqrstuvwxyz{|}~";

static int rng_type = RNG_TYPE_NONE;

static uint32_t numerical_upper_bound = 100;
static const char *printable_characters = ascii_printable;

static int rng_binary(long count)
{
	long left = count;
	char buf[BUFSIZ];
	do {
		long n = left > BUFSIZ ? BUFSIZ : left;
		left -= n;
		neb_random_buf(buf, BUFSIZ);
		size_t nw = fwrite(buf, 1, n, stdout);
		if (nw < (size_t)n)
			return -1;
	} while (left > 0);

	return 0;
}

static int rng_printable(long count)
{
	size_t upper_bound = strlen(printable_characters);
	if (upper_bound < NEB_RANDOM_MIN_UPPER_BOUND || upper_bound > UINT32_MAX) {
		fprintf(stderr, "Invalid length of printable characters\n");
		return -1;
	}

	for (long i = 0; i < count; i++)
		fprintf(stdout, "%c", printable_characters[neb_random_uniform(upper_bound)]);
	fprintf(stdout, "\n");

	return 0;
}

static void rng_numerical(long count)
{
	fprintf(stdout, "%u", neb_random_uniform(numerical_upper_bound));
	for (long i = 1; i < count; i++)
		fprintf(stdout, " %u", neb_random_uniform(numerical_upper_bound));
	fprintf(stdout, "\n");
}

static void usage_print(const char *progname)
{
	fprintf(stdout, "Usage: %s <command switch> [<count>]\n", progname);
	fprintf(stdout, "Command Switch:\n");
	fprintf(stdout, "  -b: binary output <count> bytes\n");
	fprintf(stdout, "  -c <char-range-str>:\n");
	fprintf(stdout, "    output <count> bytes from <char-range-str>\n");
	fprintf(stdout, "  -p: printable output <count> chars\n");
	fprintf(stdout, "  -P: output <count> of url-password chars\n");
	fprintf(stdout, "  -u [<upper-bound>]:\n");
	fprintf(stdout, "    output <count> uint which is less than <upper-bound>\n");
}

int main(int argc, char *argv[])
{
	int opt;

	while ((opt = getopt(argc, argv, "bc:hpPu::")) != -1) {
		switch (opt) {
		case 'h':
			usage_print(argv[0]);
			return 0;
		case 'b':
			rng_type = RNG_TYPE_BINARY;
			break;
		case 'c':
			rng_type = RNG_TYPE_PRINTABLE;
			printable_characters = optarg;
			break;
		case 'u':
			rng_type = RNG_TYPE_NUMERICAL;
			if (optarg) {
				char *endptr = NULL;
				unsigned long value = strtoul(optarg, &endptr, 10);
				if (endptr && *endptr) {
					fprintf(stderr, "Invalid numerical upper bound value %s\n", optarg);
					return -1;
				}
				if (value < NEB_RANDOM_MIN_UPPER_BOUND || value > UINT32_MAX) {
					fprintf(stderr, "Out of range numerical upper bound value %lu\n", value);
					return -1;
				}
				numerical_upper_bound = value;
			}
			break;
		case 'p':
			rng_type = RNG_TYPE_PRINTABLE;
			printable_characters = ascii_printable;
			break;
		case 'P':
			rng_type = RNG_TYPE_PRINTABLE;
			printable_characters = ascii_alphanum2;
			break;
		case '?':
		default:
			usage_print(argv[0]);
			return -1;
			break;
		}
	}

	long count = 1;
	if (optind < argc) {
		const char *s = argv[optind];
		char *endptr = NULL;
		count = strtol(s, &endptr, 10);
		if (endptr && *endptr) {
			fprintf(stderr, "Invalid count value %s\b", s);
			return -1;
		}
		if (count <= 0) {
			fprintf(stderr, "Out of range count valud %ld\n", count);
			return -1;
		}
	}

	switch (rng_type) {
	case RNG_TYPE_BINARY:
		return rng_binary(count);
		break;
	case RNG_TYPE_PRINTABLE:
		return rng_printable(count);
		break;
	case RNG_TYPE_NUMERICAL:
		rng_numerical(count);
		break;
	default:
		break;
	}

	return 0;
}

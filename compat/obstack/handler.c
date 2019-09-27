
#include <obstack.h>

#include <stdio.h>
#include <stdlib.h>

int obstack_exit_failure = -1;

static __attribute_noreturn__ void
print_and_abort (void)
{
	fprintf(stderr, "memory exhausted\n");
	exit(obstack_exit_failure);
}

__attribute_noreturn__ void (*obstack_alloc_failed_handler) (void)
	= print_and_abort;

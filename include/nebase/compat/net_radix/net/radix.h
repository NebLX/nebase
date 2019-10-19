
#ifndef NEB_COMPAT_RADIX_H
#define NEB_COMPAT_RADIX_H 1

#include <freebsd/radix.h>

#ifndef KASSERT
# include <stdlib.h>
# include <stdio.h>

# define abort_with_msg(fmt, ...) fprintf(stderr, "ABORT: "fmt, ##__VA_ARGS__), abort()

# ifndef __predict_false
#  define __predict_false(cond) __builtin_expect ((cond), 0)
# endif

# define KASSERT(exp, msg) do {  \
	if (__predict_false(!(exp))) {abort_with_msg msg;}        \
} while(0)
#endif

#endif

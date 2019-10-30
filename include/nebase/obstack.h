
#ifndef NEB_OBSTACK_H
#define NEB_OBSTACK_H 1

#define obstack_chunk_alloc malloc
#define obstack_chunk_free free

/*
 * https://www.gnu.org/software/libc/manual/html_node/Obstacks.html
 */
#include <gnulib/obstack.h>

#endif

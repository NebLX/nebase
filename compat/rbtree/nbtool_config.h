
#include <nebase/cdefs.h>

#define __RCSID(x)

#define __predict_false(cond) __builtin_expect ((cond), 0)

#ifndef __unused
# define __unused _nattr_unused
#endif

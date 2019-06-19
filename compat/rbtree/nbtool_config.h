
#include <nebase/cdefs.h>

#ifndef __RCSID
#define __RCSID(s) struct __hack
#endif

#ifndef __predict_false
#define __predict_false(cond) __builtin_expect ((cond), 0)
#endif

#ifndef __unused
#define __unused _nattr_unused
#endif

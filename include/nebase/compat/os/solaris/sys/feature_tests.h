
#ifndef NEB_COMPAT_SYS_FEATURE_TESTS_H
#define NEB_COMPAT_SYS_FEATURE_TESTS_H 1

#include_next <sys/feature_tests.h>

#ifndef __BEGIN_DECLS
/* C++ needs to know that types and declarations are C, not C++.  */
# ifdef __cplusplus
#  define __BEGIN_DECLS extern "C" {
#  define __END_DECLS   }
# else
#  define __BEGIN_DECLS
#  define __END_DECLS
# endif
#endif

#ifndef _GL_ATTRIBUTE_PURE
# define _GL_ATTRIBUTE_PURE __attribute__((__pure__))
#endif

#endif

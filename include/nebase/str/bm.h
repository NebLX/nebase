
#ifndef NEB_STR_BM_H
#define NEB_STR_BM_H 1

/*
 * Boyer-Moore string search algorithm
 */

#include <nebase/cdefs.h>

#include <sys/types.h>

struct neb_str_bm_ctx;
typedef struct neb_str_bm_ctx* neb_str_bm_ctx_t;

extern const u_char *neb_str_bm_search(const u_char *p, int plen, const u_char *t, int64_t tlen)
	_nattr_nonnull((1, 3));

extern neb_str_bm_ctx_t neb_str_bm_ctx_create(const u_char *p, int pl)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_str_bm_ctx_destroy(neb_str_bm_ctx_t c)
	_nattr_nonnull((1));
extern const u_char *neb_str_bm_ctx_search(neb_str_bm_ctx_t c, const u_char *t, int64_t tl)
	_nattr_nonnull((1, 2));

#endif

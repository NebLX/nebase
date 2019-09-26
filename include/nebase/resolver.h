
#ifndef NEB_RESOLVER_H
#define NEB_RESOLVER_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp.h>

#include <ares.h>

typedef struct neb_resolver* neb_resolver_t;
typedef struct neb_resolver_ctx* neb_resolver_ctx_t;

extern neb_resolver_t neb_resolver_create(struct ares_options *options, int optmask)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_resolver_destroy(neb_resolver_t r)
	_nattr_nonnull((1));

extern int neb_resolver_associate(neb_resolver_t r, neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
extern void neb_resolver_disassociate(neb_resolver_t r)
	_nattr_nonnull((1));


/**
 * resolver context, for each resolve action
 */

extern neb_resolver_ctx_t neb_resolver_new_ctx(neb_resolver_t r, void *udata)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_resolver_del_ctx(neb_resolver_t r, neb_resolver_ctx_t c)
	_nattr_nonnull((1, 2));



#endif

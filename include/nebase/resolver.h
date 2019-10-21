
#ifndef NEB_RESOLVER_H
#define NEB_RESOLVER_H 1

#include <nebase/cdefs.h>
#include <nebase/evdp.h>

#include <stdbool.h>
#include <sys/socket.h>

#include <ares.h>

typedef struct neb_resolver* neb_resolver_t;
typedef struct neb_resolver_ctx* neb_resolver_ctx_t;

extern neb_resolver_t neb_resolver_create(struct ares_options *options, int optmask)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_resolver_destroy(neb_resolver_t r)
	_nattr_nonnull((1));
/**
 * \param[in] addr port is not used, only family and addr should be set
 * \return -1 if invalid param, or 0 if set, but it doesn't mean the real bind will be success
 */
extern int neb_resolver_set_bind_ip(neb_resolver_t r, const struct sockaddr *addr, socklen_t addrlen)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));

extern int neb_resolver_associate(neb_resolver_t r, neb_evdp_queue_t q)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));
extern void neb_resolver_disassociate(neb_resolver_t r)
	_nattr_nonnull((1));

extern int neb_resolver_set_servers(neb_resolver_t r, struct ares_addr_port_node *servers)
	_nattr_warn_unused_result _nattr_nonnull((1, 2));


/**
 * resolver context, for each resolve action
 */

extern neb_resolver_ctx_t neb_resolver_new_ctx(neb_resolver_t r, void *udata)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_resolver_del_ctx(neb_resolver_t r, neb_resolver_ctx_t c)
	_nattr_nonnull((1, 2));
extern bool neb_resolver_ctx_in_use(neb_resolver_ctx_t c)
	_nattr_nonnull((1));

/**
 * \param[in] name there is no IDN convertion in this function
 */
extern int neb_resolver_ctx_gethostbyname(neb_resolver_ctx_t c, const char *name, int family, ares_host_callback cb)
	_nattr_warn_unused_result _nattr_nonnull((1, 2, 4));
/**
 * \param[in] addr port is not used, only family and addr should be set
 */
extern int neb_resolver_ctx_gethostbyaddr(neb_resolver_ctx_t c, const struct sockaddr *addr, socklen_t addrlen, ares_host_callback cb)
	_nattr_warn_unused_result _nattr_nonnull((1, 2, 4));
extern int neb_resolver_ctx_send(neb_resolver_ctx_t c, const unsigned char *qbuf, int qlen, ares_callback cb)
	_nattr_warn_unused_result _nattr_nonnull((1, 2, 4));


/**
 * util functions
 */

extern struct ares_addr_port_node *neb_resolver_new_server(const char *s)
	_nattr_warn_unused_result _nattr_nonnull((1));
extern void neb_resolver_del_server(struct ares_addr_port_node *n)
	_nattr_nonnull((1));

/**
 * \param[in] len may be 0 if type is null terminated
 * \return DNS type value, as defined in \<arpa/nameser.h>
 */
extern int neb_resolver_parse_type(const char *type, int len)
	_nattr_nonnull((1)) _nattr_const;

#endif

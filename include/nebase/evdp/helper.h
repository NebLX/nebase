
#ifndef NEB_EVDP_HELPER_H
#define NEB_EVDP_HELPER_H 1

#include <nebase/cdefs.h>

#include "types.h"

/**
 * \brief log sockerr and then return close
 */
extern neb_evdp_cb_ret_t neb_evdp_sock_log_on_hup(int fd, void *udata, const void *context);

#endif

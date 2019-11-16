
#ifndef NEB_COMPAT_NETINET_IP_H
#define NEB_COMPAT_NETINET_IP_H 1

#include_next <netinet/ip.h>

#ifndef IPDEFTTL
# define IPDEFTTL 64 /* default ttl, from RFC 1340 */
#endif

#endif

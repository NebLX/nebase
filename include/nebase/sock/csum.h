
#ifndef NEB_SOCK_CSUM_H
#define NEB_SOCK_CSUM_H 1

#include <nebase/cdefs.h>

struct ip;
struct ip6_hdr;
struct icmp;
struct tcphdr;

extern void neb_sock_csum_tcp4_fill(const struct ip *ipheader, struct tcphdr *tcpheader, int l4len)
	_nattr_nonnull((1, 2));
extern void neb_sock_csum_tcp6_fill(const struct ip6_hdr *ipheader, struct tcphdr *tcpheader, int l4len)
	_nattr_nonnull((1, 2));
extern void neb_sock_csum_icmp4_fill(struct icmp *icmpheader, int l4len)
	_nattr_nonnull((1));
extern void neb_sock_csum_ip4_fill(struct ip *ipheader)
	_nattr_nonnull((1));

#endif

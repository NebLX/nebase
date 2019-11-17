
#ifndef NEB_SOCK_RAW_H
#define NEB_SOCK_RAW_H 1

#include <nebase/cdefs.h>

#include <sys/types.h>
#include <stdint.h>
#include <netinet/in.h>

/*
 * IPv4 General Raw Sockets (Transparent)
 */

extern int neb_sock_raw4_new(int protocol)
	_nattr_warn_unused_result;
/**
 * \param[in] fd get it with neb_sock_raw4_new()
 * \param[in] data should contains the ip header, as we use IP_HDRINCL
 * \return the data sent, -1 if error
 */
extern ssize_t neb_sock_raw4_send(int fd, const u_char *data, size_t len)
	_nattr_warn_unused_result _nattr_nonnull((2));

extern void neb_sock_raw_init_iphdr(u_char *data, uint16_t total_len, uint8_t hdr_len,
                                    const struct in_addr *src, const struct in_addr *dst,
                                    uint8_t p, uint8_t tos, uint8_t ttl)
	_nattr_nonnull((1, 4, 5));

/*
 * ICMP Raw Sockets (Local)
 */

extern int neb_sock_raw_icmp4_new(void)
	_nattr_warn_unused_result;
extern ssize_t neb_sock_raw_icmp4_send(int fd, const u_char *data, size_t len,
                                       const struct in_addr *dst,
                                       const struct in_addr *src)
	_nattr_warn_unused_result _nattr_nonnull((2, 4));

/*
 * ICMPv6 Raw Sockets (Local)
 */

extern int neb_sock_raw_icmp6_new(void);

#endif

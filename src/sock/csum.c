
#include <nebase/endian.h>
#include <nebase/sock/csum.h>

#include <sys/types.h>
#include <stdint.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#ifndef __wsum
# define __wsum uint32_t
#endif
#ifndef __sum16
# define __sum16 uint16_t
#endif

static inline uint16_t from32to16(uint32_t x)
{
	/* add up 16-bit and 16-bit for 16+c bit */
	x = (x & 0xffff) + (x >> 16);
	/* add up carry.. */
	x = (x & 0xffff) + (x >> 16);
	return x;
}

static unsigned int do_csum(const u_char *buff, int len)
{
	int odd;
	unsigned int result = 0;

	if (len <= 0)
		goto out;
	odd = 1 & (unsigned long) buff;
	if (odd) {
#if __BYTE_ORDER == __LITTLE_ENDIAN
		result += (*buff << 8);
#else
		result = *buff;
#endif
		len--;
		buff++;
	}
	if (len >= 2) {
		if (2 & (unsigned long) buff) {
			result += *(unsigned short *) buff;
			len -= 2;
			buff += 2;
		}
		if (len >= 4) {
			const u_char *end = buff + ((unsigned)len & ~3);
			unsigned int carry = 0;
			do {
				unsigned int w = *(unsigned int *) buff;
				buff += 4;
				result += carry;
				result += w;
				carry = (w > result);
			} while (buff < end);
			result += carry;
			result = (result & 0xffff) + (result >> 16);
		}
		if (len & 2) {
			result += *(unsigned short *) buff;
			buff += 2;
		}
	}
	if (len & 1)
#if __BYTE_ORDER == __LITTLE_ENDIAN
		result += *buff;
#else
		result += (*buff << 8);
#endif
	result = from32to16(result);
	if (odd)
		result = ((result >> 8) & 0xff) | ((result & 0xff) << 8);
out:
	return result;
}

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
static __wsum csum_partial(const void *buff, int len, __wsum wsum)
{
	unsigned int sum = (unsigned int)wsum;
	unsigned int result = do_csum(buff, len);

	/* add in old sum, and carry.. */
	result += sum;
	if (sum > result)
		result += 1;
	return (__wsum)result;
}

static inline uint32_t from64to32(uint64_t x)
{
	/* add up 32-bit and 32-bit for 32+c bit */
	x = (x & 0xffffffff) + (x >> 32);
	/* add up carry.. */
	x = (x & 0xffffffff) + (x >> 32);
	return (uint32_t)x;
}

static __wsum csum_tcpudp_nofold(in_addr_t saddr, in_addr_t daddr,
                                 unsigned short len, unsigned short proto,
                                 __wsum sum)
{
	unsigned long long s = (uint32_t)sum;

	s += (uint32_t)saddr;
	s += (uint32_t)daddr;
#if __BYTE_ORDER == __BIG_ENDIAN
	s += proto + len;
#else
	s += (proto + len) << 8;
#endif
	return (__wsum)from64to32(s);
}

/*
 * Fold a partial checksum
 */
static inline __sum16 csum_fold(__wsum csum)
{
	uint32_t sum = (uint32_t)csum;
	sum = (sum & 0xffff) + (sum >> 16);
	sum = (sum & 0xffff) + (sum >> 16);
	return (__sum16)~sum;
}

/*
 * This is a version of ip_compute_csum() optimized for IP headers,
 * which always checksum on 4 octet boundaries.
 */
static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	return (__sum16)~do_csum(iph, ihl << 2);
}

static inline __sum16 csum_tcpudp_magic(in_addr_t saddr, in_addr_t daddr,
                                        unsigned short len, unsigned short proto,
                                        __wsum sum)
{
	return csum_fold(csum_tcpudp_nofold(saddr, daddr, len, proto, sum));
}

static __sum16 csum_ipv6_magic(const struct in6_addr *saddr,
                               const struct in6_addr *daddr,
                               uint32_t len, unsigned short proto,
                               __wsum csum)
{
	int carry;
	uint32_t ulen;
	uint32_t uproto;
	uint32_t sum = (uint32_t)csum;

	const uint32_t *saddr32 = (const uint32_t *)saddr;
	const uint32_t *daddr32 = (const uint32_t *)daddr;

	sum += saddr32[0];
	carry = (sum < saddr32[0]);
	sum += carry;

	sum += saddr32[1];
	carry = (sum < saddr32[1]);
	sum += carry;

	sum += saddr32[2];
	carry = (sum < saddr32[2]);
	sum += carry;

	sum += saddr32[3];
	carry = (sum < saddr32[3]);
	sum += carry;

	sum += daddr32[0];
	carry = (sum < daddr32[0]);
	sum += carry;

	sum += daddr32[1];
	carry = (sum < daddr32[1]);
	sum += carry;

	sum += daddr32[2];
	carry = (sum < daddr32[2]);
	sum += carry;

	sum += daddr32[3];
	carry = (sum < daddr32[3]);
	sum += carry;

	ulen = (uint32_t)htobe32((uint32_t)len);
	sum += ulen;
	carry = (sum < ulen);
	sum += carry;

	uproto = (uint32_t)htobe32((uint32_t)proto);
	sum += uproto;
	carry = (sum < uproto);
	sum += carry;

	return csum_fold((__wsum)sum);
}

void neb_sock_csum_tcp4_fill(const struct ip *ipheader, struct tcphdr *tcpheader, int l4len)
{
	tcpheader->th_sum = 0;
	tcpheader->th_sum = csum_tcpudp_magic(ipheader->ip_src.s_addr, ipheader->ip_dst.s_addr,
	                                      l4len, IPPROTO_TCP,
	                                      csum_partial(tcpheader, l4len, 0));
}

void neb_sock_csum_tcp6_fill(const struct ip6_hdr *ipheader, struct tcphdr *tcpheader, int l4len)
{
	tcpheader->th_sum = 0;
	tcpheader->th_sum = csum_ipv6_magic(&ipheader->ip6_src, &ipheader->ip6_dst,
	                                    l4len, IPPROTO_TCP,
	                                    csum_partial(tcpheader, l4len, 0));
}

void neb_sock_csum_icmp4_fill(struct icmp *icmpheader, int l4len)
{
	icmpheader->icmp_cksum = 0;
	icmpheader->icmp_cksum = csum_fold(csum_partial(icmpheader, l4len, 0));
}

void neb_sock_csum_ip4_fill(struct ip *ipheader)
{
	ipheader->ip_sum = 0;
	ipheader->ip_sum = ip_fast_csum(ipheader, ipheader->ip_hl);
}



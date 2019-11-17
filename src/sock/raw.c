
#include "options.h"

#include <nebase/syslog.h>
#include <nebase/sock/raw.h>
#include <nebase/sock/inet.h>

#include <unistd.h>
#include <string.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

void neb_sock_raw_init_iphdr(u_char *data, uint16_t total_len, uint8_t hdr_len,
                             const struct in_addr *src, const struct in_addr *dst,
                             uint8_t p, uint8_t tos, uint8_t ttl)
{
	if (total_len < sizeof(struct ip)) {
		neb_syslog(LOG_CRIT, "Invalid buffer size %u for IP header", total_len);
		return;
	}
	if ((hdr_len & 0b0011) != 0) {
		neb_syslog(LOG_CRIT, "Invalid IP header length %u", hdr_len);
		return;
	}
	struct ip *iphdr = (struct ip *)data;
	iphdr->ip_v = IPVERSION;
	iphdr->ip_hl = hdr_len >> 2;
	iphdr->ip_tos = tos;
	iphdr->ip_len = htons(total_len);
	iphdr->ip_id = 0; // let the kernel generate it
	iphdr->ip_off = 0; // user should set it later if needed
	if (ttl)
		iphdr->ip_ttl = ttl;
	else
		iphdr->ip_ttl = IPDEFTTL;
	iphdr->ip_p = p;
	iphdr->ip_sum = 0; // let the kernel fill if supported, or just clear for later calc
	iphdr->ip_src.s_addr = src->s_addr;
	iphdr->ip_dst.s_addr = dst->s_addr;
}

int neb_sock_raw4_new(int protocol)
{
	int fd = neb_sock_inet_new(AF_INET, SOCK_RAW, protocol);
	if (fd == -1)
		return -1;

	int hincl = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof(hincl)) < 0) {
		neb_syslog(LOG_ERR, "setsockopt(IPPROTO_IP/IP_HDRINCL): %m");
		return -1;
	}

	return fd;
}

ssize_t neb_sock_raw4_send(int fd, const u_char *data, size_t len)
{
	struct ip *iphdr = (struct ip *)data;
	struct sockaddr_in sa = {
		.sin_family = AF_INET,
		.sin_port = 0,
		.sin_addr.s_addr = iphdr->ip_dst.s_addr,
	};
	switch (iphdr->ip_p) {
	case IPPROTO_TCP:
		sa.sin_port = ((struct tcphdr *)(data + (iphdr->ip_hl << 2)))->th_dport;
		break;
	case IPPROTO_UDP:
		sa.sin_port = ((struct udphdr *)(data + (iphdr->ip_hl << 2)))->uh_dport;
		break;
	case IPPROTO_ICMP: // should set port to 0
	default:
		break;
	}

	ssize_t nw = sendto(fd, data, len, MSG_DONTWAIT | MSG_NOSIGNAL, (struct sockaddr *)&sa, sizeof(struct sockaddr_in));
	if (nw == -1) {
		neb_syslog(LOG_ERR, "sendto: %m");
		return -1;
	}

	return nw;
}

int neb_sock_raw_icmp4_new(void)
{
	int fd = neb_sock_inet_new(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	if (fd == -1)
		return -1;

#if defined(IP_RECVPKTINFO) /* for SunOS, NetBSD, AIX ... */
	int on = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_RECVPKTINFO, &on, sizeof(on)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(IPPROTO_IP/IP_RECVPKTINFO): %m");
		close(fd);
		return -1;
	}
#elif defined(IP_PKTINFO) /* for Linux */
	int on = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(IPPROTO_IP/IP_PKTINFO): %m");
		close(fd);
		return -1;
	}
#elif defined(IP_RECVIF) /* for BSD, excluding NetBSD */
	// RECVIF not work for RAW sockets, see comment in netinet/in.h
	int on = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_RECVIF, &on, sizeof(on)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(IPPROTO_IP/IP_RECVIF): %m");
		close(fd);
		return -1;
	}
#else
# error "fix me"
#endif

	return fd;
}

ssize_t neb_sock_raw_icmp4_send(int fd, const u_char *data, size_t len,
                                const struct in_addr *dst, const struct in_addr *src)
{
	struct iovec iov = {
		.iov_base = (void *)data,
		.iov_len = len
	};
#if defined(IP_PKTINFO)
	char buf[CMSG_SPACE(sizeof(struct in_pktinfo))];
#elif defined(IP_SENDSRCADDR)
	char buf[CMSG_SPACE(sizeof(struct in_addr))];
#else
# error "fix me"
#endif
	struct sockaddr_in dst_addr = {
		.sin_family = AF_INET,
		.sin_port = 0,
		.sin_addr.s_addr = dst->s_addr,
	};
	struct msghdr msg = {
		.msg_name = &dst_addr,
		.msg_namelen = sizeof(struct sockaddr_in),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
	};

	if (src) {
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = IPPROTO_IP;
#if defined(IP_PKTINFO)
		cmsg->cmsg_type = IP_PKTINFO;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
		struct in_pktinfo *info = (struct in_pktinfo *)CMSG_DATA(cmsg);
		info->ipi_ifindex = 0;
		info->ipi_spec_dst.s_addr = src->s_addr;
		info->ipi_addr.s_addr = 0;
#elif defined(IP_SENDSRCADDR)
		cmsg->cmsg_type = IP_SENDSRCADDR;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct in_addr));
		struct in_addr *addr = (struct in_addr *)CMSG_DATA(cmsg);
		addr->s_addr = src->s_addr;
#else
# error "fix me"
#endif
		msg.msg_control = buf;
		msg.msg_controllen = sizeof(buf);
	}

	ssize_t nw = sendmsg(fd, &msg, MSG_DONTWAIT | MSG_NOSIGNAL);
	if (nw == -1) {
		neb_syslog(LOG_ERR, "sendmsg: %m");
		return -1;
	}

	return nw;
}

int neb_sock_raw_icmp6_new(void)
{
	int fd = neb_sock_inet_new(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	if (fd == -1)
		return -1;

	int on = 1;
	if (setsockopt(fd, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)) == -1) {
		neb_syslog(LOG_ERR, "setsockopt(IPPROTO_IPV6/IPV6_PKTINFO): %m");
		close(fd);
		return -1;
	}

	return fd;
}

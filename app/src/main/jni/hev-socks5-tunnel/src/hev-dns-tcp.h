/*
 ============================================================================
 Name        : hev-dns-tcp.h
 Author      : TVProxy
 Description : TCP-only upstream: drop non-DNS UDP (no DNS forwarding)
 ============================================================================
 */

#ifndef __HEV_DNS_TCP_H__
#define __HEV_DNS_TCP_H__

/**
 * Reset drop counters. Call once per hev_socks5_tunnel_init.
 */
void hev_dns_tcp_init (void);

/**
 * Flush remaining drop stats. Idempotent.
 */
void hev_dns_tcp_stop (void);

/**
 * Destination UDP port of an IPv4 UDP datagram, or -1 if not IPv4 UDP.
 * Returns 0 for a continuation fragment with no UDP header.
 */
int hev_dns_tcp_udp_dst_port (const unsigned char *ip, unsigned int len);

/**
 * Count a datagram dropped because the upstream relays no UDP.
 * Logs a per-port summary at most once per stats interval.
 */
void hev_dns_tcp_udp_drop (unsigned int dst_port);

/**
 * Dump and reset drop counters, at most once per stats interval.
 */
void hev_dns_tcp_stats_flush (void);

#endif /* __HEV_DNS_TCP_H__ */

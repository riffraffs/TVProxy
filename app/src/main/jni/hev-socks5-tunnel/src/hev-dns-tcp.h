/*
 ============================================================================
 Name        : hev-dns-tcp.h
 Author      : TVProxy
 Description : DNS over TCP fallback for upstreams without UDP relay
 ============================================================================
 */

#ifndef __HEV_DNS_TCP_H__
#define __HEV_DNS_TCP_H__

/**
 * Reset module state for a new tunnel run. Must be called once per
 * hev_socks5_tunnel_init (i.e. once per TProxyStartService).
 */
void hev_dns_tcp_init (void);

/**
 * Stop accepting new queries, drop queued ones and abort in-flight workers.
 * Idempotent.
 */
void hev_dns_tcp_stop (void);

/**
 * Inspect a raw IPv4 packet read from the tun device. When the DNS-over-TCP
 * mode is on and the packet is a plain (unfragmented) UDP query to port 53,
 * the packet is consumed (1) and forwarded to the upstream SOCKS5 server as a
 * TCP CONNECT + DNS-over-TCP exchange; the answer is written back into the
 * tun as a UDP packet. Queries beyond the in-flight limit are queued (bounded)
 * and consumed as well; only a full queue drops them.
 *
 * Returns 0 when the packet is not handled and must go through the normal
 * lwIP path (never happens for a UDP datagram while this mode is on, see
 * hev_dns_tcp_udp_dst_port).
 */
int hev_dns_tcp_handle_packet (const unsigned char *ip, unsigned int len);

/**
 * Return the destination UDP port of an IPv4 UDP datagram, or -1 when the
 * packet is not IPv4 UDP. Used by the tun read loop in DNS-over-TCP mode to
 * identify non-DNS UDP datagrams that must be dropped: without an upstream
 * UDP relay such a datagram can never be delivered, and handing it to lwIP
 * would only spawn a UDP session whose SOCKS5 ASSOCIATE is answered with an
 * empty BND. Returns 0 for a continuation fragment that carries no UDP header.
 */
int hev_dns_tcp_udp_dst_port (const unsigned char *ip, unsigned int len);

/**
 * Record a non-DNS UDP datagram dropped because the upstream relays no UDP.
 * Logs a per-port summary at most once per stats interval.
 */
void hev_dns_tcp_udp_drop (unsigned int dst_port);

/**
 * Dump and reset the drop counters, at most once per stats interval.
 * No-op when there is nothing to report.
 */
void hev_dns_tcp_stats_flush (void);

#endif /* __HEV_DNS_TCP_H__ */

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
 * Stop accepting new queries and abort in-flight workers. Idempotent.
 */
void hev_dns_tcp_stop (void);

/**
 * Inspect a raw IPv4 packet read from the tun device. When it is a plain
 * (unfragmented) UDP query to port 53 and the DNS-over-TCP mode is on, the
 * packet is consumed (1) and forwarded to the upstream SOCKS5 server as a TCP
 * CONNECT + DNS-over-TCP exchange; the answer is written back into the tun as
 * a UDP packet. Returns 0 when the packet is not handled and must go through
 * the normal lwIP path.
 */
int hev_dns_tcp_handle_packet (const unsigned char *ip, unsigned int len);

#endif /* __HEV_DNS_TCP_H__ */

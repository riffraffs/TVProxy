/*
 ============================================================================
 Name        : hev-fake-ip.h
 Author      : TVProxy
 Description : Local fake-ip DNS for UDP:53 and SOCKS5 domain lookup
 ============================================================================
 */

#ifndef __HEV_FAKE_IP_H__
#define __HEV_FAKE_IP_H__

#include <lwip/ip_addr.h>
#include <hev-socks5-proto.h>

/**
 * Reset the mapping table. Call once per hev_socks5_tunnel_init.
 */
void hev_fake_ip_init (void);

/**
 * Drop every mapping. Idempotent.
 */
void hev_fake_ip_fini (void);

/**
 * If @ip is a plain IPv4 UDP datagram to port 53, consume
 * it: answer A with a 198.18.0.0/16 address (AAAA and other types get a
 * NOERROR empty answer) and write the UDP reply back to tun. Returns 1 when
 * the packet must not go to lwIP, 0 when it is not a DNS query we handle.
 */
int hev_fake_ip_handle_packet (const unsigned char *ip, unsigned int len);

/**
 * @ipv4_n is IPv4 in network byte order. Returns 1 if it lies in 198.18.0.0/16.
 */
int hev_fake_ip_in_range (unsigned int ipv4_n);

/**
 * Look up the domain for a fake IPv4 (network order). Returns 0 and writes a
 * NUL-terminated name on hit, -1 on miss.
 */
int hev_fake_ip_lookup (unsigned int ipv4_n, char *name, int namelen);

/**
 * Map a tun destination to a SOCKS5 address. Addresses in 198.18.0.0/16
 * become ATYP=domain (lookup miss is -1 so the fake IP is never forwarded).
 * Other addresses stay IPv4/IPv6. Writes the domain into @name_out when
 * used (@name_out may be NULL). Returns 1 for domain, 0 for IP, -1 on failure.
 */
int hev_fake_ip_socks_addr (HevSocks5Addr *addr, const ip_addr_t *ip,
                            u16_t port, char *name_out, int name_len);

#endif /* __HEV_FAKE_IP_H__ */

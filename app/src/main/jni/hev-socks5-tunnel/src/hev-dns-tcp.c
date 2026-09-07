/*
 ============================================================================
 Name        : hev-dns-tcp.c
 Author      : TVProxy
 Description : When the upstream has no UDP relay (dns-over-tcp=true), drop
               IPv4 UDP at the tun read loop instead of handing it to lwIP.
               DNS itself is answered by fake-ip; this module only counts
               leftover datagrams (QUIC/NTP/mDNS/other).
 ============================================================================
 */

#include <string.h>
#include <time.h>

#include "hev-logger.h"

#include "hev-dns-tcp.h"

#define DNS_STATS_INTERVAL_MS 10000

static unsigned long long stats_last_flush;
static unsigned int stat_udp_drop_total;
static unsigned int stat_udp_drop_dns;
static unsigned int stat_udp_drop_quic;
static unsigned int stat_udp_drop_ntp;
static unsigned int stat_udp_drop_mdns;
static unsigned int stat_udp_drop_other;

static unsigned long long
now_ms (void)
{
    struct timespec ts;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
stats_flush (int force)
{
    unsigned long long now = now_ms ();

    if (!force && (now - stats_last_flush) < DNS_STATS_INTERVAL_MS)
        return;
    stats_last_flush = now;

    if (!stat_udp_drop_total)
        return;

    LOG_I ("[udp] tcp-only drop total=%u dns=%u quic443=%u ntp=%u mdns=%u "
           "other=%u",
           stat_udp_drop_total, stat_udp_drop_dns, stat_udp_drop_quic,
           stat_udp_drop_ntp, stat_udp_drop_mdns, stat_udp_drop_other);

    stat_udp_drop_total = 0;
    stat_udp_drop_dns = 0;
    stat_udp_drop_quic = 0;
    stat_udp_drop_ntp = 0;
    stat_udp_drop_mdns = 0;
    stat_udp_drop_other = 0;
}

void
hev_dns_tcp_init (void)
{
    stats_last_flush = 0;
    stat_udp_drop_total = 0;
    stat_udp_drop_dns = 0;
    stat_udp_drop_quic = 0;
    stat_udp_drop_ntp = 0;
    stat_udp_drop_mdns = 0;
    stat_udp_drop_other = 0;
}

void
hev_dns_tcp_stop (void)
{
    stats_flush (1);
}

int
hev_dns_tcp_udp_dst_port (const unsigned char *ip, unsigned int len)
{
    unsigned int ihl;
    unsigned int udp_off;

    if (len < 28)
        return -1;
    if ((ip[0] >> 4) != 4)
        return -1;

    ihl = (unsigned int)(ip[0] & 0x0f) * 4;
    if (ihl < 20 || ihl > len)
        return -1;
    if (ip[9] != 17)
        return -1;

    udp_off = ihl;
    if (udp_off + 8 > len)
        return 0;

    return ((unsigned int)ip[udp_off + 2] << 8) | ip[udp_off + 3];
}

void
hev_dns_tcp_udp_drop (unsigned int dst_port)
{
    stat_udp_drop_total++;
    switch (dst_port) {
    case 53:
        stat_udp_drop_dns++;
        break;
    case 443:
        stat_udp_drop_quic++;
        break;
    case 123:
        stat_udp_drop_ntp++;
        break;
    case 5353:
        stat_udp_drop_mdns++;
        break;
    default:
        stat_udp_drop_other++;
        break;
    }
}

void
hev_dns_tcp_stats_flush (void)
{
    stats_flush (0);
}

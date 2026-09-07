/*
 ============================================================================
 Name        : hev-dns-tcp.c
 Author      : TVProxy
 Description : DNS over TCP fallback for upstream SOCKS5 servers that do not
               relay UDP (e.g. iOS Loon LAN sharing, whose UDP ASSOCIATE
               reply has a zeroed BND.ADDR). Raw UDP queries to port 53 read
               from the tun are forwarded to the same destination via an
               upstream SOCKS5 TCP CONNECT, and the answer is written back
               into the tun as a plain UDP packet.

               While this mode is on every other UDP datagram is dropped at
               the tun read loop instead of being handed to lwIP: with no UDP
               relay it can never be delivered, and a lwIP UDP session would
               only open a doomed SOCKS5 UDP ASSOCIATE against the upstream
               (connect to 0.0.0.0:0), churning one TCP connection per
               datagram flow for nothing.
 ============================================================================
 */

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include "hev-logger.h"
#include "hev-config.h"
#include "hev-socks5-tunnel.h"

#include "hev-dns-tcp.h"

#define DNS_QUERY_MAX 1400
#define DNS_PAYLOAD_MAX 1472 /* 1500 MTU - 20 (ip) - 8 (udp) */
#define DNS_INFLIGHT_MAX 8
#define DNS_PENDING_MAX 32 /* bounded queue for bursts beyond in-flight */
#define DNS_CONNECT_TIMEOUT 5000
#define DNS_IO_TIMEOUT 8000
#define DNS_STATS_INTERVAL_MS 10000

typedef struct _HevDnsQuery HevDnsQuery;

struct _HevDnsQuery
{
    HevDnsQuery *next; /* pending queue link */
    unsigned char query[DNS_QUERY_MAX];
    unsigned int qlen;
    struct in_addr src;
    struct in_addr dns;
    unsigned short src_port;
    char qname[64]; /* first question name, for diagnostics */
    unsigned long long t_enqueue; /* monotonic ms */
};

static int stop;
static unsigned int inflight;
static unsigned short ip_id_counter;

static HevDnsQuery *pending_head;
static HevDnsQuery *pending_tail;
static unsigned int pending_count;

static unsigned long long stats_last_flush;
static unsigned int stat_udp_drop_total;
static unsigned int stat_udp_drop_dns; /* dst :53 that was not consumed */
static unsigned int stat_udp_drop_quic; /* dst :443 */
static unsigned int stat_udp_drop_ntp; /* dst :123 */
static unsigned int stat_udp_drop_mdns; /* dst :5353 */
static unsigned int stat_udp_drop_other;
static unsigned int stat_dns_drop_full; /* queue full, query dropped */

static unsigned long long
now_ms (void)
{
    struct timespec ts;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int
io_yielder (HevTaskYieldType type, void *data)
{
    int *timeout = data;

    if (stop)
        return -1;

    if (type == HEV_TASK_YIELD) {
        hev_task_yield (HEV_TASK_YIELD);
        return 0;
    }

    if (*timeout < 0) {
        hev_task_yield (HEV_TASK_WAITIO);
        return 0;
    }

    *timeout = hev_task_sleep ((unsigned int)*timeout);
    if (*timeout <= 0)
        return -1;

    return 0;
}

static int
read_full (int fd, unsigned char *buf, unsigned int len)
{
    unsigned int off = 0;

    while (off < len) {
        ssize_t res;
        int timeout = DNS_IO_TIMEOUT;

        res = hev_task_io_socket_recv (fd, buf + off, len - off, 0,
                                       io_yielder, &timeout);
        if (res <= 0)
            return -1;
        off += (unsigned int)res;
    }

    return 0;
}

static int
write_full (int fd, const unsigned char *buf, unsigned int len)
{
    unsigned int off = 0;

    while (off < len) {
        ssize_t res;
        int timeout = DNS_IO_TIMEOUT;

        res = hev_task_io_socket_send (fd, buf + off, len - off, 0,
                                       io_yielder, &timeout);
        if (res <= 0)
            return -1;
        off += (unsigned int)res;
    }

    return 0;
}

static int
server_sockaddr (struct sockaddr_storage *ss, socklen_t *slen)
{
    HevConfigServer *srv = hev_config_get_socks5_server ();
    struct sockaddr_in *sin4 = (struct sockaddr_in *)ss;
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ss;

    memset (ss, 0, sizeof (*ss));

    if (inet_pton (AF_INET, srv->addr, &sin4->sin_addr) == 1) {
        sin4->sin_family = AF_INET;
        sin4->sin_port = htons (srv->port);
        *slen = sizeof (*sin4);
        return AF_INET;
    }

    if (inet_pton (AF_INET6, srv->addr, &sin6->sin6_addr) == 1) {
        sin6->sin6_family = AF_INET6;
        sin6->sin6_port = htons (srv->port);
        *slen = sizeof (*sin6);
        return AF_INET6;
    }

    return -1;
}

static int
dns_connect (const struct sockaddr *sa, socklen_t slen)
{
    int timeout = DNS_CONNECT_TIMEOUT;
    int fd, res, zero = 0;

    fd = hev_task_io_socket_socket (sa->sa_family, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    if (sa->sa_family == AF_INET6)
        setsockopt (fd, IPPROTO_IPV6, IPV6_V6ONLY, &zero, sizeof (zero));

    res = hev_task_add_fd (hev_task_self (), fd, POLLIN | POLLOUT);
    if (res < 0)
        hev_task_mod_fd (hev_task_self (), fd, POLLIN | POLLOUT);

    res = hev_task_io_socket_connect (fd, sa, slen, io_yielder, &timeout);
    if (res < 0) {
        hev_task_del_fd (hev_task_self (), fd);
        close (fd);
        return -1;
    }

    return fd;
}

static int
socks5_connect (int fd, const struct in_addr *dns_ip)
{
    const unsigned char hello[3] = { 0x05, 0x01, 0x00 };
    unsigned char buf[32];
    unsigned char req[10];
    unsigned int extra;

    if (write_full (fd, hello, sizeof (hello)) < 0)
        return -1;
    if (read_full (fd, buf, 2) < 0)
        return -1;
    if (buf[0] != 0x05 || buf[1] != 0x00)
        return -1; /* no no-auth method */

    req[0] = 0x05; /* CONNECT */
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = 0x01; /* ATYP IPv4 */
    memcpy (req + 4, dns_ip, 4);
    req[8] = 0x00; /* port 53 */
    req[9] = 0x35;
    if (write_full (fd, req, sizeof (req)) < 0)
        return -1;
    if (read_full (fd, buf, 4) < 0)
        return -1;
    if (buf[0] != 0x05 || buf[1] != 0x00)
        return -1;

    extra = 0;
    switch (buf[3]) {
    case 0x01:
        extra = 6;
        break;
    case 0x04:
        extra = 18;
        break;
    case 0x03:
        if (read_full (fd, buf, 1) < 0)
            return -1;
        extra = buf[0];
        break;
    default:
        return -1;
    }
    while (extra) {
        unsigned int n = extra > sizeof (buf) ? sizeof (buf) : extra;
        if (read_full (fd, buf, n) < 0)
            return -1;
        extra -= n;
    }

    return 0;
}

static int
dns_over_tcp_exchange (int fd, const unsigned char *query,
                       unsigned int qlen, unsigned char *ans,
                       unsigned int *anslen)
{
    unsigned char lenb[2];

    lenb[0] = (unsigned char)(qlen >> 8);
    lenb[1] = (unsigned char)(qlen & 0xff);
    if (write_full (fd, lenb, 2) < 0)
        return -1;
    if (write_full (fd, query, qlen) < 0)
        return -1;
    if (read_full (fd, lenb, 2) < 0)
        return -1;

    *anslen = ((unsigned int)lenb[0] << 8) | lenb[1];
    if (*anslen > DNS_PAYLOAD_MAX)
        return -1;

    return read_full (fd, ans, *anslen);
}

static unsigned short
ip_checksum (const void *buf, unsigned int len)
{
    const unsigned char *p = buf;
    unsigned long sum = 0;

    while (len > 1) {
        sum += ((unsigned int)p[0] << 8) | p[1];
        p += 2;
        len -= 2;
    }
    if (len)
        sum += (unsigned int)p[0] << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);

    return (unsigned short)~sum;
}

static int
build_dns_reply (const HevDnsQuery *q, const unsigned char *ans,
                 unsigned int anslen, unsigned char *out)
{
    unsigned int total = 20 + 8 + anslen;
    unsigned int udp = 20;
    unsigned short id = ++ip_id_counter;
    unsigned short sum;

    if (anslen > DNS_PAYLOAD_MAX)
        return -1;

    memset (out, 0, 20 + 8);
    out[0] = 0x45;
    out[2] = (unsigned char)(total >> 8);
    out[3] = (unsigned char)(total & 0xff);
    out[4] = (unsigned char)(id >> 8);
    out[5] = (unsigned char)(id & 0xff);
    out[8] = 64;
    out[9] = 17; /* UDP */
    memcpy (out + 12, &q->dns, 4);
    memcpy (out + 16, &q->src, 4);
    sum = ip_checksum (out, 20);
    out[10] = (unsigned char)(sum >> 8);
    out[11] = (unsigned char)(sum & 0xff);

    out[udp + 0] = 0x00; /* sport 53 */
    out[udp + 1] = 0x35;
    out[udp + 2] = (unsigned char)(q->src_port >> 8);
    out[udp + 3] = (unsigned char)(q->src_port & 0xff);
    out[udp + 4] = (unsigned char)((8 + anslen) >> 8);
    out[udp + 5] = (unsigned char)((8 + anslen) & 0xff);
    out[udp + 6] = 0x00; /* UDP checksum 0 is legal in IPv4 */
    out[udp + 7] = 0x00;
    memcpy (out + udp + 8, ans, anslen);

    return (int)total;
}

static void
pending_push (HevDnsQuery *q)
{
    q->next = NULL;
    if (pending_tail)
        pending_tail->next = q;
    else
        pending_head = q;
    pending_tail = q;
    pending_count++;
}

static HevDnsQuery *
pending_pop (void)
{
    HevDnsQuery *q = pending_head;

    if (q) {
        pending_head = q->next;
        if (!pending_head)
            pending_tail = NULL;
        pending_count--;
        q->next = NULL;
    }

    return q;
}

static void
pending_clear (void)
{
    HevDnsQuery *q;

    while ((q = pending_pop ()))
        free (q);
}

static void
stats_flush (int force)
{
    unsigned long long now = now_ms ();

    if (!force && (now - stats_last_flush) < DNS_STATS_INTERVAL_MS)
        return;
    stats_last_flush = now;

    if (!stat_udp_drop_total && !stat_dns_drop_full)
        return;

    LOG_I ("[udp] tcp-only drop total=%u dns=%u quic443=%u ntp=%u mdns=%u "
           "other=%u dns-qfull=%u",
           stat_udp_drop_total, stat_udp_drop_dns, stat_udp_drop_quic,
           stat_udp_drop_ntp, stat_udp_drop_mdns, stat_udp_drop_other,
           stat_dns_drop_full);

    stat_udp_drop_total = 0;
    stat_udp_drop_dns = 0;
    stat_udp_drop_quic = 0;
    stat_udp_drop_ntp = 0;
    stat_udp_drop_mdns = 0;
    stat_udp_drop_other = 0;
    stat_dns_drop_full = 0;
}

static void
worker_entry (void *data)
{
    HevDnsQuery *q = data;

    for (;;) {
        struct sockaddr_storage ss;
        socklen_t slen;
        unsigned char ans[DNS_PAYLOAD_MAX];
        unsigned char out[1500];
        unsigned char *ip4;
        unsigned int anslen = 0;
        unsigned int outlen;
        unsigned long long t1, wait, cost;
        char ipbuf[16];
        int fd = -1;
        int ok = 0;

        t1 = now_ms ();
        wait = t1 - q->t_enqueue;

        if (!stop && server_sockaddr (&ss, &slen) >= 0)
            fd = dns_connect ((struct sockaddr *)&ss, slen);

        if (fd >= 0 && socks5_connect (fd, &q->dns) == 0 &&
            dns_over_tcp_exchange (fd, q->query, q->qlen, ans, &anslen) == 0) {
            outlen = (unsigned int)build_dns_reply (q, ans, anslen, out);
            if (outlen > 0 && !stop &&
                hev_socks5_tunnel_write_packet (out, outlen) == (int)outlen)
                ok = 1;
        }

        if (fd >= 0) {
            hev_task_del_fd (hev_task_self (), fd);
            close (fd);
        }

        ip4 = (unsigned char *)&q->dns.s_addr;
        snprintf (ipbuf, sizeof (ipbuf), "%u.%u.%u.%u", ip4[0], ip4[1],
                  ip4[2], ip4[3]);
        cost = now_ms () - t1;
        LOG_I ("[dns] tcp %s q=%s %s cost=%llu ms wait=%llu ms", ipbuf,
               q->qname[0] ? q->qname : "?", ok ? "ok" : "fail", cost, wait);

        free (q);
        q = pending_pop ();
        if (!q)
            break;
    }

    if (inflight)
        inflight--;
}

static int
parse_dns_query (const unsigned char *ip, unsigned int len,
                 HevDnsQuery *q)
{
    const unsigned char *udp;
    unsigned int ihl;
    unsigned int udp_off;
    unsigned int udp_len;
    unsigned int qlen;
    unsigned int pos;
    unsigned int out;

    if (len < 28)
        return -1;
    if ((ip[0] >> 4) != 4)
        return -1;

    ihl = (unsigned int)(ip[0] & 0x0f) * 4;
    if (ihl < 20 || ihl > len)
        return -1;
    if (ip[9] != 17) /* UDP */
        return -1;
    /* skip fragments (MF or nonzero offset) */
    if ((ip[6] & 0x20) || (((ip[6] & 0x1f) << 8) | ip[7]))
        return -1;

    udp_off = ihl;
    if (udp_off + 8 > len)
        return -1;
    udp = ip + udp_off;

    if ((((unsigned int)udp[2]) << 8 | udp[3]) != 53)
        return -1;
    udp_len = (((unsigned int)udp[4]) << 8) | udp[5];
    if (udp_len < 12)
        return -1;
    if (udp_len - 8 > len - udp_off - 8)
        return -1;
    qlen = udp_len - 8;
    if (udp[8] & 0x80) /* QR=1: a response, not ours */
        return -1;
    if (qlen > DNS_QUERY_MAX)
        return -1;

    q->qlen = qlen;
    memcpy (q->query, udp + 8, qlen);
    memcpy (&q->src, ip + 12, 4);
    memcpy (&q->dns, ip + 16, 4);
    q->src_port = (unsigned short)(((unsigned int)udp[0] << 8) | udp[1]);

    /* extract the first question name for diagnostics: 12-byte DNS header,
       then length-prefixed labels until a zero octet */
    q->qname[0] = 0;
    pos = 0;
    out = 0;
    if (qlen >= 12) {
        for (;;) {
            unsigned int label = udp[8 + 12 + pos];
            unsigned int i;

            if (label == 0)
                break; /* root terminator: name complete */
            if (label > 63) /* compression pointer / junk: give up */
                break;
            if (pos + 1 + label > qlen - 12)
                break;
            if (out && out < sizeof (q->qname) - 1)
                q->qname[out++] = '.';
            for (i = 0; i < label && out < sizeof (q->qname) - 1; i++)
                q->qname[out++] = (char)udp[8 + 12 + pos + 1 + i];
            pos += 1 + label;
        }
    }
    q->qname[out] = 0;

    return 0;
}

void
hev_dns_tcp_init (void)
{
    stop = 0;
    inflight = 0;
    ip_id_counter = 0;
    pending_head = NULL;
    pending_tail = NULL;
    pending_count = 0;
    stats_last_flush = 0;
    stat_udp_drop_total = 0;
    stat_udp_drop_dns = 0;
    stat_udp_drop_quic = 0;
    stat_udp_drop_ntp = 0;
    stat_udp_drop_mdns = 0;
    stat_udp_drop_other = 0;
    stat_dns_drop_full = 0;
}

void
hev_dns_tcp_stop (void)
{
    stop = 1;
    pending_clear ();
    stats_flush (1);
}

int
hev_dns_tcp_handle_packet (const unsigned char *ip, unsigned int len)
{
    HevDnsQuery *q;
    HevTask *task;
    int stack;

    if (stop)
        return 0;
    if (!hev_config_get_misc_dns_over_tcp ())
        return 0;

    q = malloc (sizeof (*q));
    if (!q) {
        LOG_W ("[dns] tcp alloc query failed");
        return 1;
    }
    if (parse_dns_query (ip, len, q) < 0) {
        free (q);
        return 0;
    }

    q->next = NULL;
    q->t_enqueue = now_ms ();

    if (inflight < DNS_INFLIGHT_MAX) {
        stack = hev_config_get_misc_task_stack_size ();
        task = hev_task_new (stack);
        if (!task) {
            free (q);
            return 1;
        }

        inflight++;
        hev_task_run (task, worker_entry, q);
        hev_task_wakeup (task);
    } else if (pending_count < DNS_PENDING_MAX) {
        pending_push (q);
    } else {
        if (!stat_dns_drop_full)
            LOG_W ("[dns] tcp queue full, dropping queries");
        stat_dns_drop_full++;
        free (q);
        stats_flush (0);
    }

    return 1;
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
    if (ip[9] != 17) /* UDP */
        return -1;

    udp_off = ihl;
    if (udp_off + 8 > len)
        return 0; /* continuation fragment: no UDP header, still UDP */

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

/*
 ============================================================================
 Name        : hev-fake-ip.c
 Author      : TVProxy
 Description : Answer tun UDP:53 locally with 198.18.0.0/16 fake addresses so
               SOCKS5 sessions can send ATYP=domain to the upstream (Clash /
               Loon). Fake IPs never leave this process.
 ============================================================================
 */

#include <string.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <hev-socks5-misc.h>

#include "hev-logger.h"
#include "hev-socks5-tunnel.h"
#include "hev-utils.h"

#include "hev-fake-ip.h"

#define FAKE_CAP 1024
#define FAKE_NAME 256
#define FAKE_TTL 60
#define FAKE_NET 0xc6120000u /* 198.18.0.0 */
#define DNS_PAYLOAD_MAX 1472
#define DNS_QUERY_MAX 1400

typedef struct _HevFakeSlot HevFakeSlot;

struct _HevFakeSlot
{
    unsigned int ip_n; /* network order, 0 = empty */
    char name[FAKE_NAME];
};

static HevFakeSlot table[FAKE_CAP];
static unsigned int alloc_cursor;
static unsigned int host_seq;
static unsigned short ip_id_counter;

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

static void
name_lower (char *s)
{
    for (; *s; s++) {
        if (*s >= 'A' && *s <= 'Z')
            *s = (char)(*s - 'A' + 'a');
    }
}

static int
parse_qname (const unsigned char *dns, unsigned int qlen, char *name,
             int namelen, unsigned int *qend)
{
    unsigned int pos = 12;
    unsigned int out = 0;

    if (qlen < 12 + 4)
        return -1;
    if (namelen < 2)
        return -1;
    name[0] = 0;

    for (;;) {
        unsigned int label;
        unsigned int i;

        if (pos >= qlen)
            return -1;
        label = dns[pos];
        if (label == 0) {
            pos++;
            break;
        }
        if (label > 63)
            return -1; /* compression / reserved */
        if (pos + 1 + label >= qlen)
            return -1;
        if (out && out < (unsigned int)namelen - 1)
            name[out++] = '.';
        for (i = 0; i < label && out < (unsigned int)namelen - 1; i++)
            name[out++] = (char)dns[pos + 1 + i];
        pos += 1 + label;
    }

    if (pos + 4 > qlen)
        return -1;
    name[out] = 0;
    name_lower (name);
    *qend = pos;
    return 0;
}

static unsigned int
alloc_or_reuse (const char *name)
{
    unsigned int i;
    unsigned int host;
    unsigned int idx;

    if (!name[0])
        return 0;

    for (i = 0; i < FAKE_CAP; i++) {
        if (table[i].ip_n && strcmp (table[i].name, name) == 0)
            return table[i].ip_n;
    }

    idx = alloc_cursor % FAKE_CAP;
    alloc_cursor++;
    host = 1u + (host_seq++ % 65534u);
    table[idx].ip_n = htonl (FAKE_NET | host);
    strncpy (table[idx].name, name, FAKE_NAME - 1);
    table[idx].name[FAKE_NAME - 1] = 0;
    return table[idx].ip_n;
}

static int
build_dns_payload (const unsigned char *query, unsigned int qlen,
                   unsigned int qend, int ancount, unsigned int rdata_n,
                   unsigned char *ans, unsigned int *anslen)
{
    unsigned int qsec = qend + 4;
    unsigned int off;

    if (qsec > qlen || qsec > DNS_PAYLOAD_MAX)
        return -1;

    memcpy (ans, query, qsec);
    ans[2] = (unsigned char)((query[2] & 0x01) | 0x80); /* QR=1, copy RD */
    ans[3] = 0x80; /* RA=1, RCODE=0 */
    ans[4] = 0;
    ans[5] = 1; /* QDCOUNT=1 */
    ans[6] = 0;
    ans[7] = (unsigned char)ancount;
    ans[8] = 0;
    ans[9] = 0;
    ans[10] = 0;
    ans[11] = 0;
    off = qsec;

    if (ancount) {
        if (off + 16 > DNS_PAYLOAD_MAX)
            return -1;
        ans[off++] = 0xc0;
        ans[off++] = 0x0c;
        ans[off++] = 0x00;
        ans[off++] = 0x01; /* A */
        ans[off++] = 0x00;
        ans[off++] = 0x01; /* IN */
        ans[off++] = 0;
        ans[off++] = 0;
        ans[off++] = 0;
        ans[off++] = FAKE_TTL;
        ans[off++] = 0;
        ans[off++] = 4;
        memcpy (ans + off, &rdata_n, 4);
        off += 4;
    }

    *anslen = off;
    return 0;
}

static int
build_udp_reply (const unsigned char *ip, const unsigned char *udp,
                 const unsigned char *ans, unsigned int anslen,
                 unsigned char *out)
{
    unsigned int total = 20 + 8 + anslen;
    unsigned short id = ++ip_id_counter;
    unsigned short sum;
    unsigned int udp_off = 20;

    if (anslen > DNS_PAYLOAD_MAX)
        return -1;

    memset (out, 0, 20 + 8);
    out[0] = 0x45;
    out[2] = (unsigned char)(total >> 8);
    out[3] = (unsigned char)(total & 0xff);
    out[4] = (unsigned char)(id >> 8);
    out[5] = (unsigned char)(id & 0xff);
    out[8] = 64;
    out[9] = 17;
    memcpy (out + 12, ip + 16, 4); /* src = original dest (resolver) */
    memcpy (out + 16, ip + 12, 4); /* dest = original src */
    sum = ip_checksum (out, 20);
    out[10] = (unsigned char)(sum >> 8);
    out[11] = (unsigned char)(sum & 0xff);

    out[udp_off + 0] = 0x00; /* sport 53 */
    out[udp_off + 1] = 0x35;
    out[udp_off + 2] = udp[0];
    out[udp_off + 3] = udp[1];
    out[udp_off + 4] = (unsigned char)((8 + anslen) >> 8);
    out[udp_off + 5] = (unsigned char)((8 + anslen) & 0xff);
    memcpy (out + udp_off + 8, ans, anslen);

    return (int)total;
}

void
hev_fake_ip_init (void)
{
    memset (table, 0, sizeof (table));
    alloc_cursor = 0;
    host_seq = 0;
    ip_id_counter = 0;
}

void
hev_fake_ip_fini (void)
{
    memset (table, 0, sizeof (table));
}

int
hev_fake_ip_in_range (unsigned int ipv4_n)
{
    return (ntohl (ipv4_n) & 0xffff0000u) == FAKE_NET;
}

int
hev_fake_ip_lookup (unsigned int ipv4_n, char *name, int namelen)
{
    unsigned int i;

    if (!name || namelen < 2)
        return -1;
    for (i = 0; i < FAKE_CAP; i++) {
        if (table[i].ip_n == ipv4_n) {
            strncpy (name, table[i].name, (size_t)namelen - 1);
            name[namelen - 1] = 0;
            return 0;
        }
    }
    return -1;
}

int
hev_fake_ip_socks_addr (HevSocks5Addr *addr, const ip_addr_t *ip, u16_t port,
                        char *name_out, int name_len)
{
    if (IP_IS_V4 (ip)) {
        unsigned int a = ip4_addr_get_u32 (ip_2_ip4 (ip));
        if (hev_fake_ip_in_range (a)) {
            char local[FAKE_NAME];
            char *out = local;
            int outlen = (int)sizeof (local);

            if (name_out && name_len > 1) {
                out = name_out;
                outlen = name_len;
            } else if (name_out && name_len > 0) {
                name_out[0] = 0;
            }
            if (hev_fake_ip_lookup (a, out, outlen) < 0)
                return -1;
            hev_socks5_addr_from_name (addr, out, htons (port));
            return 1;
        }
    }
    if (name_out && name_len > 0)
        name_out[0] = 0;
    if (hev_socks5_addr_from_lwip (addr, ip, port) < 0)
        return -1;
    return 0;
}

int
hev_fake_ip_handle_packet (const unsigned char *ip, unsigned int len)
{
    const unsigned char *udp;
    unsigned char ans[DNS_PAYLOAD_MAX];
    unsigned char out[20 + 8 + DNS_PAYLOAD_MAX];
    char qname[FAKE_NAME];
    unsigned int ihl, udp_off, udp_len, qlen, qend;
    unsigned int qtype, fake_n;
    unsigned int anslen;
    int ancount, outlen;
    unsigned a;

    if (len < 28)
        return 0;
    if ((ip[0] >> 4) != 4)
        return 0;

    ihl = (unsigned int)(ip[0] & 0x0f) * 4;
    if (ihl < 20 || ihl > len)
        return 0;
    if (ip[9] != 17)
        return 0;
    if ((ip[6] & 0x20) || (((ip[6] & 0x1f) << 8) | ip[7]))
        return 0;

    udp_off = ihl;
    if (udp_off + 8 > len)
        return 0;
    udp = ip + udp_off;
    if ((((unsigned int)udp[2] << 8) | udp[3]) != 53)
        return 0;

    udp_len = ((unsigned int)udp[4] << 8) | udp[5];
    if (udp_len < 12 || udp_len - 8 > len - udp_off - 8)
        return 1; /* dport 53 but junk: drop */
    qlen = udp_len - 8;
    if (qlen > DNS_QUERY_MAX)
        return 1;
    if (udp[8] & 0x80)
        return 1; /* DNS response to :53, drop */

    if (parse_qname (udp + 8, qlen, qname, sizeof (qname), &qend) < 0) {
        LOG_W ("[dns] fake malformed query");
        return 1;
    }

    qtype = ((unsigned int)udp[8 + qend] << 8) | udp[8 + qend + 1];
    fake_n = 0;
    ancount = 0;
    if (qtype == 1 && qname[0]) {
        fake_n = alloc_or_reuse (qname);
        if (fake_n)
            ancount = 1;
    }

    if (build_dns_payload (udp + 8, qlen, qend, ancount, fake_n, ans, &anslen) <
        0)
        return 1;

    outlen = build_udp_reply (ip, udp, ans, anslen, out);
    if (outlen < 0)
        return 1;
    hev_socks5_tunnel_write_packet (out, (size_t)outlen);

    if (ancount) {
        a = ntohl (fake_n);
        LOG_I ("[dns] fake q=%s ip=%u.%u.%u.%u", qname, (a >> 24) & 0xff,
               (a >> 16) & 0xff, (a >> 8) & 0xff, a & 0xff);
    } else {
        LOG_D ("[dns] fake q=%s type=%u empty", qname[0] ? qname : "?", qtype);
    }

    return 1;
}

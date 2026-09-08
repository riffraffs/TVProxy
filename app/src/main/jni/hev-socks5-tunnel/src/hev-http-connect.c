/*
 ============================================================================
 Name        : hev-http-connect.c
 Author      : TVProxy
 Description : HTTP CONNECT handshake (no auth). Replaces SOCKS5 handshake
               after hev_socks5_client_connect; splice is unchanged.
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <hev-task.h>
#include <hev-task-io.h>
#include <hev-task-io-socket.h>

#include <hev-socks5-misc.h>

#include "hev-logger.h"

#include "hev-http-connect.h"

#define HTTP_REQ_MAX 768
#define HTTP_RSP_MAX 4096

#define task_io_yielder hev_socks5_task_io_yielder

static int
addr_host_port (const HevSocks5Addr *addr, char *host, int hostlen,
                unsigned int *port)
{
    uint16_t nport;

    if (!addr || !host || hostlen < 8 || !port)
        return -1;

    switch (addr->atype) {
    case HEV_SOCKS5_ADDR_TYPE_NAME:
        if (addr->domain.len == 0 || (int)addr->domain.len >= hostlen)
            return -1;
        memcpy (host, addr->domain.addr, addr->domain.len);
        host[addr->domain.len] = 0;
        memcpy (&nport, addr->domain.addr + addr->domain.len, 2);
        *port = ntohs (nport);
        return 0;
    case HEV_SOCKS5_ADDR_TYPE_IPV4:
        if (!inet_ntop (AF_INET, addr->ipv4.addr, host, (socklen_t)hostlen))
            return -1;
        *port = ntohs (addr->ipv4.port);
        return 0;
    default:
        LOG_W ("[http] connect ipv6 unsupported");
        return -1;
    }
}

static int
http_status_ok (const char *buf, unsigned int n)
{
    unsigned int i;
    unsigned int code;

    if (n < 12)
        return -1;
    if (0 != strncmp (buf, "HTTP/1.0 ", 9) &&
        0 != strncmp (buf, "HTTP/1.1 ", 9)) {
        LOG_W ("[http] connect bad status line");
        return -1;
    }

    code = 0;
    for (i = 9; i < n && buf[i] >= '0' && buf[i] <= '9'; i++)
        code = code * 10u + (unsigned int)(buf[i] - '0');
    if (i == 9) {
        LOG_W ("[http] connect missing status");
        return -1;
    }
    if (code == 200)
        return 0;
    if (code == 407)
        LOG_W ("[http] connect 407 (auth unsupported)");
    else
        LOG_W ("[http] connect status %u", code);
    return -1;
}

int
hev_http_connect (HevSocks5 *self, const HevSocks5Addr *addr)
{
    char host[256];
    char req[HTTP_REQ_MAX];
    char rsp[HTTP_RSP_MAX];
    unsigned int port;
    unsigned int n;
    int fd;
    int len;
    int res;

    if (!self || self->fd < 0)
        return -1;

    if (addr_host_port (addr, host, (int)sizeof (host), &port) < 0)
        return -1;

    LOG_I ("[http] connect %s:%u", host, port);

    len = snprintf (req, sizeof (req),
                    "CONNECT %s:%u HTTP/1.1\r\n"
                    "Host: %s:%u\r\n"
                    "\r\n",
                    host, port, host, port);
    if (len <= 0 || len >= (int)sizeof (req)) {
        LOG_E ("[http] connect request too long");
        return -1;
    }

    fd = self->fd;
    res = hev_task_io_socket_send (fd, req, (size_t)len, MSG_WAITALL,
                                   task_io_yielder, self);
    if (res <= 0) {
        LOG_E ("[http] connect send");
        return -1;
    }

    n = 0;
    while (n < HTTP_RSP_MAX) {
        res = hev_task_io_socket_recv (fd, rsp + n, 1, MSG_WAITALL,
                                       task_io_yielder, self);
        if (res <= 0) {
            LOG_E ("[http] connect recv");
            return -1;
        }
        n += 1;
        if (n >= 4 && rsp[n - 4] == '\r' && rsp[n - 3] == '\n' &&
            rsp[n - 2] == '\r' && rsp[n - 1] == '\n')
            break;
    }
    if (n < 4 || rsp[n - 4] != '\r' || rsp[n - 3] != '\n' ||
        rsp[n - 2] != '\r' || rsp[n - 1] != '\n') {
        LOG_E ("[http] connect header truncated");
        return -1;
    }

    return http_status_ok (rsp, n);
}

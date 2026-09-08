/*
 ============================================================================
 Name        : hev-http-connect.h
 Author      : TVProxy
 Description : HTTP CONNECT handshake after TCP connect to the upstream
 ============================================================================
 */

#ifndef __HEV_HTTP_CONNECT_H__
#define __HEV_HTTP_CONNECT_H__

#include <hev-socks5.h>
#include <hev-socks5-proto.h>

/**
 * Send CONNECT <host>:<port> and wait for HTTP/1.x 200.
 * Handshake buffers live on the coroutine stack. Returns 0 on success,
 * -1 on 407 / non-200 / IPv6 / truncated response. Does not consume
 * bytes after the header terminator.
 */
int hev_http_connect (HevSocks5 *self, const HevSocks5Addr *addr);

#endif /* __HEV_HTTP_CONNECT_H__ */

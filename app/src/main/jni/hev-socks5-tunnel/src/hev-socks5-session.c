/*
 ============================================================================
 Name        : hev-socks5-session.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2017 - 2023 hev
 Description : Socks5 Session
 ============================================================================
 */

#include <string.h>

#include "hev-logger.h"
#include "hev-config.h"
#include "hev-socks5-client.h"
#include "hev-socks5-client-tcp.h"
#include "hev-http-connect.h"

#include "hev-socks5-session.h"

void
hev_socks5_session_run (HevSocks5Session *self)
{
    HevSocks5SessionIface *iface;
    HevConfigServer *srv;
    int read_write_timeout;
    int connect_timeout;
    int res;

    LOG_D ("%p socks5 session run", self);

    srv = hev_config_get_socks5_server ();
    connect_timeout = hev_config_get_misc_connect_timeout ();
    read_write_timeout = hev_config_get_misc_read_write_timeout ();

    hev_socks5_set_timeout (HEV_SOCKS5 (self), connect_timeout);

    res = hev_socks5_client_connect (HEV_SOCKS5_CLIENT (self), srv->addr,
                                     srv->port);
    if (res < 0) {
        LOG_E ("%p socks5 session connect", self);
        return;
    }

    hev_socks5_set_timeout (HEV_SOCKS5 (self), read_write_timeout);

    if (hev_config_get_upstream_protocol () == HEV_CONFIG_UPSTREAM_HTTP) {
        HevSocks5Addr *addr;

        if (HEV_SOCKS5 (self)->type != HEV_SOCKS5_TYPE_TCP) {
            LOG_D ("%p http skip non-tcp", self);
            return;
        }
        addr = HEV_SOCKS5_CLIENT_TCP (self)->addr;
        if (!addr) {
            LOG_E ("%p http session no addr", self);
            return;
        }
        res = hev_http_connect (HEV_SOCKS5 (self), addr);
        if (res < 0) {
            LOG_E ("%p http session handshake", self);
            return;
        }
    } else {
        if (srv->user && srv->pass) {
            hev_socks5_client_set_auth (HEV_SOCKS5_CLIENT (self), srv->user,
                                        srv->pass);
            LOG_D ("%p socks5 client auth %s:%s", self, srv->user, srv->pass);
        }

        res = hev_socks5_client_handshake (HEV_SOCKS5_CLIENT (self),
                                           srv->pipeline);
        if (res < 0) {
            LOG_E ("%p socks5 session handshake", self);
            return;
        }
    }

    iface = HEV_OBJECT_GET_IFACE (self, HEV_SOCKS5_SESSION_TYPE);
    iface->splicer (self);
}

void
hev_socks5_session_terminate (HevSocks5Session *self)
{
    HevSocks5SessionIface *iface;

    LOG_D ("%p socks5 session terminate", self);

    iface = HEV_OBJECT_GET_IFACE (self, HEV_SOCKS5_SESSION_TYPE);
    hev_socks5_set_timeout (HEV_SOCKS5 (self), 0);
    hev_task_wakeup (iface->get_task (self));
}

void
hev_socks5_session_set_task (HevSocks5Session *self, HevTask *task)
{
    HevSocks5SessionIface *iface;

    iface = HEV_OBJECT_GET_IFACE (self, HEV_SOCKS5_SESSION_TYPE);
    iface->set_task (self, task);
}

HevListNode *
hev_socks5_session_get_node (HevSocks5Session *self)
{
    HevSocks5SessionIface *iface;

    iface = HEV_OBJECT_GET_IFACE (self, HEV_SOCKS5_SESSION_TYPE);
    return iface->get_node (self);
}

void *
hev_socks5_session_iface (void)
{
    static char type;

    return &type;
}

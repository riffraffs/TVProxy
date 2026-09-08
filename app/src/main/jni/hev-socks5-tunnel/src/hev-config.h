/*
 ============================================================================
 Name        : hev-config.h
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2019 - 2023 hev
 Description : Config
 ============================================================================
 */

#ifndef __HEV_CONFIG_H__
#define __HEV_CONFIG_H__

typedef struct _HevConfigServer HevConfigServer;

struct _HevConfigServer
{
    const char *user;
    const char *pass;
    unsigned int mark;
    short udp_in_udp;
    unsigned short port;
    unsigned char pipeline;
    char addr[256];
};

#define HEV_CONFIG_UPSTREAM_SOCKS5 0
#define HEV_CONFIG_UPSTREAM_HTTP 1

int hev_config_init_from_file (const char *config_path);
void hev_config_fini (void);

unsigned int hev_config_get_tunnel_mtu (void);

HevConfigServer *hev_config_get_socks5_server (void);

int hev_config_get_upstream_protocol (void);

int hev_config_get_misc_task_stack_size (void);
int hev_config_get_misc_tcp_buffer_size (void);
int hev_config_get_misc_connect_timeout (void);
int hev_config_get_misc_read_write_timeout (void);
const char *hev_config_get_misc_log_file (void);
int hev_config_get_misc_log_level (void);

#endif /* __HEV_CONFIG_H__ */

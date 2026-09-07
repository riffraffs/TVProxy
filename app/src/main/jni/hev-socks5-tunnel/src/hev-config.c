/*
 ============================================================================
 Name        : hev-config.c
 Author      : hev <r@hev.cc>
 Copyright   : Copyright (c) 2019 - 2024 hev
 Description : Config (TVProxy: trimmed key=value variant, no libyaml)
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <lwip/tcp.h>

#include "hev-logger.h"
#include "hev-config.h"
#include "hev-config-const.h"

static unsigned int tun_mtu = 8500;

static HevConfigServer srv;

static char log_file[1024];
static int task_stack_size = 86016;
static int tcp_buffer_size = 65536;
static int connect_timeout = 5000;
static int read_write_timeout = 60000;
static int log_level = HEV_LOGGER_WARN;
static int dns_over_tcp;

static int
hev_config_parse_log_level (const char *value)
{
    if (0 == strcmp (value, "debug"))
        return HEV_LOGGER_DEBUG;
    else if (0 == strcmp (value, "info"))
        return HEV_LOGGER_INFO;
    else if (0 == strcmp (value, "error"))
        return HEV_LOGGER_ERROR;

    return HEV_LOGGER_WARN;
}

static void
trim (char *s)
{
    char *start = s;
    char *end;

    while (*start && isspace ((unsigned char)*start))
        start++;
    if (start != s)
        memmove (s, start, strlen (start) + 1);

    end = s + strlen (s);
    while (end > s && isspace ((unsigned char)end[-1]))
        end--;
    *end = 0;
}

static int
hev_config_parse_line (char *line)
{
    char *eq;
    char *key;
    char *value;
    size_t n;

    n = strlen (line);
    while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = 0;

    if (!line[0] || line[0] == '#')
        return 0;

    eq = strchr (line, '=');
    if (!eq)
        return -1;

    *eq = 0;
    key = line;
    value = eq + 1;
    trim (key);
    trim (value);

    if (0 == strcmp (key, "mtu"))
        tun_mtu = (unsigned int)strtoul (value, NULL, 10);
    else if (0 == strcmp (key, "socks5-address"))
        strncpy (srv.addr, value, sizeof (srv.addr) - 1);
    else if (0 == strcmp (key, "socks5-port"))
        srv.port = (unsigned short)strtoul (value, NULL, 10);
    else if (0 == strcmp (key, "socks5-udp"))
        srv.udp_in_udp = (0 == strcmp (value, "udp")) ? 1 : 0;
    else if (0 == strcmp (key, "log-file"))
        strncpy (log_file, value, sizeof (log_file) - 1);
    else if (0 == strcmp (key, "log-level"))
        log_level = hev_config_parse_log_level (value);
    else if (0 == strcmp (key, "dns-over-tcp"))
        dns_over_tcp = ((0 == strcmp (value, "true")) ||
                        (strtoul (value, NULL, 10) == 1));
    else if (0 == strcmp (key, "task-stack-size"))
        task_stack_size = (int)strtoul (value, NULL, 10);

    return 0;
}

int
hev_config_init_from_file (const char *config_path)
{
    char line[1024];
    int min_task_stack_size;
    FILE *fp;
    int res = -1;

    fp = fopen (config_path, "r");
    if (!fp) {
        fprintf (stderr, "Open %s failed!\n", config_path);
        return -1;
    }

    while (fgets (line, sizeof (line), fp)) {
        if (hev_config_parse_line (line) < 0) {
            fprintf (stderr, "Parse %s failed!\n", config_path);
            goto exit;
        }
    }

    if (!srv.addr[0] || srv.port == 0) {
        fprintf (stderr, "Can't found socks5 server!\n");
        goto exit;
    }

    if (tcp_buffer_size > TCP_SND_BUF)
        tcp_buffer_size = TCP_SND_BUF;

    min_task_stack_size = TASK_STACK_SIZE + tcp_buffer_size;
    if (task_stack_size < min_task_stack_size)
        task_stack_size = min_task_stack_size;

    res = 0;
exit:
    fclose (fp);
    return res;
}

void
hev_config_fini (void)
{
}

unsigned int
hev_config_get_tunnel_mtu (void)
{
    return tun_mtu;
}

HevConfigServer *
hev_config_get_socks5_server (void)
{
    return &srv;
}

int
hev_config_get_misc_task_stack_size (void)
{
    return task_stack_size;
}

int
hev_config_get_misc_tcp_buffer_size (void)
{
    return tcp_buffer_size;
}

int
hev_config_get_misc_connect_timeout (void)
{
    return connect_timeout;
}

int
hev_config_get_misc_read_write_timeout (void)
{
    return read_write_timeout;
}

const char *
hev_config_get_misc_log_file (void)
{
    if (!log_file[0])
        return "stderr";

    return log_file;
}

int
hev_config_get_misc_log_level (void)
{
    return log_level;
}

int
hev_config_get_misc_dns_over_tcp (void)
{
    return dns_over_tcp;
}

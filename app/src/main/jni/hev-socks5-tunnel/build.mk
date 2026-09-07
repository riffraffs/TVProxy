# Build
#
# TVProxy prunes the upstream rwildcard list to the client-only sources used by
# the Android VpnService path: no SOCKS5 server/auth/user, no other-OS tunnel
# backends, no post-up/pre-down exec helper, no libyaml (config is key=value).

SRCFILES := \
    $(SRCDIR)/hev-config.c \
    $(SRCDIR)/hev-dns-tcp.c \
    $(SRCDIR)/hev-fake-ip.c \
    $(SRCDIR)/hev-jni.c \
    $(SRCDIR)/hev-main.c \
    $(SRCDIR)/hev-socks5-session.c \
    $(SRCDIR)/hev-socks5-session-tcp.c \
    $(SRCDIR)/hev-socks5-session-udp.c \
    $(SRCDIR)/hev-socks5-tunnel.c \
    $(SRCDIR)/hev-tunnel-linux.c \
    $(SRCDIR)/core/src/hev-socks5.c \
    $(SRCDIR)/core/src/hev-socks5-client.c \
    $(SRCDIR)/core/src/hev-socks5-client-tcp.c \
    $(SRCDIR)/core/src/hev-socks5-client-udp.c \
    $(SRCDIR)/core/src/hev-socks5-logger.c \
    $(SRCDIR)/core/src/hev-socks5-misc.c \
    $(SRCDIR)/core/src/hev-socks5-tcp.c \
    $(SRCDIR)/core/src/hev-socks5-udp.c \
    $(SRCDIR)/misc/hev-list.c \
    $(SRCDIR)/misc/hev-logger.c \
    $(SRCDIR)/misc/hev-ring-buffer.c \
    $(SRCDIR)/misc/hev-utils.c

ifeq ($(REV_ID),)
  ifneq (,$(wildcard .rev-id))
    REV_ID=$(shell cat .rev-id)
  endif
  ifeq ($(REV_ID),)
    REV_ID=$(shell git -C $(SRCDIR) rev-parse --short HEAD)
  endif
  ifeq ($(REV_ID),)
    REV_ID=unknown
  endif
endif
VERSION_CFLAGS=-DCOMMIT_ID=\"$(REV_ID)\"

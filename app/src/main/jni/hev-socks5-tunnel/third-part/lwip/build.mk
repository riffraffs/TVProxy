# Build
#
# TVProxy: compile only the lwIP modules used by the Android tun path.
# No sequential/socket API, PPP, Ethernet/other netifs, unix/win32 ports.

SRCFILES := \
    $(SRCDIR)/core/def.c \
    $(SRCDIR)/core/inet_chksum.c \
    $(SRCDIR)/core/init.c \
    $(SRCDIR)/core/ip.c \
    $(SRCDIR)/core/mem.c \
    $(SRCDIR)/core/memp.c \
    $(SRCDIR)/core/netif.c \
    $(SRCDIR)/core/pbuf.c \
    $(SRCDIR)/core/stats.c \
    $(SRCDIR)/core/sys.c \
    $(SRCDIR)/core/tcp.c \
    $(SRCDIR)/core/tcp_in.c \
    $(SRCDIR)/core/tcp_out.c \
    $(SRCDIR)/core/timeouts.c \
    $(SRCDIR)/core/udp.c \
    $(SRCDIR)/core/ipv4/icmp.c \
    $(SRCDIR)/core/ipv4/ip4.c \
    $(SRCDIR)/core/ipv4/ip4_addr.c \
    $(SRCDIR)/core/ipv4/ip4_frag.c \
    $(SRCDIR)/core/ipv6/icmp6.c \
    $(SRCDIR)/core/ipv6/inet6.c \
    $(SRCDIR)/core/ipv6/ip6.c \
    $(SRCDIR)/core/ipv6/ip6_addr.c \
    $(SRCDIR)/core/ipv6/ip6_frag.c \
    $(SRCDIR)/core/ipv6/mld6.c \
    $(SRCDIR)/core/ipv6/nd6.c \
    $(SRCDIR)/ports/lib/mem.c

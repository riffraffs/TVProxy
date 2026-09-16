# Build
#
# TVProxy: Android VpnService path only (epoll + ARM/AArch64).
# No kqueue/WSA reactors, no sample apps/tests.

SRCFILES := \
    $(SRCDIR)/kern/aide/hev-task-aide.c \
    $(SRCDIR)/kern/core/hev-task-system-schedule.c \
    $(SRCDIR)/kern/core/hev-task-system.c \
    $(SRCDIR)/kern/io/hev-task-io-reactor-epoll.c \
    $(SRCDIR)/kern/itc/hev-task-channel-select.c \
    $(SRCDIR)/kern/itc/hev-task-channel.c \
    $(SRCDIR)/kern/sync/hev-task-cond.c \
    $(SRCDIR)/kern/sync/hev-task-mutex.c \
    $(SRCDIR)/kern/task/hev-task-call.c \
    $(SRCDIR)/kern/task/hev-task-execute.S \
    $(SRCDIR)/kern/task/hev-task-executer.c \
    $(SRCDIR)/kern/task/hev-task-stack-heap.c \
    $(SRCDIR)/kern/task/hev-task-stack-mmap.c \
    $(SRCDIR)/kern/task/hev-task.c \
    $(SRCDIR)/kern/time/hev-task-timer.c \
    $(SRCDIR)/lib/cio/base/hev-task-cio.c \
    $(SRCDIR)/lib/cio/buffer/hev-task-cio-buffer.c \
    $(SRCDIR)/lib/cio/fd/hev-task-cio-fd.c \
    $(SRCDIR)/lib/cio/null/hev-task-cio-null.c \
    $(SRCDIR)/lib/cio/socket/hev-task-cio-socket.c \
    $(SRCDIR)/lib/dns/hev-task-dns-proxy.c \
    $(SRCDIR)/lib/dns/hev-task-dns.c \
    $(SRCDIR)/lib/io/basic/hev-task-io.c \
    $(SRCDIR)/lib/io/buffer/hev-circular-buffer.c \
    $(SRCDIR)/lib/io/pipe/hev-task-io-pipe.c \
    $(SRCDIR)/lib/io/poll/hev-task-io-poll.c \
    $(SRCDIR)/lib/io/socket/hev-task-io-socket.c \
    $(SRCDIR)/lib/list/hev-list.c \
    $(SRCDIR)/lib/misc/hev-debugger.c \
    $(SRCDIR)/lib/misc/hev-task-stack-detector.c \
    $(SRCDIR)/lib/object/hev-object-atomic.c \
    $(SRCDIR)/lib/object/hev-object.c \
    $(SRCDIR)/lib/rbtree/hev-rbtree-cached.c \
    $(SRCDIR)/lib/rbtree/hev-rbtree.c \
    $(SRCDIR)/mem/api/hev-memory-allocator-api.c \
    $(SRCDIR)/mem/base/hev-memory-allocator.c \
    $(SRCDIR)/mem/simple/hev-memory-allocator-simple.c \
    $(SRCDIR)/mem/slice/hev-memory-allocator-slice.c

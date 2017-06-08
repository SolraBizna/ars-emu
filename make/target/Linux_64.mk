# Linux, and likely other UNIX-likes (explicitly 64-bit)

include make/target/Linux.mk
CFLAGS+=-m64
LDFLAGS+=-m64

# Linux, and likely other UNIX-likes (explicitly 32-bit)

include make/target/Linux.mk
CFLAGS+=-m32
LDFLAGS+=-m32

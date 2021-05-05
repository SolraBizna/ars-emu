# macOS (Intel 32-bit, 10.6 and later, via a cross compiler)

include make/target/MacOSX_cross.mk
CFLAGS+=-arch i386
LDFLAGS+=-arch i386

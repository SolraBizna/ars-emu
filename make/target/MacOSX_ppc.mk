# macOS (PowerPC 32-bit, 10.5 and later)

include make/target/MacOSX.mk
CFLAGS+=-arch ppc
LDFLAGS+=-arch ppc
MIN_VERSION_OPTION=-mmacosx-version-min=10.5

# macOS (10.6 and later)

# TODO: fat binary (need x86_64 version of libstdc++)
# Compiler used to compile C code.
CC=i386-apple-darwin10-gcc
# Compiler used to compile C++ code.
CXX=i386-apple-darwin10-g++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=i386-apple-darwin10-g++

MIN_VERSION_OPTION=-mmacosx-version-min=10.6

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-Iinclude/ -Isrc/teg/ -Isrc/libsn/ -Isrc/lsx/include/ -Isrc/byuuML/ -I/opt/releng/mac/include/lua5.3 -DMACOSX -DDROPPY_OS -DMMAP_AVAILABLE
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/opt/releng/mac/Frameworks/SDL2.framework/Headers -Wall -Wextra -Werror $(MIN_VERSION_OPTION) -c
CFLAGS_DEBUG=-Og -g
CFLAGS_RELEASE=-Ofast -ffast-math -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual -fvisibility=hidden -fvisibility-inlines-hidden
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-F/opt/releng/mac/Frameworks -Wl,-rpath -Wl,@executable_path/../Frameworks $(MIN_VERSION_OPTION)
LDFLAGS_DEBUG=-g
LDFLAGS_RELEASE=-Ofast -ffast-math -flto
# Libraries.
LIBS=-framework SDL2 -L/opt/releng/mac/lib -lz -llua5.3

# Extension for executables.
EXE=

# TODO: fat binary (need x86_64 version of libstdc++)
# Compiler used to compile C code.
CC=gcc-mp-4.9
# Compiler used to compile C++ code.
CXX=g++-mp-4.9
# Linker used. (This will usually be the same as the C++ compiler.)
LD=g++-mp-4.9
# Library archiver used
AR=gcc-ar-mp-4.9

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-Iinclude/ -Isrc/teg/ -DMACOSX
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/Library/Frameworks/SDL2.framework/Headers -Wall -Wextra -Werror -c
CFLAGS_DEBUG=-ggdb
CFLAGS_RELEASE=-flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=gnu++11 -Woverloaded-virtual
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=-flto
# Libraries.
LIBS=-framework SDL2 -L/usr/local/lib -lz
# Flags passed to the library archiver
ARFLAGS=-rsc

# Extension for executables.
EXE=

# Build the Cocoa support code into TEG
COCOA_SUPPORT=1

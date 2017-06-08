# Linux, and likely other UNIX-likes

# Compiler used to compile C code.
CC=gcc
# Compiler used to compile C++ code.
CXX=g++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=g++

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -Iinclude/ -Isrc/teg/ -Isrc/libsn/ -I/usr/include/lua5.3
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=`sdl2-config --cflags` -Wall -Wextra -Werror -c
CFLAGS_DEBUG=-Og -ggdb
CFLAGS_RELEASE=-Ofast -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-pthread
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=-Ofast -flto
# Libraries.
LIBS=`sdl2-config --libs` -lz -llua5.3

# Extension for executables.
EXE=

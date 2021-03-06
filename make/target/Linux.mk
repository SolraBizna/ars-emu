# Linux, and likely other UNIX-likes

# Compiler used to compile C code.
CC=gcc
# Compiler used to compile C++ code.
CXX=g++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=g++

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -Iinclude/ -Isrc/teg/ -Isrc/libsn/ -Isrc/lsx/include/ -Isrc/byuuML/ -I/usr/include/lua5.3 -DMMAP_AVAILABLE
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=`sdl2-config --cflags` -Wall -Wextra -Werror -c
CFLAGS_DEBUG=-ggdb
CFLAGS_RELEASE=-Ofast -ffast-math -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-pthread
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=-Ofast -ffast-math -flto
# Libraries.
LIBS=`sdl2-config --libs` -lpng -lz -llua5.3 -lboost_filesystem -lboost_system

# Extension for executables.
EXE=

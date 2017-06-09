# macOS 10.5 and later

# TODO: fat binary (need x86_64 version of libstdc++)
# Compiler used to compile C code.
CC=i386-apple-darwin10-gcc
# Compiler used to compile C++ code.
CXX=i386-apple-darwin10-g++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=i386-apple-darwin10-g++

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-Iinclude/ -Isrc/teg/ -Isrc/libsn/ -I/opt/releng/mac/include/lua5.3 -DMACOSX -DDROPPY_OS
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/opt/releng/mac/Frameworks/SDL2.framework/Headers -Wall -Wextra -Werror -mmacosx-version-min=10.6 -arch i386 -arch x86_64 -c
CFLAGS_DEBUG=-Og -ggdb
CFLAGS_RELEASE=-Ofast -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual -fvisibility=hidden -fvisibility-inlines-hidden
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-F/opt/releng/mac/Frameworks -Wl,-rpath -Wl,@executable_path/../Frameworks
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=-Ofast -flto
# Libraries.
LIBS=-framework SDL2 -L/opt/releng/mac/lib -lz -llua5.3 -arch i386 -arch x86_64

# Extension for executables.
EXE=

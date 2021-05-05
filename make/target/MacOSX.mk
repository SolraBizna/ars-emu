# macOS (recent, native)

# Compiler used to compile C code.
CC=clang++
# Compiler used to compile C++ code.
CXX=clang++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=clang++

MIN_VERSION_OPTION= #-mmacosx-version-min=10.6

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-Iinclude/ -Isrc/teg/ -Isrc/libsn/ -Isrc/lsx/include/ -Isrc/byuuML/ -I/opt/homebrew/opt/lua@5.3/include/lua5.3 -I/opt/homebrew/include -DMACOSX -DDROPPY_OS -DMMAP_AVAILABLE -DNO_MASK_STRDUP
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/Library/Frameworks/SDL2.framework/Headers -Wall -Wextra -Wno-unknown-attributes $(MIN_VERSION_OPTION) -c
CFLAGS_DEBUG=-Og -g
CFLAGS_RELEASE=-Ofast -ffast-math -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual -fvisibility=hidden -fvisibility-inlines-hidden
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE)
# Flags passed to the linker.
LDFLAGS=-F/Library/Frameworks -Wl,-rpath -Wl,@executable_path/../Frameworks $(MIN_VERSION_OPTION)
LDFLAGS_DEBUG=-g
LDFLAGS_RELEASE=-Ofast -ffast-math -flto
# Libraries.
LIBS=-framework SDL2 -L/opt/homebrew/opt/lua@5.3/lib -L/opt/homebrew/lib -lpng -lz -llua5.3 -lboost_filesystem

# Extension for executables.
EXE=

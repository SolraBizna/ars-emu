# Compiler used to compile C code.
CC=emcc
# Compiler used to compile C++ code.
CXX=em++
# Linker used. (This will usually be the same as the C++ compiler.)
LD=em++

# Hacky flags
CROSS_COMPILE=1
RELEASE_ONLY=1

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -Iinclude/ -Isrc/teg/ -Isrc/libsn/ -I../ars-emscripten-libs -DNO_MASK_MALLOC -DNO_MASK_STRDUP -DNO_DEBUG_CORES -DEMSCRIPTEN
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-Wall -Wextra -Werror -s USE_SDL=2 -c
CFLAGS_RELEASE=-O3 -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE)
# Flags passed to the linker.
LDFLAGS_RELEASE=-O3 -flto
# Libraries.
LIBS=../ars-emscripten-libs/ars-emscripten-libs.bc -s USE_SDL=2 -s TOTAL_MEMORY=134217728

# Extension for executables.
EXE=.js
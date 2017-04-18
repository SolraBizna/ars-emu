# Compiler used to compile C code.
CC=i686-w64-mingw32-gcc-posix
# Compiler used to compile C++ code.
CXX=i686-w64-mingw32-g++-posix
# Linker used. (This will usually be the same as the C++ compiler.)
LD=i686-w64-mingw32-g++-posix
# Library archiver used
AR=i686-w64-mingw32-gcc-ar

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -I/opt/releng/w32/include -Iinclude/ -Imingw-std-threads/ -Isrc/teg/ -D_UNICODE -DUNICODE -DMINGW -D_WIN32_WINNT=0x0501 -DUSE_MINGW_STD_THREADS
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/opt/releng/w32/include/SDL2 -Dmain=SDL_main -Wall -Wextra -Werror -municode -c
CFLAGS_DEBUG=-ggdb
CFLAGS_RELEASE=
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=gnu++11 -Woverloaded-virtual
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-mwindows -municode
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=
# Libraries.
LIBS=-L/opt/releng/w32/lib -lmingw32 -lws2_32 /opt/releng/w32/bin/SDL2.dll -lz
# Flags passed to the library archiver
ARFLAGS=-rsc

# Extension for executables.
EXE=.exe

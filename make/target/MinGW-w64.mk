# Windows, using MinGW-w64

# Compiler used to compile C code.
CC=i686-w64-mingw32-gcc-win32
# Compiler used to compile C++ code.
CXX=i686-w64-mingw32-g++-win32
# Linker used. (This will usually be the same as the C++ compiler.)
LD=i686-w64-mingw32-g++-win32

# hack to support the Windows icon
NEED_WINDOWS_ICON=1
WINDRES=i686-w64-mingw32-windres

# Flags passed to all calls that involve a C preprocessor.
CPPFLAGS=-MP -MMD -I/opt/releng/w32/include -Iinclude/ -Isrc/teg/ -Isrc/libsn/ -Isrc/lsx/include/ -Isrc/byuuML/ -D_UNICODE -DUNICODE -DMINGW -D_WIN32_WINNT=0x0501 -DDROPPY_OS
CPPFLAGS_DEBUG=-DDEBUG=1
CPPFLAGS_RELEASE=-DRELEASE=1 -DNDEBUG=1
# Flags passed to the C compiler.
CFLAGS=-I/opt/releng/w32/include/SDL2 -I/opt/releng/w32/include/lua5.3 -Dmain=SDL_main -Wall -Wextra -Werror -municode -c
CFLAGS_DEBUG=-Og -ggdb
CFLAGS_RELEASE=-Ofast -ffast-math -flto
# Flags passed the C++ compiler.
CXXFLAGS=$(CFLAGS) -std=c++14 -Woverloaded-virtual
CXXFLAGS_DEBUG=$(CFLAGS_DEBUG)
CXXFLAGS_RELEASE=$(CFLAGS_RELEASE) -fno-enforce-eh-specs
# Flags passed to the linker.
LDFLAGS=-mwindows -municode -static-libgcc -static-libstdc++
LDFLAGS_DEBUG=-ggdb
LDFLAGS_RELEASE=-Ofast -ffast-math -flto
# Libraries.
LIBS=-L/opt/releng/w32/lib -static -lmingw32 -lz -llua5.3 /opt/releng/w32/bin/SDL2.dll

# Extension for executables.
EXE=.exe

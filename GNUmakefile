include make/cur_target.mk

CPPFLAGS+=-DGAME_PRETTY_NAME="\"ARS Emulator\""
#CPPFLAGS+=-DXGL_ENABLE_VBO -DXGL_ENABLE_RECTANGLE_TEXTURES -DXGL_ENABLE_PBO
CPPFLAGS+=-DTEG_NO_POSTINIT -DNO_OPENGL

TEG_OBJECTS=obj/teg/io.o obj/teg/main.o obj/teg/miscutil.o obj/teg/config.o

EXE_LIST:=

define define_exe =
$(eval
EXE_LIST+=$1
bin/$1-release$(EXE): obj/$1.o $2
bin/$1-debug$(EXE): $(patsubst %.o,%.debug.o,obj/$1.o $2)
)
endef

$(call define_exe,ars-emu,obj/ppu_scanline.o obj/cartridge.o obj/cpu_scanline.o obj/cpu_scanline_debug.o obj/eval.o obj/controller.o obj/ars-apu.o $(TEG_OBJECTS))

all: gen obj/ROM.rom \
	$(addsuffix -debug$(EXE),$(addprefix bin/,$(EXE_LIST))) \
	$(addsuffix -release$(EXE),$(addprefix bin/,$(EXE_LIST)))

gen: include/gen/messagefont.hh

obj/vincent.bin: src/font2bin.lua src/vincent.png
	@mkdir -p obj
	@echo "Turning Vincent into a binary blob..."
	@$^ $@

obj/ROM.rom: $(addprefix obj/,$(addsuffix .o,$(notdir $(wildcard asm/*.65c))))
	@mkdir -p bin
	@echo "Generating ROM image..."
	@echo [objects] > obj/ROM.link
	@for f in $^; do echo $$f >> obj/ROM.link; done
	@echo >> obj/ROM.link
	@echo [header] >> obj/ROM.link
	@echo src/header.dat >> obj/ROM.link
	@wlalink -S obj/ROM.link obj/ROM.rom

make/cur_target.mk:
	@echo Please point cur_target.mk to an appropriate target definition.
	@false

bin/%-release$(EXE): obj/%.o
	@mkdir -p bin
	@echo Linking "$@" "(release)"...
	@$(LD) $(LDFLAGS) $(LDFLAGS_RELEASE) -o "$@" $^ $(LIBS)
bin/%-debug$(EXE): obj/%.debug.o
	@mkdir -p bin
	@echo Linking "$@" "(debug)"...
	@$(LD) $(LDFLAGS) $(LDFLAGS_DEBUG) -o bin/"$*"-debug$(EXE) $^ $(LIBS)

include/gen/%.hh: src/gen_%_hh.lua
	@mkdir -p include/gen
	@echo Generating "$@"...
	@$^

obj/%.o: src/%.m
	@mkdir -p obj
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.m
	@mkdir -p obj
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -o "$@" "$<"

obj/%.o: src/%.c
	@mkdir -p obj
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.c
	@mkdir -p obj
	@echo Compiling "$<" "(debug)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) -o "$@" "$<"

obj/%.o: src/%.cc
	@mkdir -p obj/teg
	@echo Compiling "$<" "(release)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) $(CXXFLAGS) $(CXXFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/%.cc
	@mkdir -p obj/teg
	@echo Compiling "$<" "(debug)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) $(CXXFLAGS) $(CXXFLAGS_DEBUG) -o "$@" "$<"

obj/%.65c.o: asm/%.65c $(wildcard asm/*.inc) obj/vincent.bin
	@mkdir -p obj
	@echo Assembling "$<"...
	@wla-65c02 -qo "$<" "$@"

clean:
	rm -rf bin obj include/gen

include $(wildcard obj/*.d)
include $(wildcard obj/teg/*.d)

.PHONY: all clean data gen

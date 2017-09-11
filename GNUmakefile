include make/cur_target.mk

UNIFONT_VERSION=9.0.06

CPPFLAGS+=-DGAME_PRETTY_NAME="\"ARS Emulator\""
#CPPFLAGS+=-DXGL_ENABLE_VBO -DXGL_ENABLE_RECTANGLE_TEXTURES -DXGL_ENABLE_PBO
CPPFLAGS+=-DTEG_NO_POSTINIT -DNO_OPENGL -DTEG_USE_SN

TEG_OBJECTS=obj/teg/io.o obj/teg/main.o obj/teg/miscutil.o obj/teg/config.o

EXE_LIST:=ars-emu

FULL_EXE_LIST:=$(addsuffix -release$(EXE),$(addprefix bin/,$(EXE_LIST)))
ifndef RELEASE_ONLY
FULL_EXE_LIST+=$(addsuffix -debug$(EXE),$(addprefix bin/,$(EXE_LIST)))
endif

ifdef CROSS_COMPILE
all: gen $(FULL_EXE_LIST)
else
all: gen all-data \
	$(FULL_EXE_LIST)
endif

define define_exe =
$(eval
bin/$1-release$(EXE): obj/$1.o $2
ifndef RELEASE_ONLY
bin/$1-debug$(EXE): $(patsubst %.o,%.debug.o,obj/$1.o $2)
endif
)
endef

EXTRA_OBJECTS:=
ifdef NEED_WINDOWS_ICON
EXTRA_OBJECTS+=obj/ars-emu.res
endif

# We include obj/lsx/lsx_bzero.o while making no attempt to prevent it from
# being optimized out, because there is no sensitive data to "leak". The only
# SimpleConfig image currently considered "secure" is publicly available.
$(call define_exe,ars-emu,obj/ppu_scanline.o obj/cartridge.o obj/cpu_scanline.o obj/cpu_scanline_debug.o obj/cpu_scanline_intprof.o obj/eval.o obj/controller.o obj/apu.o obj/sn_core.o obj/sn_get_system_language.o obj/font.o obj/utfit.o obj/configurator.o obj/prefs.o obj/menu.o obj/menu_main.o obj/menu_fight.o obj/menu_keyboard.o obj/audiocvt.o obj/windower.o obj/lsx/lsx_sha256.o obj/lsx/lsx_bzero.o obj/ppu_common.o obj/fx.o obj/messages.o obj/display.o obj/display_safe.o obj/display_sdl.o obj/upscale.o $(TEG_OBJECTS) $(EXTRA_OBJECTS))
ifndef CROSS_COMPILE
$(call define_exe,compile-font,obj/sn_core.o $(TEG_OBJECTS))
$(call define_exe,pretty-string,obj/font.o obj/utfit.o obj/sn_core.o $(TEG_OBJECTS))
endif

gen:
	@true # currently nothing to generate

all-data: bin/Data

bin/Data:
	@echo "Linking data files..."
	@mkdir -p bin
	@(cd bin; ln -s ../Data)

ifndef CROSS_COMPILE
Data/Font: bin/compile-font-release$(EXE) dist/unifont-$(UNIFONT_VERSION)/font/precompiled/unifont-$(UNIFONT_VERSION).hex dist/unifont-$(UNIFONT_VERSION)/font/precompiled/unifont_upper-$(UNIFONT_VERSION).hex
	@echo "Compiling font..."
	@mkdir -p Data
	@$^ > $@

Data/SimpleConfig.etarz: obj/SimpleConfig.etars
	@echo "Compressing simple configuration ROM image..."
	@mkdir -p Data
	@gzip -cfk9 "$<" > "$@"

obj/SimpleConfig.etars: $(addprefix obj/,$(addsuffix .o,$(notdir $(wildcard asm/*.65c))))
	@mkdir -p obj
	@echo "Generating simple configuration ROM image..."
	@echo [objects] > obj/SimpleConfig.link
	@for f in $^; do echo $$f >> obj/SimpleConfig.link; done
	@echo >> obj/SimpleConfig.link
	@echo [header] >> obj/SimpleConfig.link
	@echo src/header.dat >> obj/SimpleConfig.link
	@wlalink -S obj/SimpleConfig.link obj/SimpleConfig.etars
endif

make/cur_target.mk:
	@./choose_target.sh

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

obj/lsx/%.o: src/lsx/src/%.c
	@mkdir -p obj/lsx
	@echo Compiling "$<" "(release)"...
	@$(CC) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) -o "$@" "$<"
obj/lsx/%.debug.o: src/lsx/src/%.c
	@mkdir -p obj/lsx
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

obj/%.o: src/libsn/%.cc
	@mkdir -p obj/teg
	@echo Compiling "$<" "(release)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_RELEASE) $(CFLAGS) $(CFLAGS_RELEASE) $(CXXFLAGS) $(CXXFLAGS_RELEASE) -o "$@" "$<"
obj/%.debug.o: src/libsn/%.cc
	@mkdir -p obj/teg
	@echo Compiling "$<" "(debug)"...
	@$(CXX) $(CPPFLAGS) $(CPPFLAGS_DEBUG) $(CFLAGS) $(CFLAGS_DEBUG) $(CXXFLAGS) $(CXXFLAGS_DEBUG) -o "$@" "$<"

obj/%.res: src/%.rc
	@mkdir -p obj
	@echo Compiling "$<"...
	@$(WINDRES) "$<" -O coff -o "$@"

obj/%.65c.o: asm/%.65c $(wildcard asm/*.inc)
	@mkdir -p obj
	@echo Assembling "$<"...
	@wla-65c02 -q -o "$@" "$<"

clean:
	rm -rf bin obj include/gen

include $(wildcard obj/*.d)
include $(wildcard obj/teg/*.d)

.PHONY: all clean data gen all-data

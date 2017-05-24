This is an emulator for a fictional 8-bit home video game system from the mid 1980's. I wanted to make an 8-bit style game that was "authentic", in that it had to face the same sort of limitations real 8-bit games faced. No giant rotating bosses or expensive particle effects for me, no sir. That said, I didn't want to deal with the *annoying* limitations. So I designed (in some places, down to the circuit element level!) an 8-bit console with only *fun* limitations. This is an emulator for that console.

# Status

Emulates all ARS hardware, as well as a fake Emulator Configuration port. Includes a fairly accurate, scanline-based emulator core, and a simple debugger. The scanline-based core probably runs at full speed on any 800MHz or faster system. Configuration and UI can be localized to any Unicode-compatible language that doesn't require combining characters.

Still to be done:

- <del>Better</del> Any video/audio configuration
- Various levels of TV fakery
- 100% cycle-accurate core, with accompanying debugger
- ARM-optimized core, usable on the EOMA68 A20 Computer Card (if needed)
- Exotic input devices (light pen, light gun, keyboard)
- Modem cartridge
- "Homebrew Achievements Module"

# Building

Building on Windows or macOS is not tested, but should be doable by tweaking the relevant files in `make/target/`. Building on Windows requires MinGW.

## Dependencies

The current version of ARS-emu requires SDL 2.0.4 or later. A future version will be usable all the way back to 2.0.0. Lua 5.3 and a recent zlib are also required.

Packages to install on Debian Stretch or later: `libsdl2.0-dev liblua5.3-dev zlib-dev`

ARS-emu makes use of C++14 features, and requires a very recent C++ compiler. The code should compile with recent clang or GCC. Maybe it compiles with Microsoft's compiler too.

## Linux

(after installing dependencies)

    $ ln -s target/Linux.mk make/cur_target.mk
    $ git submodule init
    $ git submodule update
    $ make bin/ars-emu-debug # or -release instead of -debug if you like

# Limits (and lack thereof)

Ways in which the ARS's limitations are typical of 8-bit systems:

- Simple controller (hat switch, three face buttons, and a pause button)
- Limited audio synthesis (7 simple waveform channels and a noise channel)
- Restricted color palette (based on the NTSC NES palette)
- Small video resolution (approximately 240x224 usable area)
- One scrollable background layer, with a limited palette
- 64 sprites on screen, 8 pixels wide
- 8-bit CPU (a 65C02)
- Theoretical upper limit of 4MiB on cartridge size

Ways in which the ARS's limitations are somewhat laxer than a typical 8-bit system:

- CPU is clocked *very* fast by 8-bit console standards (12.285MHz)
- Memory is plentiful (see table below)
- 3-bit sprites rather than the usual 2-bit ones (allowing Mega Man like sprites with ease)
- No limit on sprites per scanline (which I **EARNED** by careful design!)

<table>
<thead>
<tr><th>System</th><th>Work RAM</th><th>Video RAM</th></tr>
</thead>
<tbody>
<tr><td>NES</td><td>2KiB</td><td>0</td></tr>
<tr><td>Master System</td><td>8KiB</td><td>16KiB</td></tr>
<tr><td>ARS</td><td>~32KiB</td><td>64KiB</td></tr>
</tbody>
</table>

# Documentation

Technical documentation, along with a fictional history of the ARS, will come in the next commit.

# License

ARS-emu is distributed under the GNU General Public License version 3 or later.

- If you distribute a modified binary of ARS-emu, you *must* provide the source code to your modifications.
- Bundling a closed-source or otherwise GPL-incompatible game with a copy of ARS-emu is permitted, even encouraged. Doing so *does not* "infect" the game with the GPL. The only requirement is that the user must be free to subsitute their own build of ARS-emu, or a different emulator entirely, for the one you bundled.

## SimpleConfig

The files in the `asm/` directory belong to the SimpleConfig ROM. SimpleConfig is in the public domain. It is designed to be directly embeddable in other ROMs, allowing games to provide a built-in way to configure the emulator. Detailed documentation on how to do so will come in the next commit.

## GNU Unifont

The emulator includes a compiled version of GNU Unifont. The source repository for the emulator also includes the complete source code of GNU Unifont. You can substitute a different version of Unifont, or a different font entirely, using the included font compiler.

The included version of GNU Unifont is 9.0.06, and is copyright (C) 2016 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, et al. It is licensed under the GNU General Public License version 2 or later, with the GNU Font Embedding Exception.


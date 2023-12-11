This is an emulator for a fictional 8-bit home video game system from the mid 1980's. I wanted to make an 8-bit style game that was "authentic", in that it had to face the same sort of limitations real 8-bit games faced. No giant rotating bosses or expensive particle effects for me, no sir. That said, I didn't want to deal with the *annoying* limitations. So I designed (in some places, down to the circuit element level!) an 8-bit console with only *fun* limitations. This is an emulator for that console.

# Status

Emulates all ARS hardware, as well as a fake Emulator Configuration port. Includes a fairly accurate, scanline-based emulator core, and a simple debugger. The scanline-based core probably runs at full speed on any 800MHz or faster system. Configuration and UI can be localized to any Unicode-compatible language that can be rendered entirely using precomposed characters, and that is readable in a horizontal layout. (LTR and RTL are both supported.)

# Building

Building on Windows or macOS is not tested, but should be doable by tweaking the relevant files in `make/target/`. Building on Windows requires MinGW.

## Dependencies

The current version of ARS-emu requires SDL 2.0.2 or later. A future version will be usable all the way back to 2.0.0. Lua 5.3 and a recent zlib are also required.

Packages to install on Debian Stretch or later: `libsdl2.0-dev liblua5.3-dev zlib-dev`

ARS-emu makes use of C++14 features, and requires a very recent C++ compiler. The code should compile with recent clang or GCC. Maybe it compiles with Microsoft's compiler too, but you're on your own if you want to do that.

## Linux

    $ git submodule init
    $ git submodule update
    $ make bin/Data bin/ars-emu-debug # or -release instead of -debug if you like

When prompted for a target, answer `Linux`

## Emscripten

    $ git submodule init
    $ git submodule update
    $ make bin/ars-emu-release.js

When prompted for a target, answer `Emscripten`

This does NOT include the `Data` directory or a cartridge. You'll want to do something like:

    python path/to/emsdk_portable/emscripten/VERSION/tools/file_packager.py Data.data --embed path/to/ars-emu/bin/Data@/Data --js-output=Data.js
    python path/to/emsdk_portable/emscripten/VERSION/tools/file_packager.py rom.data --embed path/to/cartridge.etarz@/cartridge.etarz --js-output=rom.js

and include the resulting JavaScript files in addition to `ars-emu-release.js`.

## Other platforms

You'll either have to edit one of the files in `make/target` or write one of your own. All of the targets already present (other than `Linux`) are intended for cross-compiling from my releng environment, and will require minor tweaking.

If your build is happening with the wrong target, run `choose_target.sh`.

# Limits (and lack thereof)

Ways in which the ARS's limitations are typical of 8-bit systems:

- Simple controller (hat switch, three face buttons, and a pause button)
- Limited audio synthesis (7 simple waveform channels and a noise channel)
- Restricted color palette (loosely based on the NTSC NES palette)
- Small video resolution (approximately 256x224 usable area)
- One scrollable background layer, with a limited palette
- 64 sprites on screen, 8 pixels wide
- 8-bit CPU (a 65C02)
- Theoretical upper limit of 8MiB on cartridge size

Ways in which the ARS's limitations are somewhat laxer than a typical 8-bit system:

- CPU is clocked *very* fast by 8-bit console standards (12.273MHz)
- RAM is plentiful (see table below)
- 3-bit sprites rather than the usual 2-bit ones (allowing Mega Man like sprites with ease)
- No limit on sprites per scanline (which I **EARNED** by careful design and wanton wasting of gates!)

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

A brief history of Eiling Technologies and the Artificial Reality System console is in `FICTION.md`. A detailed description of the ARS hardware is in `HARDWARE.md`. Both documents are written in an "in-universe" tone, as if the ARS were a real product and Eiling Technologies were a real electronics company.

# License

ARS-emu is licensed under the GNU General Public License version 3 or later. The main consequence of this is that if you distribute modified binaries of ARS-emu, you must also distribute the source code behind those modifications. That is, you must allow everyone else the same freedom to examine and improve the emulator as you yourself had.

## Cartridge Licensing

ARS-emu is designed to be used in combination with ARS cartridge images, and, until another ARS emulator is written, cartridge images cannot be used *except* in combination with ARS-emu. Some reasonable interpretations of the GPL would "contaminate" ARS cartridge images with the GPL. This is not my intention.

To be explicit: Even though many users will not perceive the boundary between game and emulator, I consider bundling a non-Free cartridge with a binary of ARS-emu to constitute an aggregate according to the text of the GPL, and totally acceptable **as long as the user is free to substitute their own modified build of ARS-emu (or other ARS emulator) with no significant loss of features.**

## SimpleConfig

The files in the `asm/` directory belong to the SimpleConfig ROM. SimpleConfig is in the public domain. It is designed to be directly embeddable in other ROMs, allowing games to provide a built-in way to configure the emulator. Detailed documentation on how to do so is at the end of `HARDWARE.md`.

ARS-emu contains an internal whitelist of SimpleConfig images, and its *default* policy is to permit configuration access only to exact copies of known safe SimpleConfig binaries. **This is not a DRM measure.** This is a convenience feature. The *user* need merely pass the `-C` option and the policy becomes "always permit". This feature exists to allow cartridge authors to conveniently provide emulator configuration from within their games, while protecting users from malicious cartridge authors who may have found a security flaw in the (comparatively powerful) configuration interface.

## GNU Unifont

The emulator includes a compiled version of GNU Unifont. The source repository for the emulator also includes the complete source code of GNU Unifont. You can substitute a different version of Unifont, or a different font entirely, using the included font compiler.

The included version of GNU Unifont is 9.0.06, and is copyright (C) 2016 Roman Czyborra, Paul Hardy, Qianqian Fang, Andrew Miller, et al. It is licensed under the GNU General Public License version 2 or later, with the GNU Font Embedding Exception.

## Floppy sounds

The emulator includes some floppy drive sounds. They have been modified from their original form, to save memory and to adapt them to the emulator's requirements. The much higher quality original source audio from which these sounds were extracted was recorded by [MrAuralization](https://freesound.org/people/MrAuralization/), and can be  [downloaded on freesound.org](https://freesound.org/people/MrAuralization/packs/15891/). They are included in ars-emu under the Creative Commons Attribution 3.0 Unported license.

# Missing, Planned Features

## Video

- Selectable window size based on desired scaling (2x, 3x, 4x, etc.)
- Fullscreen mode
- CRT effects
    - Pixel layouts
    - Bloom (both kinds)
    - Ghosting
    - Barrel distortion
    - CRT housing

## Audio

- "Pinky mode" (aggressive lowpass filter for those with sensitive ears)

## Input

- Support for physical controllers outside the SDL Game Controller API

## Hardware

- Modem cartridge?

## Emulation

- Freeze/defrost
- Fast forward
- Game Genie-alike cheat support
- Movie recording, playback, and rendering
- Cycle-based core and debugger (slightly more accurate, but much slower)
- Low-fidelity core, usable on the original Raspberry Pi and on the EOMA68 A20 Computer Card
- "Homebrew Achievements Module"

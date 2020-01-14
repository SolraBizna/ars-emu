<!--*- word-wrap: t; mode:markdown -*-->

# Clocks

The ARS has a seemingly strange core clock frequency. It experiences 780 clock ticks for every scanline of output. With color NTSC's line rate fixed at &approx;15734.266Hz (exactly 2250/143 kHz), this gives a clock rate of &approx;12.272727MHz (exactly 135/11 MHz.) However, where normal interlaced video signals contain 262.5 lines per field, the ARS's video outputs 262 lines per frame. The framerate is thus &approx;60.05445fps (exactly 1125000/18733 fps).

However, computers output video at 60 frames per second, not 60.05445. If an emulator simulates 204360 clocks per frame, and outputs 60 frames per second, all these clocks will effectively be slowed down by 0.09%; this makes everything else so much simpler that it's worth the hardly noticeable difference.

Here are all the interesting clock rates in the console:

| Clock      | Real ratio  | Real value         | Emulated    | Notes     |
| ---------- | ----------- | ------------------ | ----------- | --------- |
| Core       |   135MHz/11 |      12.2727...MHz |  12.2616MHz |           |
| Dot,APU    |   135MHz/22 |      6.13636...MHz |   6.1308MHz | Core/2    |
| Audio      | 135MHz/2816 | 47.940340909...kHz | 47896.875Hz | Core/256  |
| Colorburst | 39375KHz/11 |   3.57954545...MHz |         N/A | Core*7/24 |

# Memory Map

A15 might as well be an active-high "cartridge select" signal.

WCS# (Work RAM chip-select) pin is pulled low, which means that any access is a Work RAM access "by default". A discrete transistor drives it high for accesses to `$8000-$FFFF`, which creates the "cartridge space". The PPU drives it high for `$0211`, `$0213`, `$0215`, `$0217`, allowing reads from the VRAM, CRAM, SAM, and SSM memory ports. IO unit drives it high for `$0240-$0247`, even though not all of those addresses are used. A WCS# pin _is_ provided to the cartridge; fancy hardware could drive it high for certain addresses to provide additional read ports in general Work RAM space, provide additional memory banks, etc. (No official hardware does so.)

- `$0000-$7FFF`: Unless otherwise specified below, Work RAM.
- `$0200-$021F`: PPU registers
- `$0211,$0213,$0215,$0217`: PPU memory ports
- `$0220-$023F`: APU control memory
- `$0240,$0241`: Controller ports
- `$0245`: Homebrew Achievements Module
- `$0246`: Emulator configuration
- `$0247`: Developer cartridge debug port
- `$0248,$024F`: Bank select registers. (See Cartridge Memory Map.)
- `$7C00-$7F7F`: Area of work RAM used for Overlay tilemap.
- `$7F80-$7FF7`: Area of work RAM used for Overlay palette selection.
- `$8000-$FFFF`: Cartridge space. (See Cartridge Memory Map.)

Note on registers: Writes to register space write both to the register and to Work RAM. Most register space reads read the WRAM and _not_ the register value. Most registers will retain the same value as is written, so the distinction only matters with the port address registers.

# PPU

## Registers

(no mark) Safe at all times, writes take effect immediately  
[+] Writes take effect at some point during next H-blank  
[H] Dangerous during (part of) H-blank, safe at other times  
[O] Dangerous during scan-out, safe at other times

`$0200`: [+]BG scroll X  
`$0201`: [+]BG scroll Y  
`$0202`: [+]

- Bit 0: Swap BGs horizontally (0<->1, 2<->3)
- Bit 1: Swap BGs vertically (0<->2, 1<->3)
- Bit 2: 0 = BG mode 1; 1 = BG mode 2
- Bit 3: Video enable. Checked only at the moment the _first_ hblank occurs. If 0, only black will be output and normal PPU processing is skipped.
- Bit 4-7: High nybble of OL base tile address. `0000` disables the overlay, freeing up about 11.25% of main CPU cycles. Note that unlike other tile data, overlay tile data is fetched from *main* memory, not VRAM.

`$0203`: BG tile bases (top)

- Bit 0-3: High nybble of BG0 base tile address.
- Bit 4-7: High nybble of BG1 base tile address.

`$0204`: BG tile bases (bottom)

- Bit 0-3: High nybble of BG2 base tile address.
- Bit 4-7: High nybble of BG3 base tile address.

`$0205`: Sprite palette info, per background region

- Bit 0-1: High 2 bits of base location in CRAM for sprite palette (in BG0 region)
- Bit 2-3: As above for BG1 region
- Bit 4-5: As above for BG2 region
- Bit 6-7: As above for BG3 region

`$0206`: High 4 bits of base location in CRAM for BG0 palette  
`$0207`: As above for BG1  
`$0208`: As above for BG2  
`$0209`: As above for BG3  
`$020A`: BG0 foreground info

- Bit 0-1: For palette 0: 00 = no foreground colors, 01 = colors 2 and 3 are foreground, 10 = colors 1-3 are foreground, 11 = all colors are foreground
- Bit 2-3: As above for palette 1
- Bit 4-5: As above for palette 2
- Bit 6-7: As above for palette 3

`$020B`: As above for BG1  
`$020C`: As above for BG2  
`$020D`: As above for BG3  
`$020E`: High 5 bits of base location in CRAM for OL palette  
`$020F`: "Color mod" value. Added to the hardware palette index before output. Pretty much only usable for fades. In theory, arbitrary video could be outputted by writing to this register repeatedly *very* quickly...  
`$0210`: VRAM access page; high 8 bits of location from which to access VRAM. Writing this register clears the low 8 bits of the location.  
`$0211`: [HO]VRAM access port.  
`$0212`: CRAM access index.  
`$0213`: [O]CRAM access port.  
`$0214`: SSM access index.  
`$0215`: [H+]SSM access port.  
`$0216`: SAM access index.  
`$0217`: [HO]SAM access port.  
`$0218`: VRAM access byte; low 8 bits of access location. Write the access page _first_ if you intend to write both.  
`$0219`: IRQ scanline number. During H-blank/scan-out, IRQB# wil be asserted if the upcoming/current (respectively) scanline number is greater than or equal to this register's value. A value of 240 will signal IRQ during the entire vblank period. (This is probably not an intentional feature, and has no known use.) Values of 241 and above effectively disable scanline IRQ. Values < 128 will _not_ signal for scanline numbers ≥ 128, as a special case; among other things, this allows a clean scanline 0 IRQ.  
`$021A`: [HO]VRAM DMA. Write `xx` to this, and the PPU will copy `$xx00-$xxFF` as if it were written to `$0211`. This takes 257 cycles, not counting the write to `$021A`.  
`$021B`: [HO]VRAM "splat" DMA. Write `xx` to this, and the PPU will do a "splat copy". Takes 1025 cycles. (See information on BG mode 2.)  
`$021C`: [HO]CRAM DMA. As `$021A`, but writes to CRAM (as if via `$0213`).  
`$021D`: [H+]SSM DMA. As `$021A`, but writes to SSM (as if via `$0215`).  
`$021E`: [HO]SAM DMA. As `$021A`, but writes to SAM (as if via `$0217`), and only transfers 64 bytes, taking 65 cycles. (This means it can't be used to touch $40-FF from any page.)  
`$021F`: [H+]SSM Unpacked DMA. As `$021D`, but reads `$00`, `$40`, `$80`, `$C0`, `$01`, `$41`, ... Simplifies certain OAM processing methods.

Note on PPU ports: Writing a port increments its respective address. Reading does not.

## VRAM

`$0000-$03FF`: BG0 tilemap  
`$0400-$07FF`: BG1 tilemap  
`$0800-$0BFF`: BG2 tilemap  
`$0C00-$0FFF`: BG3 tilemap  
(All 64KiB of VRAM is accessible for BG or sprite tiles, but using the first 4KiB will usually have silly results.)

Tile data is stored planar style, lowest plane first. Background and Overlay data is 2 bits per pixel, while Sprite data is 3 bits per pixel. Tile data is stored in single-plane blocks of 8x8; a Background tile thus consists of 8 bytes of low-plane data followed by 8 bytes of high-plane data. (This is the format understood by most tile editors.)

### BG mode 1

In mode 1 (bit 2 of `$0202` is clear), the tilemap consists of a 32x30 array of 8-bit tile numbers, followed by a 16x15 array of 2-bit palettes, each of which applies to a block of 2x2 tiles. (This layout is exactly the same as that of the NES.) Background screens are then 256x240 pixels.

### BG mode 2

In mode 2 (bit 2 of `$0202` is _set_), the tilemap consists of a 32x32 array of bytes. Background screens are then 256x256 pixels. The high 6 bits of each byte are the high 6 bits of that tile's tile number and the low 2 bits are the palette number for that tile. Bit 0 of the tile number is 0 for even rows and 1 for odd rows, and bit 1 of the tile number is 0 for even columns and 1 for odd columns.

If you write the same value to each 2x2 block, like so:

    AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPP
    AABBCCDDEEFFGGHHIIJJKKLLMMNNOOPP
    QQRRSSTTUUVVWWXXYYZZ001122334455
    QQRRSSTTUUVVWWXXYYZZ001122334455
    ...

then it is as if we are working with 16x16 tiles instead of 8x8 tiles, and we had an array like:

    ABCDEFGHIJKLMNOP
    QRSTUVWXYZ012345
    ...

The "splat" DMA mode simulates this, though if you don't use it then you can freely specify different palettes on a per-tile basis.

## Palette

The ARS palette is based on HSV. `$00`, `$10`, ..., `$50` is a ramp of increasingly bright grays. With `x` in the range `$1`-`$d`, `$1x`, `$3x`, and `$5x` are dark, medium, and bright fully-saturated versions of hue `x`, and `$0x`, `$2x`, and `$4x` are dark, medium, and bright half-saturated versions of hue `x`. All color values `$60`-`$FF` are black.

This layout may seem odd, but it means that subtracting `$20` from a color value gives you a darker version of the color. Therefore, assuming valid color inputs, writing `$E0` or `$C0` to the color mod register is like reducing the brightness. A simple way to do a fade out is to write `$E0`, then `$C0`, then disable video output entirely; and for a fade in, write `$C0`, then `$E0`, then `$00`.

<figure>
<figcaption>Hues</figcaption>
<table>
<thead>
<tr><th>Index</th><th>Hue</th><th>Colors</th></tr>
</tr>
</thead>
<tbody>
<tr><td>$0</td><td>White</td><td><img src="img/hue0.png"></img></tr>
<tr><td>$1</td><td>Red</td><td><img src="img/hue1.png"></img></tr>
<tr><td>$2</td><td>Orange</td><td><img src="img/hue2.png"></img></tr>
<tr><td>$3</td><td>Pale Orange</td><td><img src="img/hue3.png"></img></tr>
<tr><td>$4</td><td>Yellow</td><td><img src="img/hue4.png"></img></tr>
<tr><td>$5</td><td>Yellow-Green</td><td><img src="img/hue5.png"></img></tr>
<tr><td>$6</td><td>Green</td><td><img src="img/hue6.png"></img></tr>
<tr><td>$7</td><td>Cyan</td><td><img src="img/hue7.png"></img></tr>
<tr><td>$8</td><td>Turquoise</td><td><img src="img/hue8.png"></img></tr>
<tr><td>$9</td><td>Aqua</td><td><img src="img/hue9.png"></img></tr>
<tr><td>$a</td><td>Blue</td><td><img src="img/hueA.png"></img></tr>
<tr><td>$b</td><td>Deep Purple</td><td><img src="img/hueB.png"></img></tr>
<tr><td>$c</td><td>Purple</td><td><img src="img/hueC.png"></img></tr>
<tr><td>$d</td><td>Magenta</td><td><img src="img/hueD.png"></img></tr>
</tbody>
</table>
</figure>

<figure>
<figcaption>Whole palette</figcaption>
<img src="img/full_palette.png">
</figure>

**Note that `$00` is not completely black!**

The ARS PPU outputs one of these colors for each pixel. Its output is hooked directly to a ROM that contains these color values in RGB. Normally that ROM controls an NTSC signal generator, but the board also has an unsoldered header (perfect for adding a DB-25 plug) that allows direct access to the digital RGB signal. It exposes 18 color pins, an HSYNC pin, a VSYNC pin, two ground pins, two +5V pins, and leaves one pin floating. There are circuit diagrams floating around out there for converting this signal into various analog signal formats.

## Sprites

### SSM

256 bytes, 4 per sprite.

`$0`: X coordinate  
`$1`: Y coordinate (>=240 effectively disables the sprite)  
`$2`:

- Bit 0: Horizontal flip
- Bit 1: Vertical flip
- Bit 2: Foreground bit
- Bit 3-7: Bits 3-7 of tile address

`$3`: High 8 bits of tile address

(Low 3 bits of tile address are always zero.)

The coordinates are the pixel position of the upper-leftmost pixel occupied by the sprite on the screen, regardless of the horizontal and vertical flip bits.

SSM port access will encounter bus conflicts during the final 192 cycles of an H-blank, and are safe at other times.

### SAM

64 bytes, 1 per sprite.

- Bit 0-2: Palette number
- Bit 3-7: (Height in tiles-1)

SAM port access will encounter bus conflicts during the cycle before each 3-cycle "sprite prefetch" block of the H-blank, and will also encounter a bus conflict in every "dot out" cycle whose dot comes from a sprite. Safe at other times.

## Overlay

If any of the high 4 bits of `$0202` are set, the Overlay is enabled. It is very similar to the background layer, with some differences:

- Tilemap and tiles are stored in main memory (Work RAM or cartridge space), not VRAM
- Not scrollable
- Palette index 0 is transparent, and others are always "in front" of everything else

Having the Overlay enabled steals about 11.25% of main CPU cycles. There are plenty of those to go around, but if you really need every cycle you can get a small speedup by disabling the Overlay when it's not in use.

A major advantage of the Overlay over other types of graphics is that it can be written freely and safely at all times. SimpleConfig, included with the ARS emulator, uses the Overlay along with some scanline trickery to create a 2-bpp framebuffer.

The Overlay is a 32x28 tilemap. The first and last rows are repeated to produce a full 256x240 pixel image. (The repeated rows are normally invisible due to overscan.) Tile numbers are stored straightforwardly, one byte per tile, starting at `$7C00`. Each tile has a corresponding bit starting at `$7F80` which allows a choice between two palettes.

# Controller ports

## Pinout

The controller ports use a DA-12 connector. Looking at the (female) controller plug, pins are numbered starting in the upper left and going left to right, top to bottom. There should be two rows of six pins, with the upper row offset to the right of the lower row.

1. Vcc (+5V)
2. Blank
3. D7#
4. D6#
5. D5#
6. D4#
7. D3#
8. D2#
9. D1#
10. D0#
11. Strobe
12. GND

Note that Dx# are active-low. The D pins are pulled high (logic 0), and driven low (logic 1).

The Blank pin is used by the light pen and light gun to synchronize with a CRT display; other controllers leave it unconnected. It's driven to a *positive* voltage when the PPU is *not* outputting video, and (almost) to zero volts when it *is*. The voltages vary wildly depending on temperature, phase of the moon, etc. but are usually in the [-0.1, +0.2] volt range in frame and [+0.9, +1.5] volt range out of frame. Somehow, this was enough for pretty reliable phase lock...

## Access

`$0240`: Controller port 1  
`$0241`: Controller port 2

Writing to a port drives Strobe to +5V and any active Dx pins to 0V. Reading from a port drives Strobe to 0V and returns what is currently sensed on the Dx# pins. The read should be performed twice to allow console-driven 1s to drain out and to allow time for the controller to drive the lines.

Controllers return a fixed ID when `$80` is written. ET games give a "no controller" message if `$00` is read, or "wrong controller" if something other than `$01` is read.

In theory, this interface can be used to provide ~1.5MB/s transfers under CPU control.

## Standard controller

`write $00`: Get button state  
`write $80`: Get controller ID (`$01` for the standard controller)

Button state:

- Bit 0: A
- Bit 1: B
- Bit 2: C
- Bit 3: Hat Left
- Bit 4: Hat Up
- Bit 5: Hat Right
- Bit 6: Hat Down
- (Bit 7 always 0)

Pause button asserts all Hat bits (bits 3-6).

## Keyboard

A 70-key keyboard with mechanical switches. Apart from a few oddities (like Control's placement and the twin delete keys), this is remarkably similar to a modern PC keyboard. I find it pretty pleasant to type on.

Keyboards are normally plugged into the first port (`$0240`).

`write $00`: Get next key press/release  
`write $80`: Get controller ID (`$02` for a keyboard)

- Bit 0-6: Key scancode
- Bit 7: 0 ("positive") for press, 1 ("negative") for release

<figure>
<figcaption>Keyboard layout with key caps</figcaption>
<img src="img/keyboard.svg">
</figure>

<figure>
<figcaption>Keyboard layout with scancodes</figcaption>
<img src="img/keycodes.svg">
</figure>

Scancode table:

- `$01`: Left Shift
- `$02`: Left Alt
- `$03`: Left Meta
- `$04`: Control
- `$05`: Right Shift
- `$06`: Right Alt
- `$07`: Right Meta
- `$08`: Escape
- `$09`: Backward Delete
- `$0A`: Forward Delete
- `$0B`: Insert
- `$0C`: Break
- `$0D`: Home
- `$0E`: End
- `$0F`: Page Up
- `$10`: Page Down
- `$11`: Left Arrow
- `$12`: Up Arrow
- `$13`: Right Arrow
- `$14`: Down Arrow
- `$15`: Enter
- `$16`: Tab
- `$17`: Space
- `$18`: \` ~
- `$19`: 1 !
- `$1A`: Q
- `$1B`: A
- `$1C`: Z
- `$1D`: 2 @
- `$1E`: W
- `$1F`: S
- `$20`: X
- `$21`: 3 #
- `$22`: E
- `$23`: D
- `$24`: C
- `$25`: 4 #
- `$26`: R
- `$27`: F
- `$28`: V
- `$29`: 5 %
- `$2A`: T
- `$2B`: G
- `$2C`: B
- `$2D`: 6 ^
- `$2E`: Y
- `$2F`: H
- `$30`: N
- `$31`: 7 &amp;
- `$32`: U
- `$33`: J
- `$34`: M
- `$35`: 8 *
- `$36`: I
- `$37`: K
- `$38`: , &lt;
- `$39`: 9 (
- `$3A`: O
- `$3B`: L
- `$3C`: . &gt;
- `$3D`: 0 )
- `$3E`: P
- `$3F`: ; :
- `$40`: / ?
- `$41`: - _
- `$42`: [ {
- `$43`: ' "
- `$44`: \ |
- `$45`: = +
- `$46`: ] }

## Mouse

Mice are normally plugged into the second port (`$0241`).

`write $00`: Get button state  
`write $40`: Get (and reset) the X movement counter  
`write $80`: Get controller ID (`$03` for a mouse)  
`write $C0`: Get (and reset) the Y movement counter

Button state:

- Bit 0: Left button
- Bit 1: Right button
- (Bit 2-7 always 0)

X/Y movement is an 8-bit two's complement integer.

Example simplified read procedure (cursor bounding logic is left out):

```6502
    ; Read button state
    STZ $0241
    LDA $0241
    STA g_MouseButtons
    ; Read X movement
    LDA #$40
    STA $0241
    LDA $0241 ; (garbage read)
    LDA $0241
    ; ...and add it to the cursor position
    CLC
    ADC g_CursorX
    STA g_CursorX
    ; Read Y movement
    LDA #$C0
    STA $0241
    LDA $0241 ; (garbage read)
    LDA $0241
    ; ...and add it to the cursor position
    CLC
    ADC g_CursorY
    STA g_CursorY
```

## Light pen

Same ID as the light gun. Button bits 3-6 are always 0 on the light pen; this lets you tell it apart from the light gun.

Light pens are normally plugged into the second port (`$0241`).

`write $00`: Get button state
`write $40`: Get X position in frame, 0-255. Only valid if the in-frame bit is 1.  
`write $80`: Get controller ID (`$04`)  
`write $C0`: Get Y position in frame, 0-239. Only valid if the in-frame bit is 1.

Button state:

- Bit 0: "Press" registered
- Bit 1: Grip button is currently held ("eraser mode"?)
- (Bit 2-6 always 0)
- Bit 7: In-frame

## Light gun

A needlessly realistic (and heavy) accessory that looks way too much like an M16A2 assault rifle...

Same ID as the light pen. Button bits 3-6 are always 0 on the light pen; this lets you tell it apart from the light gun.

Light guns are normally plugged into the second port (`$0241`).

`write $00`: Get button state
`write $40`: Get X position in frame, 0-255. Only valid if the in-frame bit is 1.  
`write $80`: Get controller ID (`$04`)  
`write $C0`: Get Y position in frame, 0-239. Only valid if the in-frame bit is 1.

Button state:

- Bit 0: Main trigger
- Bit 1: Underbarrel trigger
- Bit 2: Thumb button (reload)
- Bit 3: Auto selected
- Bit 4: Burst selected
- Bit 5: Single selected
- Bit 6: Safe selected
- Bit 7: In-frame

Exactly one of bits 3-6 is normally 1. Reading all four as 0, or more than one as 1, means you should reuse the read from the previous frame for debounce purposes.

# Developer cartridge

Official "developer cartridges" for the ARS have a lot of special hardware. Reverse engineering the whole cartridge would be hard, given how complex it is, and not very useful, given the limitations compared to a modern solution. Nevertheless, all information I have is documented here.

If anybody manages to find actual documentation for the developer cartridges, please let me know. There are lots of old, working dev cartridges out there, but as far as I know no copies of the manuals that came with them have survived.

## Serial port

The serial port is the only "developer cartridge" feature specifically implemented by the emulator. It only emulates the write end of the port, and it emulates it at infinite speed; write a byte to `$0247` and that byte will be written to stderr, read a byte and the V bit will be set.

Emulation of the debug port must be enabled with a command line option *and* specified in the game's manifest in order to function.

(If you are implementing an emulator or a homebrew program, you probably do not need the rest of this section.)

Developer cartridges have an RS-232 port built-in. It's a standard DB-25 connector, except that the cartridge side seems to require +12V on pin 9 and -12V on pin 10. There is evidence that this was compatible with the serial ports used on Eiling minicomputers, but unfortunately more information on that topic is hard to come by. (Most third-party development couldn't actually have been on Eiling minicomputers—they weren't exactly common—but I haven't yet found any information on adapters, etc. for non-Eiling hardware.)

When the ARS is running, the port is mapped to `$0247`. Reading it returns the next byte in the read buffer, or sets the V bit if another byte isn't available yet. Writing it puts another byte into the write buffer, or sets the V bit if there wasn't yet room in the buffer. The serial port is hardcoded to 19200 baud, 8 data bits, 1 stop bit, odd parity. Hardware flow control is used, and several of the more exotic pins seem to be connected to the IPL, but I haven't found any way to do anything with those from ARS software.

## IPL

Game Folders may specify arbitrary ROM/RAM mappings, including those supported by the Developer Cartridge and its IPL. That is as close as ARS-emu comes to supporting the Dev Cartridge IPL.

(If you are implementing an emulator or a homebrew program, you probably do not need the rest of this section.)

The cartridge includes a built-in microcontroller that is active whenever the console is in reset or powered off. It allows rewriting and inspection of the program RAM, battery-backed RAM, and other glue logic. It communicates with a host system over the serial port. It seems to be a compact binary protocol of some kind. I didn't get far in reverse engineering it.

Bizarrely, the IPL includes its own 32KiB RAM chip that maintains a shadow copy of the WRAM. I guess this was useful for debugging?

Glue logic state that can be affected by the IPL includes the BSx pins.

## Memory slots

(If you are implementing an emulator or a homebrew program, you probably do not need this section.)

The cartridge contains four 30-pin SIMM slots. The four slots are identical, and are labeled as follows:

1. ROM1
2. ROM2
3. DRAM
4. SRAM

SIMMs must be capable of single-cycle operation at 12.273MHz. If there's a way to communicate to the IPL or the glue logic how large the modules are, I wasn't able to find it.

The cartridge also contains a bank of eight DIP switches that control how the B6/B7 pins select between the slots. The switches are grouped in pairs, and each pair controls which chip is selected by a given B6/B7 combination. These switches can be used to describe the mappings of every official cartridge. (Coincidence?!)

# APU

It is clocked at half the main system clock, and it accesses its RAM during the first phase of each cycle. The upshot from the MPU's perspective is that writes to `$0220-$023F` will take one extra cycle on even-numbered MPU cycles. This extra access latency is often not emulated for performance reasons.

Note that even though `$0227` is not used, writes to this address can still stall the MPU by one cycle! (Maybe this is useful for gaining phase lock?)

- `$0220-$0226`: Voice rates, low
- `$0228-$022E`: Voice rates, high + slide
- `$022F`: Noise period (in units of 1/16 APU clock == 1/32 main clock)
- `$0230-$0236`: Voice waveforms
- `$0237`: Noise waveform
- `$0238-$023E`: Voice volumes
- `$023F`: Noise volume

## Noise

The APU uses a 15-bit LFSR to produce white-ish noise. The APU "block clock" (1/16th APU clock, 1/32nd main clock) is divided by `$022F`+1 to clock the LFSR. Thus, higher `$022F` values produce a greater divisor, and therefore noise with a lower "pitch". In each block, the current output of the LFSR (1 or 0) is added to a counter, and in the final block that counter is modulated into the output samples as if it were an eighth voice output.

The volume register is just as with the voices. The high bit resets the LFSR.

The noise waveform register makes things a little more complicated. The low 7 bits control whether that block advances the period. If bit 0 is set, the period does not advance in block 0, and etc. all the way up to bit 6. Bit 7 is different. Block 7 always advances; bit 7 instead controls which bit of the LFSR is the second tap. If bit 7 is 0, the tap is bit 1, making white noise. If it is 1, the tap is bit 6, making hideous periodic noise. In either case, the noise produced is uncannily like a NES's.

Some useful noise waveforms:

- `$00`: Plain white noise, full rate
- `$55`: Plain white noise, half rate (half of blocks are skipped)
- `$77`: Plain white noise, quarter rate (3/4 of blocks are skipped)
- `$80`: Hideous periodic noise, full rate
- `$D5`: Hideous periodic noise, half rate
- `$F7`: Hideous periodic noise, quarter rate

This noise generator is capable of closely matching a NES's noise generator. It could be a coincidence; a 15-bit LFSR happens to be a convenient way to generate white noise on the cheap. On the other hand, bit 7 of the waveform register perfectly mimics a bizarre feature of the NES APU...

Here are the ET209 period and waveform values that come closest to matching particular NTSC NES period values:

<table>
<thead><tr><th>NES</th><th>Period</th><th>Waveform</th><th>NES</th><th>Period</th><th>Waveform</th></tr></thead>
<tbody>
<tr><td>$0</td><td>$00</td><td>$00</td><td>$1</td><td>$01</td><td>$00</td></tr>
<tr><td>$2</td><td>$02</td><td>$00</td><td>$3</td><td>$06</td><td>$00</td></tr>
<tr><td>$4</td><td>$0D</td><td>$00</td><td>$5</td><td>$14</td><td>$00</td></tr>
<tr><td>$6</td><td>$1A</td><td>$00</td><td>$7</td><td>$21</td><td>$00</td></tr>
<tr><td>$8</td><td>$2A</td><td>$00</td><td>$9</td><td>$35</td><td>$00</td></tr>
<tr><td>$A</td><td>$51</td><td>$00</td><td>$B</td><td>$6C</td><td>$00</td></tr>
<tr><td>$C</td><td>$A2</td><td>$00</td><td>$D</td><td>$D9</td><td>$00</td></tr>
<tr><td>$E</td><td>$D9</td><td>$55</td><td>$F</td><td>$D9</td><td>$77</td></tr>
</tbody>
</table>

(Note that the values in FamiTracker are backwards; F = 0, E = 1, etc.)

## Voices

Rate is the value (minus one) added to the accumulator on each output sample. The mapping between rate values and frequencies is:

    freq = rate * 47940.341 / 65536
    rate = freq * 65536 / 47940.341

As an example, assuming you wanted to output a standard "middle A" (440Hz):

    rate = 440 * 65536 / 47940.341 = 601.49426137791...

A rate value of 601 is as close as you can get. It works out to be within 0.5Hz of the correct frequency. Subtract 1, and the value you write to the rate register is 600.

The high 2 bits of the rates control hardware pitch slides.

`00`: Instantaneous change  
`01`: Four samples per 1-unit change (~8767.2Hz/s)  
`10`: Eight sample per 1-unit change (~4383.6Hz/s)  
`11`: Sixteen samples per 1-unit change (~2191.8Hz/s)

Waveform bits:

- Bit 0: Invert 0-12.5%
- Bit 1: Invert 0-25%
- Bit 2: Invert 0-50%
- Bit 3: Invert all (toggled on carry if bit 4)
- Bit 4: Toggle "invert all" bit on carry
- Bit 5: Output accumulator
- Bit 6 and 7: Pan (see below)

Invert bits stack. e.g. if bit 0 and 1 are set, this results in an inverted 12.5-25%.

Some useful waveforms:

- `$10`: Simple square wave (1/2 rate)
- `$01`: 12.5% duty cycle pulse wave, like NES duty cycle 0 (1x rate)
- `$02`: 25% duty cycle pulse wave, like NES duty cycle 1 (1x rate)
- `$04`: Simple square wave, like NES duty cycle 2 (1x rate)
- `$09`: Inverted 25% like NES duty cycle 3 (1x rate)
- `$20`: Sawtooth wave (1/2 rate)
- `$30`: Triangle wave (1x rate)
- `$F0`: Boosted triangle wave, good for bass

Pan values:

- `00`: Center. The voice will be played on both speakers at half volume. (Unless the speakers are out of phase, this results in the same volume as Left or Right.)
- `01`: Right. The voice will be played at full volume, but only on the right speaker.
- `10`: Left. The voice will be played at full volume, but only on the left speaker.
- `11`: Boosted. The voice will be played at full volume in both speakers. This is used almost exclusively with triangle waves. (Unless the speakers are out of phase, this results in double the volume of Center/Left/Right.)

Volume bits:

- Bit 0-6: Volume (0=mute, 64=max)
- Bit 7: Reset Accumulator. Should be set on each note-on. Internally set to 0 each tick.

## PCM

If you have nothing better for the main CPU to do, you can perform bit-banged PCM. Write the magnitude to the volume register, and write a waveform of `$00` for negative polarity and `$08` for positive polarity. Wait until 256 MPU cycles have passed, then repeat. Pray for phase-lock. This won't work with most emulators.

## Timing

There are 128 APU clocks per output sample. They are divided into 8 blocks of 16 cycles each. Noise processing occurs at the beginning of each block. The first 7 blocks process each voice in turn. In the eighth block, the noise and voices are mixed, and then the output to the DAC is updated.

Emulators don't usually emulate the precise timing below the sample level, for performance reasons. Some even process the APU in chunks, once per frame; while this generally works well, this breaks the PCM trick.

Voice block cycle timings:

1. Read target rate high byte
2. Read actual rate high byte, begin comparison for slide
3. Read target rate low byte
4. Read actual rate low byte
5. Write actual rate low byte, load accumulator into first adder A
6. Write actual rate high byte, load actual rate into first adder B
7. Read volume register into multiplier A, write {Q,0} (depending on reset flag) into accumulator
8. Evalute waveform, load waveform sample into multiplier B
9. Add LFSR state to noise counter
10. Read noise waveform, increment noise accumulator, clock LFSR on overflow
11. (dead cycle)
12. (dead cycle)
13. (dead cycle)
14. Read multiplier Q, shift/mask and write to both adder As
15. Read sample accumulators into adder Bs
16. Write adder Qs to sample accumulators

Mix block cycle timings:

1. (dead cycle)
2. (dead cycle)
3. (dead cycle)
4. (dead cycle)
5. (dead cycle)
6. (dead cycle)
7. Read noise volume register into multiplier A, reset noise accumulator if reset flag was set in volume register
8. Write noise counter into multiplier B
9. Add LFSR state to noise counter (which is zeroed at the rising edge of this cycle)
10. Read noise waveform, increment noise accumulator, clock LFSR on overflow
11. (dead cycle)
12. (dead cycle)
13. (dead cycle)
14. Read multiplier Q into both adder As
15. Read sample accumulators into both adder Bs
16. Write adder Qs to DAC latches, and 0 to sample accumulators

On any cycle where a particular meaningful read or write does not take place, the last address accessed is generally read.

# Cartridge Memory Map

The ARS can be thought of as providing a 20- to 23-bit address bus, depending on the bank size. The high 8 bits of the address come from the appropriate Bank Select register, and the remaining low bits from the accessed address.

The cartridge connector has two pins, BS0 and BS1, which effectively control the bank size. Cartridges usually hardwire them to a particular logic value, and connect (or don't connect) their Ax/Bx pins as needed.

- 0: 1x32KiB slot
  - `$8000-$FFFF`: Bank Select 0 (`$0248`)
- 1: 2x16KiB slots
  - `$8000-$BFFF`: Bank Select 0 (`$0248`)
  - `$C000-$FFFF`: Bank Select 4 (`$024C`)
- 2: 4x8KiB slots
  - `$8000-$9FFF`: Bank Select 0 (`$0248`)
  - `$A000-$BFFF`: Bank Select 2 (`$024A`)
  - `$C000-$DFFF`: Bank Select 4 (`$024C`)
  - `$E000-$FFFF`: Bank Select 6 (`$024E`)
- 3: 8x4KiB slots
  - `$8000-$8FFF`: Bank Select 0 (`$0248`)
  - `$9000-$9FFF`: Bank Select 1 (`$0249`)
  - `$A000-$AFFF`: Bank Select 2 (`$024A`)
  - `$B000-$BFFF`: Bank Select 3 (`$024B`)
  - `$C000-$CFFF`: Bank Select 4 (`$024C`)
  - `$D000-$DFFF`: Bank Select 5 (`$024D`)
  - `$E000-$EFFF`: Bank Select 6 (`$024E`)
  - `$F000-$FFFF`: Bank Select 7 (`$024F`)

When reset falls, all Bank Select registers are initialized to the value currently present on Bx. The cartridge should weakly pull those pins either up or down to give the Power On Bank number.

## The Technical Truth

Internally, the ARS always uses Bank Select 0 for `$8xxx`, Bank Select 1 for `$9xxx`, etc. The BSx pins actually control the granularity of writes to the Bank Select registers. With a value of 0, for example, writing any address from `$0248-$024F` actually writes that value to all eight Bank Select registers. With a value of 1, writing an address from `$0248-024B` writes Bank Select registers 0 through 3, etc. This starts to become noticeable if you have BSx other than 3, and write to the "missing" Bank Select registers.

# "Fake" hardware

## Homebrew Achievements Module

Provides access to Steamworks-alike features, whether offered by Steam or by another service such as XBLA. Also provides the ability to create "cheat tolerance" features, so that, for instance, cheating may be allowed but disable the awarding of achievements on a given save slot.

The emulator will load a cartridge which contains a HAM, but will always ignore writes and return 0 on reads. When, in the future, the HAM interface is defined, its presence and usability will be indicated by a non-zero "cold read" (i.e. without any other command being sent), similar to the emulator configuration device.

## Emulator configuration

Provides generic access to emulator configuration. Homebrew games can use this to provide in-game configuration menus. The built-in configuration system for the emulator (SimpleConfig) uses this.

If (and ONLY if) the Configuration port is enabled on the command line, *and* the ROM image has bit 7 set in its expansion hardware flags, will the port be fully enabled. If the port is neither enabled nor disabled on the command line, and bit 7 is set in the expansion hardware flags, the port will be enabled in "secure" mode. (See below.)

When the port is enabled, a cold read (no other command in progress), returns `$EC`, and whenever a byte is written to the port, any previous command result is discarded.

If the Configuration port is not enabled, reads will normally be open bus.

Recommended handshake sequence:

    ; Discard any leftover command
    LDA #$FF
    STA r_EmuConfig
    STA r_EmuConfig
    STA r_EmuConfig
    ; Test cold read
    LDA r_EmuConfig
    CMP #$EC
    BNE _notPresent
    ; Echo sequence, unlikely on an open bus, even a badly emulated one
    STZ r_EmuConfig
    LDA #$22
    LDX #$55
    STA r_EmuConfig
    STX r_EmuConfig
    CMP r_EmuConfig
    BNE _notPresent
    CPX r_EmuConfig
    BNE _notPresent
    ; If execution reaches here, the device is present and usable

This is sufficient if you are just planning to jump into SimpleConfig. To determine if you have permission to run non-standard configuration code (which I don't recommend writing in the first place), an additional check is required:

    ; Read menu cookie
    LDA #$0B
    STA r_EmuConfig
    LDA r_EmuConfig
    ; If the Configuration Port is working, the menu cookie will never be $EC
    CMP #$EC
    BEQ _customConfigCodeNotAllowed

When the emulator is in its default, "secure" configuration mode, all reads and writes to the Emulator Configuration port are open bus, just like if the port is disabled... *unless* an untampered, known-good SimpleConfig program is present in the `$F000-$F7FF` region. If that code is present, all reads to the port from outside that code return `$EC`, and all writes are ignored. To summarize, there are three possible outcomes to program against:

- Handshake fails. Configuration is disabled (or SimpleConfig is not mapped correctly)
- Handshake succeeds, but reading the menu cookie gives `$EC`. Custom configuration code will not run, but jumping into SimpleConfig will work correctly.
- Handshake succeeds, and reading the menu cookie gives something other than `$EC`. Custom configuration code will run.

To reiterate, it will always be safe to activate SimpleConfig if the standard handshake succeeds. **You only need to care about the difference between "secure" and fully enabled mode if you are writing your own configuration code.** (Which, again, you probably shouldn't do.)

When reading a rendered string, read a byte giving the number of columns, then sixteen bytes per column giving a two-tile-high 1-bit bitmap containing the rendered text. Rendered strings will never contain more than 28 columns.

Commands:

- `$00`, `A`, `B` - Echo two  
  Read results in bytes `A`, `B`. Used as part of the recommended handshake sequence.
- `$01` - Init menu system  
  [Re-]Initializes the menu system.
- `$02` - Cleanup menu system  
  Tears down any state relating to the menu system. It's good practice to do this when `$04` returns 0. If you don't do this, freezing won't work. (If you want to intentionally disable freezing, use the HAM instead.)  
  This may reset the emulated system, for example if the user has selected a different CPU core.
- `$03` - Render menu title  
  Render title of the currently-active menu. (See `$06`)
- `$04` - Get number of menu items  
  Read a single byte giving the number of items in this menu, or 0 if configuration has finished.
- `$05`, `n` - Get type of item `n`  
  Read a single byte giving the type of item `n`:
  - `$00`: Divider (unselectable)
  - `$01`: Text label (unselectable)
  - `$02`: Simple button
  - `$03`: Back button
  - `$04`: Apply button
  - `$05`: Apply-and-back button
  - `$06`: Submenu
  - `$07`: Selector (can be reverse-selected)
  - `$08`: Key config (must be keybound instead of activating)
  - `$ff`: No item (past the end of the menu)
  - The only negative type code is the "no item" type code, so BMI can be used to break out of a rendering loop early.
  - The unselectable type codes are `$00` and `$01`, so a comparison with `$02` can safely be used to determine whether a given item is selectable.
  - A menu will *always* have at least one selectable item.
- `$06`, `n` - Render menu item labels  
  First, a bitfield describing which labels are present. Then, the labels, in the order given below.
  - `$01`: Left-aligned label
  - `$02`: Right-aligned label
  - `$04`: Left-aligned label, dim
  - `$08`: Right-aligned label, dim
- `$07`, `n`: Activate button / enter submenu
- `$08`: Back out of menu  
  Activating a Back button should have the same effect as this command, but you should still use `$07` to activate any Back button..
- `$09`, `n`: Establish new key binding for item `n`  
  Read a byte giving a number of strings (one per line) to display to the user which provide key binding instructions. Then read those strings. (There will never be more than 12.) Then read a controller type byte, then a button index. (These can be used to render a visual aid to the user.) Then, every frame, read until the result is non-zero. A non-zero result will be returned when the user has completed the key binding operation, at which point the previous menu will be revisited, though some strings may have changed.  
  Calling this on anything that isn't a key config item is a bad idea.
- `$0A`, `n`: Change selection in reverse direction (for selectors only)
- `$0B`: Read menu cookie  
  Reads a value that will change whenever the currently active menu changes. This cookie will never equal `$EC`; this is important in "secure" configuration mode.
- `$FF`: Guaranteed to do nothing (except that, like any other command, it discards any previous command result)

The Emulator Configuration port is always disabled in movie recording and playback.

### Embedding SimpleConfig

Unless requested otherwise on the command line, the emulator *will* trust SimpleConfig with configuration port manipulation, even if it won't trust third-party code. If you follow the directions below, your ROM will have access to the emulator configuration port by default... as long as it only accesses it through SimpleConfig.

    .ORGA $F000
    .SECTION "!SimpleConfig" FORCE
    SimpleConfigEntryPoint:
    .INCBIN "path/to/SimpleConfig.etars/config.rom"
    .ENDS

(`SimpleConfig.etars` is what you get if you unzip `SimpleConfig.etarz`.)

With a proper `.BANK` directive, this will put SimpleConfig into your ROM. SimpleConfig expects to be mapped to `$F000` through `$F7FF`.

**Make sure SimpleConfig's bank is mapped**, then use the standard handshake to determine if the Emulator Configuration port is present. If it is, you could (for example) present a menu option within your game's menu system to enter the configuration screen, confident that jumping into SimpleConfig will work as desired. (However, this does not necessarily mean that custom code accessing the Emulator Configuration port will work.)

Don't forget to add the appropriate `expansion` tag to your manifest!

Then, use code like the following (assuming the standard ET license block is present and active):

    enterSimpleConfig:
        ; Disable video output
        STZ r_Multi
        ; (wait for this to take effect)
        JSR awaitNMI
        ; Set up NMI and IRQ handlers for SimpleConfig
        LDA $f7fa
        STA g_NMIHandler
        LDA $f7fb
        STA g_NMIHandler+1
        LDA $f7fe
        STA g_IRQHandler
        LDA $f7ff
        STA g_IRQHandler+1
        ; We can do whatever BG/SP setup we want here, and it will remain
        ; mostly untouched by SimpleConfig. This example just sets them up to
        ; be black.
        ; (SimpleConfig will overwrite all four BGPalette registers to $00
        ; outside the "hilight region" and $0E inside it, but leave scroll,
        ; base tile addresses, etc. untouched.)
        LDX #9
    -   DEX
        STZ r_SPPalettes,X
        BNE -
        ; Zero color mod
        STZ r_ColorMod
        ; Black out most of the CRAM. (SimpleConfig will be using the top 32
        ; colors, though.)
        STZ r_CRAMIndex
        LDA #$FF
        LDX #256-32
    -   STA r_CRAMPort
        DEX
        BNE -
        ; Stub out all the sprites
        STZ r_SSMIndex
        LDX #64
    -   STZ r_SSMPort
        STA r_SSMPort
        STZ r_SSMPort
        STZ r_SSMPort
        DEX
        BNE -
        ; Next 16 colors are the hilight background (we're using purple)
        LDA #$1A
        LDX #16
    -   STA r_CRAMPort
        DEX
        BNE -
        ; The text palette
        LDA #$30
        LDY #$20
        JSR _loadPalSeg
        LDA #$4A
        JSR _loadPalSeg
        LDA #$50
        LDY #$30
        JSR _loadPalSeg
        JSR _loadPalSeg
        ; All set
        JSR SimpleConfigEntryPoint
        ; Whoever called us has some serious cleanup to do :)
        RTS
    
    _loadPalSeg:
        STY r_CRAMPort
        STA r_CRAMPort
        STY r_CRAMPort
        STA r_CRAMPort
        RTS

SimpleConfig will clobber RAM addresses `$00`, `$00EE-$00FF`, and `$4000-$7FFF`. It makes no assumptions about the initial values of these addresses. If the emulator is in "secure" configuration mode, and the configuration system is currently active, writes to these addresses from outside SimpleConfig are blocked.

<!--*- word-wrap: t; mode:markdown -*-->

# Clocks

The core clock of the ARS is exactly 135/11 MHz. This is because it's tied to the NTSC field rate of 59.94 fields per second. This results in the rather weird number 12.27272727...MHz for the core frequency, and this weirdness propagates downward to all the related clocks. However, computers output video at 60 frames per second, not 59.94. Therefore, it's convenient to multiply all these clocks by 1.001 when emulating on a computer. This makes games run 0.1% faster, and audio play back 1.73 cents out of tune, but everything else becomes so much easier that it's worth the hardly noticeable difference.

Here are all the interesting clock rates in the console:

| Clock   | NTSC                             | Emulated       | Notes    |
| ------- | -------------------------------- | -------------- | -------- |
| Core    | 135MHz/11 = 12.2727...MHz        |      12.285MHz |          |
| Dot,APU | 135MHz/22 = 6.13636...MHz        |      6.1425MHz | Core/2   |
| Audio   | 135MHz/2816 = 47.940340909...kHz | 47.98828125kHz | Core/256 |

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
2. NC
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

## Access

`$0240`: Controller port 1  
`$0241`: Controller port 2

Writing to a port drives Strobe to +5V and any active Dx pins to 0V. "Output" pins may be either 0 or 1. "Input" pins should be 0, _or_ the read should be performed twice to allow the driven 1s to drain out.

Reading from a port drives Strobe to 0V and returns what is currently sensed on the Dx# pins. This will be the logical OR of whatever was last written to the port and whatever the controller is driving. If full duplex communication is required, reading from the port twice consecutively _should_ allow the console-driven 1s to "drain out".

The standard controller returns a fixed ID when `$80` is written. (Don't forget to mask out that high bit on read!) Other controllers presumably would have returned a different ID. ET games give a "no controller" message if `$00` is read, or "wrong controller" if something other than `$01` is read.

In theory, this interface can be used to provide ~1.5MB/s transfers under CPU control.

## Standard controller

`port := $00`: Read button state  
`port := $80`: Read controller ID (`$01`; will read as `$81` on first read)

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

# Developer cartridge

Official "developer cartridges" for the ARS have a lot of special hardware. Reverse engineering the whole cartridge would be hard, given how complex it is, and not very useful, given the limitations compared to a modern solution. Nevertheless, all information I have is documented here.

If anybody manages to find actual documentation for the developer cartridges, please let me know. There are lots of old, working dev cartridges out there, but as far as I know no copies of the manuals that came with them have survived.

## Serial port

The serial port is the only "developer cartridge" feature specifically implemented by the emulator. It only emulates the write end of the port, and it emulates it at infinite speed; write a byte to `$0247` and that byte will be written to stderr, read a byte and the V bit will be set.

Emulation of the debug port must be enabled with a command line option *and* specified in the ROM file header in order to function.

(If you are implementing an emulator or a homebrew program, you probably do not need the rest of this section.)

Developer cartridges have an RS-232 port built-in. It's a standard DB-25 connector, except that the cartridge side seems to require +12V on pin 9 and -12V on pin 10. There is evidence that this was compatible with the serial ports used on Eiling minicomputers, but unfortunately more information on that topic is hard to come by. (Most third-party development couldn't actually have been on Eiling minicomputers—they weren't exactly common—but I haven't yet found any information on adapters, etc. for non-Eiling hardware.)

When the ARS is running, the port is mapped to `$0247`. Reading it returns the next byte in the read buffer, or sets the V bit if another byte isn't available yet. Writing it puts another byte into the write buffer, or sets the V bit if there wasn't yet room in the buffer. The serial port is hardcoded to 19200 baud, 8 data bits, 1 stop bit, odd parity. Hardware flow control is used, and several of the more exotic pins seem to be connected to the IPL, but I haven't found any way to do anything with those from ARS software.

## IPL

Image files can contain initialization data for DRAM and SRAM chips, and this is as close as the emulator comes to supporting the IPL.

(If you are implementing an emulator or a homebrew program, you probably do not need the rest of this section.)

The cartridge includes a built-in microcontroller that is active whenever the console is in reset or powered off. It allows rewriting and inspection of the program RAM, battery-backed RAM, and other glue logic. It communicates with a host system over the serial port. It seems to be a compact binary protocol of some kind. I didn't get far in reverse engineering it.

Bizarrely, the IPL includes its own 32KiB chip that maintains a shadow copy of the WRAM. I guess this was useful for debugging?

Glue logic state that can be affected by the IPL includes the BSx pins.

## Memory slots

(If you are implementing an emulator or a homebrew program, you probably do not need this section.)

The cartridge contains four 30-pin SIMM slots. The four slots are identical, and are labeled as follows:

1. ROM1
2. ROM2
3. DRAM
4. SRAM

SIMMs must be capable of single-cycle operation at 12.273MHz. If there's a way to communicate to the IPL or the glue logic how large the modules are, I wasn't able to find it.

The cartridge also contains a bank of eight DIP switches that control how the B6/B7 pins select between the slots. Not coincidentally, the switches work in exactly the same way as mapping type `$00` in the ROM file format, and can describe the mappings of every official cartridge.

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

- `$40`: Simple square wave (1/2 rate)
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

If you have nothing better for the main CPU to do, you can perform bit-banged PCM. Write the magnitude to the volume register, and write a waveform of `$00` for negative polarity and `$08` for positive polarity. Wait until 256 MPU cycles have passed, then repeat. Pray for phase-lock. This won't work with some emulators.

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

# ROM file format

- `$0-$2`: ASCII bytes "ARS"
- `$3`: `$1A` (control-Z = DOS end-of-file)
- `$4`: Mapping type (must be `$00`)
- `$5`: Expansion hardware flags
  - `$01`: Developer debug port
  - `$40`: homebrew achievements module
  - `$80`: emulator configuration module
- (remaining bytes describe mapping type 0)
- `$6`: ROM1 size
- `$7`: ROM2 size
- `$8`: SRAM size
- `$9`: DRAM size
- `$A`: Pin mapping
- `$B`: Bank mapping
- `$C`: Power On Bank
- `$D`: If not `$FF`, the hardwired bank number for overlay tile accesses
- `$E`: Best controller setup for this cartridge (soft constraint). Low nybble is controller port 1, high nybble is port 2.
    - `$0`: Standard controller
    - `$1`: Light gun
    - `$2`: Keyboard
    - `$3`: Light pen
    - `$4-$E`: Reserved
    - `$F`: No controller required
- `$F`: Reserved for expansion. A non-zero value is an error.

## Chips

- ROM1: The main ROM chip. Nearly every cartridge has one.
- ROM2: A secondary ROM chip. I'm not aware of any official cartridge that has one, but the developer cartridge has support for one. Supposedly it was intended for localization data, which would allow the same ROM1 to be used on all international versions with different ROM2s for different languages.
- DRAM: Non-persistent memory. On a real cartridge, this would almost certainly be an SRAM chip in practice... the difference being the lack of a battery. (Isn't 32KiB enough for you?!)
- SRAM: Persistent memory, i.e. battery-backed SRAM.

## Sizes

A size value of 0 means no chip of this type is present. The high bit is a flag indicating that initialization data for that chip is present in the ROM file. It is an error to have a non-zero size for a ROM chip without also having this flag set, or a zero size for any chip with this flag set.

Useful examples:

- `$0C`: 2KiB
- `$0D`: 4KiB
- `$0E`: 8KiB
- `$0F`: 16KiB
- `$10`: 32KiB
- `$11`: 64KiB
- `$12`: 128KiB
- `$13`: 256KiB
- `$14`: 512KiB
- `$15`: 1MiB
- `$16`: 2MiB
- `$17`: 4MiB
- `$18`: 8MiB

No larger value is useful under mapping type `$00`, as 8MiB is the size of the entire address space.

If initialization data is specified for the SRAM chip, it is only used if the emulator does not already have an SRAM image for this cartridge, or to fill in the end if the existing SRAM image is too small.

## Pin Mapping

- `$01`: A14-A0 are connected. BSx is `00` (1 slot, 32KiB banks, 23-bit space)
- `$42`: A13-A0 are connected. BSx is `01` (2 slots, 16KiB banks, 22-bit space)
- `$83`: A12-A0 are connected. BSx is `10` (4 slots, 8KiB banks, 21-bit space)
- `$C4`: A11-A0 are connected. BSx is `11` (8 slots, 4KiB banks, 20-bit space)

Technically, the low nybble gives the number of address pins that are not connected, and the high two bits give the hardwired BSx values. *All* known official cartridges can be described using these four example values. Values `$10-$3F`, `$50-$7F`, `$90-$BF`, and `$D0-$FF` (i.e values whose bitwise AND with `$30` is non-zero) are reserved, and should be considered invalid.

Layout types with "too few" disconnected Ax pins would effectively create separate bankspaces for each bank slot. There are situations where this would be useful, but as mentioned, no official cartridge does this.

## Bank Mapping

The same as the DIP switches on the developer cartridge. Low 2 bits control the mapping of the low 64 banks, next 2 bits control the next 64 banks, etc. Each 2 bits controls 1/4 of the total banks.

- `00`: ROM1
- `01`: ROM2
- `10`: DRAM
- `11`: SRAM

Examples:

- `$00`: ROM1 is mapped in all banks.
- `$A0`: ROM1 is mapped in low 128 banks. DRAM is mapped in high 128 banks.
- `$F0`: ROM1 is mapped in low 128 banks. SRAM is mapped in high 128 banks.
- `$F8`: ROM1 is mapped in low 64 banks. DRAM is mapped in next 64 banks. SRAM is mapped in high 128 banks.
- `$AC`: ROM is mapped in low 64 banks. SRAM is mapped in next 64 banks. DRAM is mapped in high 128 banks.

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
    .INCBIN "path/to/SimpleConfig.etars" SKIP 16
    .ENDS

(`SimpleConfig.etars` is what you get if you un-gzip `SimpleConfig.etarz`.)

With a proper `.BANK` directive, this will put SimpleConfig into your ROM. SimpleConfig expects to be mapped to `$F000` through `$F7FF`.

**Make sure SimpleConfig's bank is mapped**, then use the standard handshake to determine if the Emulator Configuration port is present. If it is, you could (for example) present a menu option within your game's menu system to enter the configuration screen, confident that jumping into SimpleConfig will work as desired. (However, this does not necessarily mean that custom code accessing the Emulator Configuration port will work.)

Don't forget to set bit 7 of byte `$5` of the header!

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

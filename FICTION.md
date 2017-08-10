<!--*- word-wrap: t; mode:markdown -*-->

The Eiling Technologies Artificial Reality System is an 8-bit video game console created by Eiling Technologies.

# Table of Contents
1. [History](#history)
2. [Technical Specifications](#technical)
3. [Problems](#problems)

# History<a name="history">

In 1985, Charles Eiling (founder and namesake of Eiling Technologies) retired, and was replaced as CEO by his son, Lyle Eiling. The new CEO foresaw the industry upheaval that the microcomputer revolution would soon bring to a head, and in November 1985 ordered the creation of a video game console.

As the years dragged on, the company poured more of its staff and money onto the project. Its existing engineers were highly skilled, but until that point had only worked on high-end electronics for corporations and militaries. The project severely overran its budget, and was not ready in time for its originally-announced mid-1986 release date.

When the Artificial Reality System finally became available in stores in July 1988, it faced numerous [problems](#problems), technical and otherwise. Approximately two million were manufactured, but less than 3,000 were ever sold. Eiling Technologies was losing billions of dollars per year. Since they neglected all other products in favor of the ARS, the industry upheaval of the late 1980's hit ET particularly hard. Without a viable product, ET finally declared bankruptcy in March 1991. It was broken up and sold to various companies for less than a penny on the dollar.

There are still a few Eiling Technologies mainframes in use today. It is also believed that ET manufactured some technology that is still used by the militaries of NATO countries.

The fate of the two million unsold ARS units is unknown.

# Technical Specifications<a name="technical">

## CPU<a name="cpu">

The main processor of the ARS is a [W65C02](https://en.wikipedia.org/wiki/WDC_65C02) running at 12.285MHz. This was excessive for the time, and few games made much actual use of this processing power.

## Memory<a name="memory">

The ARS is commonly quoted as containing 30.5KB of "Work RAM". In practice, nearly a full 32KiB can be used. It also contains 64KiB of Video RAM, 256 bytes of Sprite State Memory, and 64 bytes of Sprite Attribute Memory.

The console contains a simple bank-based memory mapper. Depending on the way the cartridge is wired, it gives access to between 1MiB and 8MiB of address space for cartridge data and hardware.

There is a six-byte hole in the work RAM space where no hardware is mapped. The developer cartridge maps its serial port in this space. The WCS# pin on the cartridge and expansion port could be used to provide additional memory or IO ports in the work RAM space. No known official cartridge does this, nor any official expansion hardware, but the emulator maps the "Homebrew Achievement Module" and the "Emulator Configuration Port" in this area.

## Graphics<a name="graphics">

The console generates a 256x240 video signal, of which most emulators display 240x224 for overscan reasons. The console generates square pixels, unlike its contemporaries. Though ET never officially endorsed or supported this, many users modify their ARS to output its internal RGB signal directly to a suitable monitor, giving a considerable improvement in quality. (It is believed that ET's own engineers, including its in-house game developers, may have used such setups.)

It has a hardware palette containing 85 colors, including 7 levels of gray. Each of the four background "regions" can choose a group of four four-color palettes to use for its background tiles, and a group of eight seven-color palettes to use for sprites within that region. On top of that, the overlay can choose a group of two three-color palettes. This means that, in theory, the ARS can display up to 182 different colors, even without scanline trickery. (Or it could, if its color generator weren't limited to 85 colors.)

The PPU contains 64KiB of VRAM, of which 4KiB is dedicated to the background tilemaps. The remaining 60KiB is used to store background and sprite tiles. The overlay tilemap is stored in the upper 1KiB of the CPU's work RAM, and the overlay tiles can be stored in any of 15 locations in the main memory space.

The background tilemap is divided into four regions, arranged in a square. There are two background modes. In mode 1, the regions are 32x30 tiles (256x240 pixels) and laid out in exactly the same way as the nametables on the NES. In mode 2, the regions are 32x32 tiles (256x256) pixels, and laid out in a packed format. Each mode has advantages and disadvantages, though most games that make use of vertical scrolling use mode 2 as it makes some of the math simpler.

The PPU also supports 64 hardware sprites. Each sprite can use one of eight region-specific palettes, depending on the background region. (A sprite that exists in more than one region can end up using more than one palette as a result.) Sprites are 8 pixels wide, and can be any multiple of 8 pixels up to 64 pixels tall. There is no limit on the number of sprites per scanline.

Finally, there is the overlay. It is a non-scrollable 32x28 tilemap, which is always "on top of" the other layers. It is usually used for on-screen text, and stationary interface elements during gameplay. The first and last row are repeated to make it 32x30; this is usually not visible due to overscan. When the overlay is enabled, it "steals" cycles from the main CPU to fetch its data, effectively slowing down the CPU by about 11.25%.

## Audio

The ARS contains a custom "digital synthesizer" chip. It theoretically outputs at just over 47988Hz&mdash;the main clock divided by 256. It supports seven "voices", each of which can be a square, sawtooth, triangle, pulse, or strange hybrid wave. It also has a noise generator that can output white or periodic noise at various bandwidths. It has limited stereo capability; each voice can either be Center, Left, Right, or Boosted. (Noise is always Boosted.)

## Controller

The console has two DA-12 controller ports. They are very flexible, but ET never sold any controller-port devices other than the standard controller. They had developed a light gun, a keyboard, and even a light pen, but these were never released in retail.

Unlike the console and its cartridges, the standard controller is cheap and internally simple. It has an eight-way hat switch, three face buttons (A, B, C), and a Pause button.

# Problems<a name="problems">

## Price<a name="price">

When the Nintendo Entertainment System debuted in North America in 1984, its price point was $100. The ARS, on the other hand, debuted four years later at a ridiculous $850 price point. The price dropped in subsequent years, eventually reaching a low of $50 just before ET's bankruptcy in 1991... by which time, the technically superior SNES and Sega Genesis were well-established in the market, destroying any chance it might have had.

In addition to the exhorbitant price point of the console, the high-speed 80ns ROMs required for the game cartridges meant games originally retailed for $60-100.

## Overheating<a name="overheating">

The ARS was powerful for an 8-bit system. The power came at a heavy cost. With its complex electronics running at a high clock speed, the system wasted quite a bit of electricity and generated a correspondingly large amount of heat. In spite of this, it was released with no fan and only a small heatsink. While this was perfectly adequate for brief use on an engineer's test bench in a well-ventilated office, it was less than satisfactory for extended play in real world conditions. This problem was exascerbated by Eiling Technologies' habitual use of industrial- and military-grade hardware, which would continue to operate even after reaching extreme temperatures.

In the fairly common "console on carpet" and "console in cabinet" situations, the console would frequently reach dangerous temperatures. Usually, this resulted in premature failure, as chips desoldered themselves or circuit boards cracked from thermal stress. Among the few customers, dozens suffered minor burns when touching their console&mdash;or simply touching a controller cable. There were even two cases of non-fatal house fires caused by overheating ARS units. Several lawsuits were filed, which only contributed to the bad press surrounding ET.

Most long-term users of the ARS modify their consoles, attaching large heatsinks to the circuit boards, and mounting a fan (or two) in the case. The ARS's ample power supply and roomy case actually make this modification fairly easy and unobtrusive.

## Game Selection<a name="gamesel">

At launch, the ARS came bundled with one of several games made in-house at Eiling Technologies. Unfortunately, most of ET's engineers were neither experienced nor particularly talented when it came to game design. While there were a few decent games, the majority of first party games were awful.

After several months of lackluster sales, ET offered development tools and expertise to any developer who asked, even resorting to cold calling to recruit developers. The few third-party games that were eventually released came far too late a date to save the console or the company.

ARS games are distributed in Game Folder format, which is similar to (and largely compatible with) byuu's Game Pak format.

The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED",  "MAY", and "OPTIONAL" in this document are to be interpreted as described in [RFC 2119](https://www.ietf.org/rfc/rfc2119.txt).

More information about the .ZIP file format can be found in PKWARE's [Application Note on the .ZIP file format](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT). This document is written against version 6.3.4 of that specification, which was released October 1, 2014.

More information about the byuuML markup language can be found in [my reverse engineered specification for the language](https://github.com/SolraBizna/byuuML/blob/master/FORMAT.md). That specification is "standard" in the sense that it is the specification understood ARS-emu.

# Game Folder Structure

A Game Folder is a directory containing a `manifest.bml` file. The directory name of a normal Game Folder SHOULD end with `.etars`. Generally, this Game Folder is packed within a .ZIP file for easy distribution. (See below.) The file name of a zipped Game Folder SHOULD end with `.etarz`. The file name of a Game Folder stored in any other type of archive should end with `.etars` (or `.etarz`) followed by the customary file extension of that archive format.

## Paths

A path within a Game Folder consists of zero or more directory names followed by a file name, separated by forward slashes. Paths MUST contain only safe characters (and slashes and periods). Paths MUST NOT be empty, and MUST NOT contain: a leading slash, a trailing slash, more than one consecutive slash, or any path component beginning with a period. Readers MAY impose any length and character restrictions on paths they like, except that they MUST accept any valid 8.3-compliant path with two or fewer directory components.

A path is 8.3-compliant if each directory name consists of 1-8 "safe" characters; and the file name consists of 1-8 "safe" characters, a period (.), and exactly 3 "safe" characters.

The following characters are considered "safe":

- Lowercase ASCII letters (a-z)
- ASCII digits (0-9)
- Underscores (_)

## Archives

A Game Folder MAY be stored inside an archive. When producing an archived Game Folder, the archive SHOULD NOT be a "tar bomb"; all contents SHOULD be at least one directory "deep" in the archive. In addition, such an archive MUST contain only one file named `manifest.bml`; an archive that contains more than one file with that name MUST NOT be considered a Game Folder (even though it is still an archive that *contains* Game Folders). Readers MUST be able to correctly process an archived Game Folder whose `manifest.bml` is at any valid depth within in the directory tree, including zero depth.

Game Folders are usually encapsulated into .ZIP files. Such Game Folders MUST have a "minimum version to extract" of 20 or lower for `manifest.bml` and all files referenced by it. Such .ZIP files SHOULD also have file names ending in `.etarz`, and a MIME type of `application/prs.bizna.etars+zip`.

Other types of Game Folder archive SHOULD have a file extension of `.etars` (or `.etarz`), followed by the customary file extension of the archive format in question. (e.g. `GAMENAME.etars.tar.gz` for gzipped tar archives.) The MIME type should be the normal one for archives of that type.

## `manifest.bml`

`manifest.bml` is a byuuML file provides all information necessary to semantically reconstruct a physical cartridge. This includes chip types and sizes, and information on how to map them. It MAY also contain human-readable metadata about a game. `manifest.bml` does not attempt to describe all theoretically-possible cartridge layouts, merely a sensible subset of them.

The `manifest.bml` format described here is deliberately compatible with that used by higan.

`manifest.bml` files MUST be encoded in UTF-8, but MAY use any line ending convention that is compatible with byuuML.

### `board`

`manifest.bml` MUST contain a root tag named `board`. All semantic information about the cartridge hardware MUST be contained within this tag. 

`board` tags without `id` children, or whose `id` children do not have a data value of `ETARS`, MUST be ignored. In addition, `board` tags are localizable. The first valid `board` tag found is the one that is used.

All *direct* children of the `board`, *except* `id`, are localizable. The following children are defined.

- `rom`
- `ram`
- `expansion`
- `mapper`

#### `rom`

At least one `rom` or `ram` tag is required. Every `rom` tag found applies.

There must be a `size` child, whose data is an integer giving the number of bytes addressable by this ROM chip. The value MUST be greater than or equal to 1, be less than or equal to 2^30, and be a power of two. (If you want to simulate a single-package multi-ROM where some sections are "shadowed", you MAY either repeat the shadow sections in the ROM image or carefully manipulate the `mapper` info.)

The value of the `id` child (if any) is the identifier for this chip. If it's not present, it is as if it were present with an empty value. The significance of this value, if any, is up to the mapper... but the namespace is always shared between active `rom` and `ram` tags, and only safe filename characters may appear in the value. There MUST NOT be more than one `rom` or `ram` tag with the same tag.

If a `name` child is present, it is the path to the initialization image for this ROM. The image is truncated or padded to fit into `size`. If no `name` child is present, there is no initialization image, and this ROM only contains padding bytes. If the initialization image cannot be opened, the reader MUST present an error.

If a `pad` child is present, its data is an integer giving the byte to pad initialization images (or lack thereof) with. If absent, 0 is used.

#### `ram`

At least one `rom` or `ram` tag is required. Every `ram` tag found applies.

`ram` tags are like `rom` tags, with the following differences:

- If the initialization image cannot be opened, the reader MUST act as though the file existed but was empty. (instead of throwing an error) 
- If no `pad` child is present, the initialization image (or lack thereof) MAY padded with random values, simulating uninitialized RAM.
- If a `volatile` child is present, the RAM is not persistent. If, however, there is no `volatile` child, the RAM *is* persistent. Persistent RAM MUST have a `name` child, and the `id` of persistent RAM SHOULD consist entirely of up to 8 safe characters.

If the initialization image file of a persistent `ram` tag can successfully be overwritten, with zero risk to the other files, the emulator MUST do so, and MUST NOT load initialization data from any other location. Otherwise, the emulator MUST attempt to save the data externally, adjacent to the Game Folder. If that fails, the emulator MAY also attempt to save the data externally in other well-defined locations.

To determine a filename for external storage:

- Start with the Game Folder name.
- If the name contains any occurrence of either `.etarz` or `.etars`, delete everything after it.
- If this `ram` tag has a non-empty `id`, append a period, then the `id`. Otherwise, append `.ram`.

#### `expansion`

`expansion` tags are optional. Every `expansion` tag found applies. Conflicts (i.e. different expansions that map to the same address) SHOULD not exist in the manifest.

The value of the `expansion` tag is the type of expansion hardware. Each `expansion` tag adds a known type of expansion hardware to the cartridge. If an emulator does not understand a given type of expansion hardware *at least* well enough to "stub it out", it MUST throw an error.

Valid values:

- `ham`: The Homebrew Achievements Module, default address `0x245`.
- `config`: The emulator configuration port, default address `0x246`.
- `debug`: The serial port from the developer cartridge, default address `0x247`.

There MAY be an `addr` tag; if there is, its value is the address to map the expansion hardware to, which MUST be between `0x242` and `0x247`. If the `addr` tag is not present, the expansion hardware MUST be mapped to the default address, which depends on the type of expansion hardware.

While no current expansion hardware does this, at a later date, expansion hardware MAY depend on additional children of the `expansion` tag.

#### `mapper`

The first `mapper` tag found applies.

If more than one `rom`/`ram` tag exists, `mapper` is required. Otherwise, if no `mapper` is found, it is as if a single empty `mapper` was found.

The value of the `mapper` tag is the type of mapper.

##### Empty `mapper`

This is the default if only one `rom`/`ram` tag exists and no `mapper` is specified. Straightforwardly connects A0-A14 to corresponding pins on the memory chip, and B0-B7 to A15+ on the memory chip (if present), and A15 to Chip Select. Power On Bank is 0. BSx=0.

##### `mapper=devcart`

Uses the two most significant bits of the bank number to select which chip is active, like the developer cart (and every "official" cartridge). Children:

- `0`/`1`/`2`/`3`: Required. Value is the `id` of the chip to select, or the exact value `open` for open bus. `0`/`1`/`2`/`3` respectively act on banks `$00-$3F`/`$40-$7F`/`$80-$BF`/`$C0-$FF`.
    - `unshift`: Optional. This value affects the way addresses are calculated for this quarter of the bank space. Non-zero values mean not every bank of cartridge space can be mapped to every area of this chip, allowing a theoretically larger memory space. MUST not be greater than `bs`. Default is 0. 
- `bs`: Optional. Value is an integer 0, 1, 2, or 3, giving the value of the BSx pins; BSx=0, 1, 2, 3 gives 32KiB, 16KiB, 8KiB, 4KiB banks respectively. Default is 0.
- `power-on-bank`: Optional. Value is written to the bank select registers on reset. Default is 0.
- `overlay-bank`: Optional. Value is the `id` of the chip to select during an overlay tile fetch. If a `bank` child is present, its value is the `bank` to use during overlay fetches. (If absent, banks are calculated as normal.)

The effective address of a fetch is `(B << (23-(bs-unshift))) | (A & (0x7FFF>>(bs-unshift)))`.

#### `input`

`input` tags are optional, and denote recommended input configuration for playing this game. A `port` child denotes which controller port this `input` tag applies to, `1` or `2`. Presence of the following children indicates support of the given device type in the given port:

- `none`: This port may be unconnected.
- `controller`: Standard controller, with 3+1 buttons and a hat switch.
- `light-gun`: Light gun.
- `keyboard`: Keyboard.
- `light-pen`: Light pen.

Example: `input port=1 controller keyboard` indicates that the game is designed to be played with either a standard controller or a keyboard plugged into controller port 1

If no `input port=1` is provided, `input port=1 controller` MAY be assumed. If no `input port=2` is provided, `input port=2 none` MAY be assumed. Emulators and readers MAY entirely ignore `input` tags.

### `information`

`manifest.bml` MAY contain a root tag named `information`, containing human-readable meta-information about the cartridge. If present, the `information` tag SHOULD have, at minimum, a child `title` whose data is a sensible title for the cartridge.

All *direct* children of the `information` are localizable. The tag names SHOULD be in English, even when their data are not.

### `languages`

If localization is used, `manifest.bml` SHOULD contain a root tag named `languages`, containing meta-information about each language code used by this `manifest.bml`. `languages` contains child `lang` tags, each of which MUST have a value equal to the IETF language code, and one or more `name` children giving human-recognizable names. The `name` tag is localizable.

If a `default` child is present, this language is the one that is used "by default" in this manifest (wherever no other language matches). If not present, the first `lang` tag MAY be considered the default. There SHOULD NOT be multiple `lang` tags with a `default` child. This is advisory only, and MUST NOT affect parsing of localized tags!

If a `quality` child is present, its data should be taken as an integer percentage from 0 to 100 representing the subjective quality of the translation. 0 is unintelligible, while 100 is perfectly fluent. (This applies to the localization of cartridge elements, not metadata.) The quality of the default translation MAY be assumed to be flawless if not specified.

If a `coverage` child is present, its data should be taken as an integer percentage from 0 to 100 representing the approximate "completeness" of the translation. (This also applies to the localization of cartridge elements, not metadata.) The coverage of the default translation MAY be assumed to be perfect.

Example:

    languages
        lang=en name="English" default
        lang=en-US name="English (American)" quality=100 coverage=100
        lang=en-GB name="English (British)" quality=100 coverage=100
        lang=de name="German" quality=5 coverage=50
            name="Deutsch" lang=de
        lang=ja name="Japanese" quality=10 coverage=95
            name="日本語" lang=ja

Each `lang` child SHOULD have localized names for, at minimum:

- English, which SHOULD also be the default
- The game's default language, if its name for the language is different from the English one
- The language itself, if its name for itself is different from the English one

So, for example, in a natively Arabic game with a Swahili localization, the `sw` language code should have a default `name=Swahili` (covering both English and Swahili) and `name="سواحلية" lang=ar` (covering Arabic).

Another example is an English game with an Esperanto localization. Because the English and Esperanto names for Esperanto are the same, only a default name need be given.

The idea is that the following categories of people should be easily able to identify a given localization:

- Speakers of the game's default language
- Speakers of the particular language in question
- Anyone comfortable enough with Latin script to look up a language based on its English name
- Me, because I am both selfish and fluent in English

### Localization

Some tags are "localizable". The rules here control which tags are effective depending on local language choices. The following is a complete list of localizable tags:

- `board`
- `board` -> (any direct child except `id`)
- `information` -> (any direct child)
- `languages` -> `lang` -> `name`

If a reader does not wish to adopt the complexity of local language choices, it MUST ignore any "localizable" tag which *has* lang children, if (and only if) none of those children have a value of `default` or `*`.

Emulators SHOULD allow some means for the user to override their system language and play a game in any of its localized configurations. In such a situation, they SHOULD confine the effects of such an override to the `board` tag and its children.

A localizable tag may have one or more `lang` children, each of which is one language code that applies to this tag. `lang=default` means that this tag should be used if no sibling tag of the same name had a matching `lang`. `lang=*` means that this tag should be used no matter what language is in use. If a localizable tag has no `lang` children, it MUST be treated as if it had a single `lang=default` child.

For tags where only the first one encountered will be used, the default tag (`lang=default` or no `lang`) SHOULD be first.

When searching for any of the above tags, use the following algorithm:

    for the IETF code of each acceptable local language:

        for each properly-named tag:
            if the tag has a lang child which matches the IETF code
            and the value of the matching lang child is longer than the longest match:
               the longest match := value of matching lang child
    
        if the longest match is set:
            return every properly-named tag
              EITHER with a lang=* child
              OR with a lang child whose value is exactly equal to the longest match
    
    return every properly-named tag
      EITHER with a lang=* child
      OR with a lang=default child
      OR with no lang child

(Note: Whether some children of the `language` tag are marked as `default` MUST NOT affect this algorithm. An example where this matters is given in the examples section.)

Tag order within the document MUST be preserved, so it's not enough to build lists and then concatentate them. This may seem complicated, but it is simpler than it appears. Before processing a localizeable tag, first find a Longest Match among the instances of that tag. Then, when actually processing, skip tags according to the values of their lang children.

A `lang` tag's value matches a local language code if the local language code is EXACTLY equal to the value, OR if the local language code starts with EXACTLY the value followed by a hyphen. Matches are CASE SENSITIVE. Examples:

| `lang`  | language | matches                          |
| ------- | -------- | -------------------------------- |
| `en`    | `en-GB`  | yes (language starts with `en-`) |
| `en-G`  | `en-GB`  | NO                               |
| `en-GB` | `en-GB`  | yes (exact match)                |
| `en-gb` | `en-GB`  | NO                               |

Note that any `lang` children of tags other than the one being searched for do not affect this algorithm at all. Consider:

    board id:ETARS
        rom id=rom1 name=main.rom
        rom id=rom2 name=us.rom lang=default
        rom id=rom2 name=eu.rom lang=fr lang=de lang=it lang=es lang=pt lang=en-GB
        rom id=rom2 name=ja.rom lang=ja
        rom id=rom2 name=eo.rom lang=eo
        expansion=debug lang=zh
    // `languages` tag omitted from this example

In this rather contrived example, if the local language is Chinese, the debug port is present, otherwise it is absent. But the search for `mem` tags is not affected by this. Chinese users will still get `us.rom` for their ROM2 chip.

## Examples

### SimpleConfig

- `config.rom`, 2048 bytes

Contents of `manifest.bml`:

    board id:ETARS
      rom name=config.rom size=2048
      expansion=config

This manifest describes a "cartridge" containing a single, 2048-byte, straightforwardly-mapped ROM chip, and the (utterly unreal) emulator configuration port. Under normal circumstances, SimpleConfig only receives already-localized text from the emulator via the configuration port, so localizing SimpleConfig is not necessary.

### Stardust

- `stardust.rom`, 32768 bytes
- `stard_de.rom`, 32768 bytes
- `stard_eo.rom`, 32768 bytes
- `stard_ja.rom`, 32768 bytes

Contents of `manifest.bml`:

    board id:ETARS
      rom name=stardust.rom size=0x8000
      rom name=stard_de.rom size=0x8000 lang=de
      rom name=stard_eo.rom size=0x8000 lang=eo
      rom name=stard_ja.rom size=0x8000 lang=js
    
    information
      title="Stardust"
      title="Sternenstaub" lang=de
      title="Stelpolvigejo" lang=eo
      title="星屑を作る兄弟" lang=ja
    
    languages
      lang=en name="English" default
      lang=de name="German"
        name="Germana" lang=eo
        name="Deutsch" lang=de
      lang=eo name="Esperanto"
      lang=ja name="Japanese"
        name="Japana" lang=eo
        name="日本語" lang=ja

This manifest describes a hypothetical ARS port of Stardust, a puzzle game by James Burton. The cartridge contains a single, straightforwardly-mapped ROM chip, with no other hardware or logic. One of four different versions of this ROM will be used, depending on the user's language preferences; the English one is used if none of the alternatives are preferred. It also provides localized titles for each of those languages.

(Whoever made this manifest really liked Esperanto; normally only the native and English names of the languages would be provided.)

### Nu, Pogodi!

- `nupogodi.rom`, 131072 bytes
- `lang_ru.rom`, 14031 bytes
- `lang_en.rom`, 16112 bytes
- `lang_zh.rom`, 57488 bytes

Contents of `manifest.bml`:

    board id:ETARS
      mapper:devcart
        bs=1
        0=rom1
        1=rom2
        2=sram
        3=sram
      rom id=rom1 name=nupogodi.rom size=0x20000 lang=*
      rom id=rom2 name=lang_ru.rom size=0x4000
      rom id=rom2 name=lang_en.rom size=0x4000 lang=en
      rom id=rom2 name=lang_zh.rom size=0x10000 lang=zh
      ram id=sram name=sram.ram size=256
      input port=1 controller
      input port=2 controller none
    
    information
      title="Ну, погоди!"
      title="Nu, Pogodi!" lang=en
      title="兔子，等着瞧！" lang=zh
    
    languages
      lang=ru name="Russian" default
        name="Русский" lang=ru
      lang=en name="English"
        name="Английский" lang=ru
      lang=zh name="Chinese"
        name="Китайский" lang=ru
        name="中文" lang=zh

This manifest describes a hypothetical ["Ну, погоди!"](https://en.wikipedia.org/wiki/Nu,_pogodi!) ARS game. It has locale-independent data in one ROM chip, and locale-specific data in another, which is what most reasonably complex, localized ARS software should probably do. The "original" version is Russian, so Russian is the default language... note, however, that the default *language names* are still in English.

This cartridge uses the standard mapper. Banks `$00-3F` come from `nupogodi.rom`, banks `$40-7F` come from one of the `lang_*.rom` files, banks `$80-FF` come from `sram.ram`. `bs=1` means banks are 16KiB in size.

`nupogodi.rom` is an ordinary cartridge image, and is used whatever language is selected. Which `lang_*.rom` file is used depends on the user's language preferences. Each of the `lang_*.rom` files is shorter than the `rom` tag indicates; they are padded with zeroes to fit. `lang_zh.rom` is longer than the rest (probably because it includes a Chinese font). 

Note that the `ram` tag does not have a `lang` tag. Even though `ram` and `rom` both describe memory devices, because the name of the tag is different, this `ram` tag is not affected by the `lang` tags of its `rom` siblings, or vice versa.

`sram.ram` doesn't exist in the Game Folder, so on first startup the game's SRAM will either be all zeroes or random garbage. Writes the game makes to SRAM will be saved, and will be found in the SRAM on future boots.

The game is intended to be played with a controller connected to port 1, and optionally another one connected to port 2.

; -*- fundamental -*-

; This file is designed to fill 2KiB. This allows SimpleConfig to be embedded
; in another ROM, even one that includes the ET license screen block.

.MEMORYMAP
DEFAULTSLOT 0
SLOT 0 $F000 $0800
SLOT 1 $00EE $0012 ; the part of zero page we can use
SLOT 2 $0256 $79AA ; WRAM
.ENDME
.ROMBANKSIZE $0800
.ROMBANKS 1
.DEFINE ZeroPageSlot 1
.DEFINE WRAMSlot 2

.MACRO WAI
.DB $CB
.ENDM
.MACRO STP
.DB $DB
.ENDM

; PPU
.ENUM $0200
r_BGScrollX DB
r_BGScrollY DB
r_Multi DB
r_BGTileBases DW
r_SPPalettes DB
r_BGPalettes DS 4
r_BGFG DS 4
r_OLPalette DB
r_ColorMod DB
r_VRAMPage DB
r_VRAMPort DB
r_CRAMIndex DB
r_CRAMPort DB
r_SSMIndex DB
r_SSMPort DB
r_SAMIndex DB
r_SAMPort DB
r_VRAMIndex DB
r_IRQScanline DB
r_VRAMDMA DB
r_VRAMSplatDMA DB
r_CRAMDMA DB
r_SSMDMA DB
r_SAMDMA DB
r_SSMUnpackedDMA DB
.ENDE

; APU
.ENUM $0220
r_VoiceRateLo DS 8
r_VoiceRateHi DS 7
r_NoisePeriod DB
r_VoiceWaveform DS 7
r_NoiseWaveform DB
r_VoiceVolume DS 7
r_NoiseVolume DB
.ENDE

.DEFINE r_Controller1 $0240
.DEFINE r_Controller2 $0241
.DEFINE r_HAM $0245
.DEFINE r_EmuConfig $0246
.DEFINE r_DebugPort $0247

; Overlay tilemap and attribute map
.DEFINE r_OLTiles $7C00
.DEFINE r_OLAttributes $7C00+(32*28)

.DEFINE g_NMICount $00

.ORG 0

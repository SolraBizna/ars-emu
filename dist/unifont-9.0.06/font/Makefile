#
# Makefile to build a version of GNU Unifont with the
# unifoundry.com GNU Unifont utilities.  This Makefile
# converts unifont-$(VERSION).hex and related files into a final
# GNU Unifont font file.
#
# This software is released under version 2.0 of the GNU Public License,
# or (at your option) a later version of the GPL.
#
# Paul Hardy, 7 July 2008
#
# commands will be interpreted with the Bourne Shell
#
SHELL = /bin/sh
INSTALL = install
GZFLAGS = -f -9 -n
FONTFORGE = fontforge
#
# assembly date of this version
#
UNICODE_VERSION = 9.0
PKG_REV = 06
VERSION = $(UNICODE_VERSION).$(PKG_REV)

COPYRIGHT = "Copyright (C) 2016 Roman Czyborra, Paul Hardy, Qianqian Fang, \
Andrew Miller, et al.  \
Licensed under the GNU General Public License; either version 2, or \
(at your option) a later version, with the GNU Font Embedding Exception."

#
# Path to local unifont-bin utilities.  If your system doesn't
# use GNU's version of "make", use the first BINDIR definition.
# If using GNU's "make", use the second BINDIR definition.
#
# BINDIR = ../bin
BINDIR = $(CURDIR)/../bin
#
# The top-level directory for installing fonts on the system,
# and the installation directories for PCF and TrueType fonts.
#
CONSOLEDEST = $(DESTDIR)/usr/share/consolefonts
FONTDEST    = $(DESTDIR)/usr/share/fonts
PCFDEST     = $(FONTDEST)/X11/misc
TTFDEST     = $(FONTDEST)/truetype/unifont
#
# destination directory for compiled fonts
#
COMPILED_DIR = compiled
#
# destination directory for .bmp or .png representation of font
#
BMPDIR = $(COMPILED_DIR)/bmp
PNGDIR = $(COMPILED_DIR)/png

# Choose one graphics format or neither.
GRAPHICS=png
# GRAPHICS=bmp
#
# Directory with original unifont-$(VERSION).hex files
#
HEXDIR = plane00
#
# These are the files for building GNU Unifont with the Qianqian Fang's
# Wen Quan Yi CJK ideographs.  This version provides complete coverage
# of the Unicode Basic Multilingual Plane.
#
# If you want don't want to inlcude blank glyphs in unassigned code points,
# uncomment the UNASSIGNED definition or override from the make command line.
#
# Likewise, uncomment the PUA definiation if you want to use Private
# Use Area glyphs.
#
UNASSIGNED = $(HEXDIR)/unassigned.hex
# UNASSIGNED =

#
# Non-printing glyphs.  There are only about 100 of these, and many end
# users do want to print representations of these glyphs, so they are
# included as an optional assignment.
#
NONPRINTING = $(HEXDIR)/nonprinting.hex
# NONPRINTING =

#
# Private Use Area glyphs.  Uncomment to include four-digit hexadecimal glyphs
# or override from the make command line.
#
# PUA = $(HEXDIR)/pua.hex
PUA = 

# The remaining .hex files will be constant unless a customized font
# is being built.
UNIFONTBASE = $(HEXDIR)/unifont-base.hex
CJK         = $(HEXDIR)/wqy.hex
HANGUL      = $(HEXDIR)/hangul-syllables.hex
SPACES      = $(HEXDIR)/spaces.hex

UNIFILES = $(UNIFONTBASE) $(CJK) $(HANGUL) $(SPACES) $(UNASSIGNED) \
	   $(NONPRINTING) $(PUA)

#
# Planes 1 through 14 (0x0E) are ordinary;
# Planes 15 (0x0F) and 16 (0x10) are Private Use Area
#
UPPER_FILES = plane0[1-E]/*.hex

#
# Location of the file containing a list of Unicode combining characters.
#
COMBINING = plane00/bmp-combining.txt

UPPER_COMBINING = plane0[0-E]/*combining*.txt

VPATH = plane00 ttfsrc

#
# Location of  TTF source directory, where TTF font is built.
#
TTFSRC = ttfsrc


all: compiled

compiled: $(UNIFILES) $(COMBINING) $(UPPER_FILES) $(UPPER_COMBINING)
	$(MAKE) compiled-files

compiled-files: pcf psf $(GRAPHICS) ttf csurttf upperttf bigpic coverage
	install -p index.html $(COMPILED_DIR)/index.html

#
# Build the aggregate .hex font files
#
hex:
	if [ ! -d $(COMPILED_DIR) ] ; then \
	   mkdir -p $(COMPILED_DIR) ; \
	fi
	sort $(UNIFILES) > $(COMPILED_DIR)/unifont-$(VERSION).hex
	(cd $(HEXDIR) && sort *.hex) | \
	  egrep -v "^FFF[EF]" | \
	   $(BINDIR)/unigencircles $(COMBINING) plane00/nonprinting.hex \
	   > $(COMPILED_DIR)/unifont_sample-$(VERSION).hex
	sort $(UPPER_FILES) \
	   > $(COMPILED_DIR)/unifont_upper-$(VERSION).hex
	sort $(UPPER_COMBINING) > $(COMPILED_DIR)/upper_combining.txt
	sort plane0[1-F]/plane*-nonprinting.hex \
	   > $(COMPILED_DIR)/upper_nonprinting.hex
	sort plane0[1-F]/*.hex | egrep -v "^..FFF[EF]" | \
	   $(BINDIR)/unigencircles $(COMPILED_DIR)/upper_combining.txt \
	   $(COMPILED_DIR)/upper_nonprinting.hex \
	   > $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).hex
	# Create a .hex file with CSUR glyphs, without combining circles
	sort $(COMPILED_DIR)/unifont-$(VERSION).hex plane00csur/*.hex \
	   > $(COMPILED_DIR)/unifont_csur_lower-$(VERSION).hex
	# Create a .hex file with only CSUR glyphs from all planes
	sort plane00csur/*.hex | sed -e 's/^/00/' \
	   > $(COMPILED_DIR)/unifont_csur-$(VERSION).hex
	sort plane0[1-F]csur/*.hex \
	   >> $(COMPILED_DIR)/unifont_csur-$(VERSION).hex
	# Create a .hex file with all CSUR glyphs, with combining circles
	sort plane00csur/plane00csur-combining.txt \
	   >  $(COMPILED_DIR)/csur_combining.txt
	sort plane0[1-F]csur/plane0[1-F]csur-combining.txt \
	   >> $(COMPILED_DIR)/csur_combining.txt
	$(BINDIR)/unigencircles \
	     $(COMPILED_DIR)/csur_combining.txt \
	     plane00/nonprinting.hex \
	   < $(COMPILED_DIR)/unifont_csur-$(VERSION).hex \
	   > $(COMPILED_DIR)/unifont_csur_sample-$(VERSION).hex

#
# Build a BDF font file from the final .hex file.
#
bdf: hex
	# First make the default BDF font.  The font name will be "unifont".
	$(BINDIR)/hex2bdf --version "$(VERSION)" --copyright $(COPYRIGHT) \
	   $(COMPILED_DIR)/unifont-$(VERSION).hex \
	   >$(COMPILED_DIR)/unifont-$(VERSION).bdf
	gzip $(GZFLAGS) <$(COMPILED_DIR)/unifont-$(VERSION).bdf \
	           >$(COMPILED_DIR)/unifont-$(VERSION).bdf.gz
	# Now make a version with combining circles.  The font name
	# will be "unifont_sample" instead of "unifont" to distinguish
	# it from the default font.
	$(BINDIR)/hex2bdf --font "Unifont Sample" \
	   --version "$(VERSION)" --copyright $(COPYRIGHT) \
	     $(COMPILED_DIR)/unifont_sample-$(VERSION).hex \
	   > $(COMPILED_DIR)/unifont_sample-$(VERSION).bdf
	gzip $(GZFLAGS) <$(COMPILED_DIR)/unifont_sample-$(VERSION).bdf \
	           >$(COMPILED_DIR)/unifont_sample-$(VERSION).bdf.gz
	# Font with ConScript Unicode Registry (CSUR) glyphs added
	$(BINDIR)/hex2bdf --font "Unifont CSUR" \
	   --version "$(VERSION)" --copyright $(COPYRIGHT) \
	     $(COMPILED_DIR)/unifont_csur_lower-$(VERSION).hex \
	   > $(COMPILED_DIR)/unifont_csur-$(VERSION).bdf
	gzip $(GZFLAGS) <$(COMPILED_DIR)/unifont_csur-$(VERSION).bdf \
	           >$(COMPILED_DIR)/unifont_csur-$(VERSION).bdf.gz
	# Font with glyphs above Plane 0
	$(BINDIR)/hex2bdf --font "Unifont Upper" \
	   --version "$(VERSION)" --copyright $(COPYRIGHT) \
	     $(COMPILED_DIR)/unifont_upper-$(VERSION).hex \
	   > $(COMPILED_DIR)/unifont_upper-$(VERSION).bdf
	gzip $(GZFLAGS) <$(COMPILED_DIR)/unifont_upper-$(VERSION).bdf \
	           >$(COMPILED_DIR)/unifont_upper-$(VERSION).bdf.gz
	# Font with glyphs above Plane 0 with combining circles added
	$(BINDIR)/hex2bdf --font "Unifont Upper Sample" \
	   --version "$(VERSION)" --copyright $(COPYRIGHT) \
	     $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).hex \
	   > $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).bdf
	gzip $(GZFLAGS) <$(COMPILED_DIR)/unifont_upper_sample-$(VERSION).bdf \
	           >$(COMPILED_DIR)/unifont_upper_sample-$(VERSION).bdf.gz

#
# Build a PCF font file from the final .hex file.
#
pcf: bdf
	bdftopcf <$(COMPILED_DIR)/unifont-$(VERSION).bdf \
	         >$(COMPILED_DIR)/unifont-$(VERSION).pcf
	gzip $(GZFLAGS) $(COMPILED_DIR)/unifont-$(VERSION).pcf
	bdftopcf <$(COMPILED_DIR)/unifont_sample-$(VERSION).bdf \
	         >$(COMPILED_DIR)/unifont_sample-$(VERSION).pcf
	gzip $(GZFLAGS) $(COMPILED_DIR)/unifont_sample-$(VERSION).pcf
	bdftopcf <$(COMPILED_DIR)/unifont_csur-$(VERSION).bdf \
	         >$(COMPILED_DIR)/unifont_csur-$(VERSION).pcf
	gzip $(GZFLAGS) $(COMPILED_DIR)/unifont_csur-$(VERSION).pcf

#
# Make the PSF (console) font for APL (A Programming Language).
#
psf: bdf
	bdf2psf --fb \
	   $(COMPILED_DIR)/unifont-$(VERSION).bdf \
	   psf/apl-equivalents.txt \
	   psf/unifont-apl.txt \
	   512 \
	   $(COMPILED_DIR)/Unifont-APL8x16-$(VERSION).psf
	gzip $(GZFLAGS) $(COMPILED_DIR)/Unifont-APL8x16-$(VERSION).psf

#
# Print coverage of scripts in Basic Multilingual Plane in .txt file.
#
coverage:
	sort $(HEXDIR)/*.hex | \
		$(BINDIR)/unicoverage > $(COMPILED_DIR)/coverage.txt

#
# Print HTML page coverage in Basic Multilingual Plane in .txt file.
#
pagecount: $(COMPILED_DIR)/unifont-$(VERSION).hex $(BINDIR)/unipagecount
	$(BINDIR)/unipagecount -l < $(COMPILED_DIR)/unifont-$(VERSION).hex \
		        > $(COMPILED_DIR)/pagecount.html
	if [ x$(GRAPHICS) = xbmp ] ; then \
	   sed -i 's/png/bmp/g' $(COMPILED_DIR)/plane00.html ; \
	fi

#
# Create the .png (Portable Network Graphics) versions of the glyphs.
#
png: hex $(BINDIR)/unihex2png
	if [ ! -d $(PNGDIR) ] ; then \
	   mkdir -p $(PNGDIR) ; \
	fi
	if [ ! -d $(PNGDIR)/plane00 ] ; then \
	   mkdir -p $(PNGDIR)/plane00 ; \
	fi
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2png -p 00$$i$$j \
	         -i $(COMPILED_DIR)/unifont_sample-$(VERSION).hex \
	         -o $(PNGDIR)/plane00/uni00$$i$$j.png ; \
	   done ; \
	done
	if [ ! -d $(PNGDIR)/plane01 ] ; then \
	   mkdir -p $(PNGDIR)/plane01 ; \
	fi
	# Generate files in plane01/all directory to create PNG files.
	(cd plane01 && make)
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2png -p 01$$i$$j \
	         -i plane01/all/plane01-all.hex \
	         -o $(PNGDIR)/plane01/uni01$$i$$j.png ; \
	   done ; \
	done
	if [ ! -d $(PNGDIR)/plane0E ] ; then \
	   mkdir -p $(PNGDIR)/plane0E ; \
	fi
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2png -p 0E$$i$$j \
	         -i $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).hex \
	         -o $(PNGDIR)/plane0E/uni0E$$i$$j.png ; \
	   done ; \
	done
	if [ ! -d $(PNGDIR)/csur ] ; then \
	   mkdir -p $(PNGDIR)/csur ; \
	fi
	for i in E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF \
	         F0 F1 F2 F3 F4 F5 F6 F7 F8 ; do \
	      $(BINDIR)/unihex2png -p 00$$i \
	         -i $(COMPILED_DIR)/unifont_csur_sample-$(VERSION).hex \
	         -o $(PNGDIR)/csur/uni00$$i.png ; \
	done
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2png -p 0F$$i$$j \
	         -i $(COMPILED_DIR)/unifont_csur_sample-$(VERSION).hex \
	         -o $(PNGDIR)/csur/uni0F$$i$$j.png ; \
	   done ; \
	done
	# Create HTML table showing color-coded percent coverage in Plane 0.
	$(BINDIR)/unipagecount -h < $(COMPILED_DIR)/unifont-$(VERSION).hex \
	                          > $(COMPILED_DIR)/plane00.html
	# Create HTML table showing color-coded percent coverage in Plane 1.
	../bin/unipagecount -P1 -h < plane01/all/plane01-all.hex \
	   > compiled/plane01.html
	echo "Done creating $(PNGDIR)"

#
# Create the .bmp (Bitmap Graphics) versions of the glyphs.
#
bmp: hex $(BINDIR)/unihex2bmp
	if [ ! -d $(BMPDIR) ] ; then \
	   mkdir -p $(BMPDIR) ; \
	fi
	if [ ! -d $(BMPDIR)/plane00 ] ; then \
	   mkdir -p $(BMPDIR)/plane00 ; \
	fi
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2bmp -p00$$i$$j \
	         < $(COMPILED_DIR)/unifont_sample-$(VERSION).hex \
	         > $(BMPDIR)/plane00/uni00$$i$$j.bmp ; \
	   done ; \
	done
	if [ ! -d $(BMPDIR)/plane01 ] ; then \
	   mkdir -p $(BMPDIR)/plane01 ; \
	fi
	# Generate files in plane01/all directory to create PNG files.
	(cd plane01 && make)
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2bmp -p01$$i$$j \
	         < plane01/all/plane01-all.hex \
	         > $(BMPDIR)/plane01/uni01$$i$$j.bmp ; \
	   done ; \
	done
	if [ ! -d $(BMPDIR)/plane0E ] ; then \
	   mkdir -p $(BMPDIR)/plane0E ; \
	fi
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2bmp -p0E$$i$$j \
	         < $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).hex \
	         > $(BMPDIR)/plane0E/uni0E$$i$$j.bmp ; \
	   done ; \
	done
	if [ ! -d $(BMPDIR)/csur ] ; then \
	   mkdir -p $(BMPDIR)/csur ; \
	fi
	for i in E0 E1 E2 E3 E4 E5 E6 E7 E8 E9 EA EB EC ED EE EF \
	         F0 F1 F2 F3 F4 F5 F6 F7 F8 ; do \
	      $(BINDIR)/unihex2bmp -p00$$i \
	         < $(COMPILED_DIR)/unifont_csur_sample-$(VERSION).hex \
	         > $(BMPDIR)/csur/uni00$$i.bmp ; \
	done
	for i in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	   for j in 0 1 2 3 4 5 6 7 8 9 A B C D E F; do \
	      $(BINDIR)/unihex2bmp -p0F$$i$$j \
	         < $(COMPILED_DIR)/unifont_csur_sample-$(VERSION).hex \
	         > $(BMPDIR)/csur/uni0F$$i$$j.bmp ; \
	   done ; \
	done
	# Create HTML table showing color-coded percent coverage in Plane 0.
	$(BINDIR)/unipagecount -h < $(COMPILED_DIR)/unifont-$(VERSION).hex \
	                          > $(COMPILED_DIR)/plane00.html
	sed -i 's/png/bmp/g' $(COMPILED_DIR)/plane00.html ; \
	# Create HTML table showing color-coded percent coverage in Plane 01.
	../bin/unipagecount -P1 -h < plane01/all/plane01-all.hex \
	   > $(COMPILED_DIR)/plane01.html
	sed -i 's/png/bmp/g' $(COMPILED_DIR)/plane01.html ; \
	echo "Done creating $(BMPDIR)"

#
# Build one bitmap of the entire Unifont Plane 0 set of glyphs.
# Use all the .hex files in $(HEXDIR) so we add the contents of
# omit.hex (FFFE & FFFF).  Those two glyphs aren't included in the
# unifont_sample font because they cause problems with MS Windows,
# but we can put them in the picture.  Also add the Plane 0
# ConScript Unicode Registry (CSUR) glyphs in the Private Use Area.
# The CSUR glyphs are optional, appearing in the unifont_csur font
# files rather than the main unifont font files.
#
bigpic: $(COMPILED_DIR)/unifont_sample-$(VERSION).hex
	cat $(HEXDIR)/*.hex plane00csur/*.hex | \
	   $(BINDIR)/unifontpic -d120 > $(COMPILED_DIR)/unifont-$(VERSION).bmp

#
# Note that $(TTFSRC) must exist, because it contains some source files
# Perform a "make && make clean" because ALL of the files would consume
# over 200 Megabytes if left around.  The .sfd file is about 100 Megabytes,
# and it is created from merging "[0-F].sfd", which take up another 100 MB.
#

ttf:
	# First, copy the ordinary version, to make a TrueType font.
	install -p \
		$(COMPILED_DIR)/unifont-$(VERSION).hex \
		$(TTFSRC)/unifont.hex
	install -p \
		$(HEXDIR)/bmp-combining.txt \
		$(TTFSRC)/combining.txt
	cd $(TTFSRC) && \
	   $(MAKE) FONTFILE="unifont" COMBINING="combining" \
		FONTNAME="Unifont" PSNAME="Unifont"
	mv $(TTFSRC)/unifont.sfd $(COMPILED_DIR)/unifont-$(VERSION).sfd 
	mv $(TTFSRC)/unifont.ttf \
		$(COMPILED_DIR)/unifont-$(VERSION).ttf 
	# Second, copy unifont_sample.bdf, to make an SBIT font.
	install -p \
		$(COMPILED_DIR)/unifont_sample-$(VERSION).bdf \
		$(TTFSRC)/unifont_sample.bdf
	cd $(TTFSRC) && \
	   $(MAKE) sbit FONTFILE="unifont_sample" COMBINING="" \
		FONTNAME="Unifont Sample" PSNAME="UnifontSample"
	mv $(TTFSRC)/unifont_sample.ttf \
		$(COMPILED_DIR)/unifont_sample-$(VERSION).ttf 

#
# Now build the ConScript Unicode Registry PUA font.
#
csurttf:
	install -p \
		$(COMPILED_DIR)/unifont_csur-$(VERSION).hex \
		$(TTFSRC)/unifont_csur.hex
	sort -u plane00csur/plane00csur-combining.txt \
		>  $(TTFSRC)/combining_csur.txt
	sort -u plane0[1-F]csur/plane0[1-F]csur-combining.txt \
		>> $(TTFSRC)/combining_csur.txt
	cd $(TTFSRC) && \
	   $(MAKE) outline FONTFILE="unifont_csur" COMBINING="combining_csur" \
		FONTNAME="Unifont CSUR" PSNAME="UnifontCSUR"
	mv $(TTFSRC)/unifont_csur.sfd \
		$(COMPILED_DIR)/unifont_csur-$(VERSION).sfd 
	mv $(TTFSRC)/unifont_csur.ttf \
		$(COMPILED_DIR)/unifont_csur-$(VERSION).ttf 
	gzip $(GZFLAGS) $(COMPILED_DIR)/unifont_csur-$(VERSION).sfd 

#
# Now build the ConScript Unicode Registry PUA font.
#
upperttf:
	# First, copy the ordinary version, to make a TrueType font.
	install -p \
		$(COMPILED_DIR)/unifont_upper-$(VERSION).hex \
		$(TTFSRC)/unifont_upper.hex
	sort -u $(UPPER_COMBINING) > $(TTFSRC)/combining_upper.txt
	cd $(TTFSRC) && \
	   $(MAKE) outline FONTFILE="unifont_upper" COMBINING="combining_upper" \
		FONTNAME="Unifont Upper" PSNAME="UnifontUpper"
	mv $(TTFSRC)/unifont_upper.sfd \
		$(COMPILED_DIR)/unifont_upper-$(VERSION).sfd 
	mv $(TTFSRC)/unifont_upper.ttf \
		$(COMPILED_DIR)/unifont_upper-$(VERSION).ttf 
	gzip $(GZFLAGS) $(COMPILED_DIR)/unifont_upper-$(VERSION).sfd 
	# Second, copy unifont_upper_sample.bdf, to make an SBIT font.
	install -p \
		$(COMPILED_DIR)/unifont_upper_sample-$(VERSION).bdf \
		$(TTFSRC)/unifont_upper_sample.bdf
	cd $(TTFSRC) && \
	   $(MAKE) sbit FONTFILE="unifont_upper_sample" COMBINING="" \
		FONTNAME="Unifont Upper Sample" PSNAME="UnifontUpperSample"
	mv $(TTFSRC)/unifont_upper_sample.ttf \
		$(COMPILED_DIR)/unifont_upper_sample-$(VERSION).ttf 


#
# Copy the newly created files from $(COMPILED_DIR) to the precompiled/
# directory.  This has to be called manually, because the precompiled/
# directory usually remains untouched.
#
precompiled: all
	\rm -rf precompiled
	install -m0755 -d precompiled
	install -m0644 -p $(COMPILED_DIR)/unifont-$(VERSION).hex \
	                  $(COMPILED_DIR)/unifont-$(VERSION).bdf.gz \
	                  $(COMPILED_DIR)/unifont-$(VERSION).pcf.gz \
	                  $(COMPILED_DIR)/Unifont-APL8x16-$(VERSION).psf.gz \
	                  $(COMPILED_DIR)/unifont-$(VERSION).ttf \
	                  $(COMPILED_DIR)/unifont_csur-$(VERSION).pcf.gz \
	                  $(COMPILED_DIR)/unifont_csur-$(VERSION).ttf \
	                  $(COMPILED_DIR)/unifont_sample-$(VERSION).hex \
	                  $(COMPILED_DIR)/unifont_sample-$(VERSION).bdf.gz \
	                  $(COMPILED_DIR)/unifont_sample-$(VERSION).pcf.gz \
	                  $(COMPILED_DIR)/unifont_sample-$(VERSION).ttf \
	                  $(COMPILED_DIR)/coverage.txt \
	                  $(COMPILED_DIR)/unifont-$(VERSION).bmp \
	                  $(COMPILED_DIR)/unifont_upper-$(VERSION).hex \
	                  $(COMPILED_DIR)/unifont_upper-$(VERSION).ttf \
	                  $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).hex \
	                  $(COMPILED_DIR)/unifont_upper_sample-$(VERSION).ttf \
	   precompiled

#
# This is set up for Debian.  Solaris places fonts in
# "/usr/share/fonts/TrueType".  Other unices use other directories.
# The original font format was BDF, but Debian doesn't use that and
# xfs should only need TrueType, so the BDF font isn't installed.
#
# The TrueType font is about 16 Megabytes.  To avoid duplication,
# the fonts are installed as symbolic links back to the original package.
# Alternatively, the fonts can be copied to the destination directory
# with "install -m0644" or moved there (but "mv" is a destructive
# one-time operation).
#
# After installing the new fonts, register them with X Window System using:
#
#      xset fp rehash
#
install:
	if [ x$(CURDIR) = x ] ; \
	then \
	   echo "Fatal Error: CURDIR not defined -- define in Makefile." ; \
	   exit 1 ; \
	fi
	$(INSTALL) -m0755 -d $(CONSOLEDEST)
	$(INSTALL) -m0755 -d $(PCFDEST)
	$(INSTALL) -m0755 -d $(TTFDEST)
	if [ ! -d $(COMPILED_DIR) ] ; then \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/Unifont-APL8x16-$(VERSION).psf.gz $(CONSOLEDEST)/Unifont-APL8x16.psf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont-$(VERSION).pcf.gz         $(PCFDEST)/unifont.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont_sample-$(VERSION).pcf.gz  $(PCFDEST)/unifont_sample.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont_csur-$(VERSION).pcf.gz    $(PCFDEST)/unifont_csur.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont-$(VERSION).ttf            $(TTFDEST)/unifont.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont_sample-$(VERSION).ttf     $(TTFDEST)/unifont_sample.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont_csur-$(VERSION).ttf       $(TTFDEST)/unifont_csur.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/precompiled/unifont_upper-$(VERSION).ttf      $(TTFDEST)/unifont_upper.ttf ; \
	else \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/Unifont-APL8x16-$(VERSION).psf.gz $(CONSOLEDEST)/Unifont-APL8x16.psf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont-$(VERSION).pcf.gz         $(PCFDEST)/unifont.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont_sample-$(VERSION).pcf.gz  $(PCFDEST)/unifont_sample.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont_csur-$(VERSION).pcf.gz    $(PCFDEST)/unifont_csur.pcf.gz ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont-$(VERSION).ttf            $(TTFDEST)/unifont.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont_sample-$(VERSION).ttf     $(TTFDEST)/unifont_sample.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont_csur-$(VERSION).ttf       $(TTFDEST)/unifont_csur.ttf ; \
	   $(INSTALL) -m0644 -p $(CURDIR)/$(COMPILED_DIR)/unifont_upper-$(VERSION).ttf      $(TTFDEST)/unifont_upper.ttf ; \
	fi

clean:
	\rm -rf $(COMPILED_DIR)/png
	\rm -f $(COMPILED_DIR)/unifont-$(VERSION).hex
	\rm -f $(COMPILED_DIR)/unifont-$(VERSION).bdf
	\rm -f $(COMPILED_DIR)/unifont-$(VERSION).bdf.gz
	\rm -f $(COMPILED_DIR)/unifont-$(VERSION).sfd.gz
	\rm -f *~

#
# Note that distclean leaves precompiled/ alone.  This is intentional.
# The .DS files are created under Mac OS X.
#
distclean:
	\rm -rf $(COMPILED_DIR)
	$(MAKE) -C plane01 distclean
	$(MAKE) -C ttfsrc distclean
	\rm -f *~
	\rm -rf .DS* ._.DS*

.PHONY: all hex bdf pcf coverage pagecount png bigpic csurttf upper ttf \
	precompiled install clean distclean 


# Test ROMs for judging per-layer 3D depth.
#
# Needs cc65 for its 65816 assembler:  apt install cc65
#
#   make            build both ROMs here
#   make install    also copy them onto Azahar's emulated SD card
#
# Mode7Plane.sfc lays a checked plane out in perspective, with the matrix
# rewritten every scanline by HDMA, so the depth a build gives it should grow
# from nothing at the bottom of the screen towards the horizon.
#
# Mode7Flat.sfc draws the same plane at one fixed scale, minified 2:1, so the
# depth should be the same on every scanline: half of what the slot's slider
# says, since half is how far a plane at twice life size lies.

ROMS    := Mode7Plane.sfc Mode7Flat.sfc
SDCARD  := $(HOME)/.local/share/azahar-emu/sdmc

all: $(ROMS)

Mode7Plane.sfc: mode7plane.o mode7plane.cfg
	ld65 -C mode7plane.cfg -o $@ $<

Mode7Flat.sfc: mode7flat.o mode7plane.cfg
	ld65 -C mode7plane.cfg -o $@ $<

mode7plane.o: mode7plane.s
	ca65 --cpu 65816 -o $@ $<

mode7flat.o: mode7plane.s
	ca65 --cpu 65816 -DFLAT -o $@ $<

install: $(ROMS)
	cp $(ROMS) $(SDCARD)/

clean:
	rm -f $(ROMS) *.o

.PHONY: all install clean

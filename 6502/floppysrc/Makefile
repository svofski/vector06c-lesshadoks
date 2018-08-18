C65=../cc65/bin/cc65
A65=../cc65/bin/ca65
L65=../cc65/bin/ld65

WEB_OSD ?= 1

DSTDIR=../../firmware/6502

SRCS:=$(foreach srcfile,./,$(wildcard *.c))
OBJS:=$(patsubst %.c,build/%.o,$(SRCS))
OBJS:=$(OBJS) build/crt0.o
ASSES:=$(patsubst %.c,build/%.s,$(SRCS))

#OBJS=crt0.o philes.o menu.o osd.o slave.o fddimage.o tff.o mmc.o timer.o serial.o main.o
#ASSS=philes.s menu.s osd.s slave.s fddimage.s tff.s mmc.s timer.s serial.s main.s


SIM ?= 0
SIMFLAG=-DSIMULATION=$(SIM)

# snapshot version of cc65 has a patch regarding -Os
# release version will only work with -Oirs
CFLAGS=-Os -t none -T --cpu 6502 -DWEB_OSD=$(WEB_OSD) $(SIMFLAG)

all:	disk

disk:	$(OBJS)
	$(L65) -C vector.lc $(OBJS) ../cc65/lib/vector.lib -o disk.bin -m disk.map
	gobjcopy -Ibinary -Oihex disk.bin disk.hex
	mkdir -p $(DSTDIR)
	mv disk.{hex,bin,map} $(DSTDIR)
	hexdump -v -e '/1 "%02X "' $(DSTDIR)/disk.bin >$(DSTDIR)/disk.hax

clean:
	rm -f build/*

build/%.s:	%.c
	$(C65) $(CFLAGS) -I $(AINC) -I $(CINC) $< -o $@

build/%.o:	build/%.s
	$(A65) -t none $< -o $@

build/%.o:	%.S
	$(A65) -t none $< -o $@

build/%.o:	%.c

#ld65 -C vector.lc lib/vector.o mmc.o lib/vector.lib -o mmc.bin -m mmc.map

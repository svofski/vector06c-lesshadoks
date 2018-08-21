ЛЕШАДОК ПОМПЕ
=============

ЛЕШАДОК ПОМПЕ (Les shadoks pompaient) is an expansion board for Вектор-06ц home computer from USSR era.

![ЛЕШАДОК ПОМПЕ](/doc/photos/lesshadoks1.jpg)

Features (real and planned):
  - [x] RAM disk expansion in SDRAM
  - [x] AY and other sound device emulation
  - [ ] Joystick ports
  - [x] floppy disk emulator
  - [x] floppy images on SD card
  - [ ] floppy images downloaded from the internets
  - [x] ESP12F working as FPGA configurator
  - [x] ESP12F as user interface for SD card image selection
  - [ ] ESP12F for uploading floppy disk images to SDRAM
  - [ ] VI53 sound passthrough
  
Key hardware features:
  * Main chip: EP4CE6 Intel Cyclone IV device
  * 8MB SDRAM (working at 96MHz)
  * ESP12F module
  * Audio output with amplifier
  * Analog input for internal audio mixing (using LVDS sigma-delta ADC)

Board schematic
---------------
[Schematic](/kicad/shadok-cheap/shadok.pdf) (pdf)

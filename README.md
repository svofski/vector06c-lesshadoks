ЛЕШАДОК ПОМПЕ
=============

ЛЕШАДОК ПОМПЕ (Les shadoks pompaient) is an expansion board for Вектор-06ц home computer from USSR era.

![ЛЕШАДОК ПОМПЕ](/doc/photos/lesshadoks1.jpg)

Features (real and planned):
  - [x] RAM disk expansion in SDRAM
  - [x] AY and other sound device emulation
  - [x] Joystick ports
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


Build instructions
------------------
This project consists of many parts and putting it together is not a simple task.
Here I wrote down some instructions primarily for myself to be able to remember
what to do. Good luck!


### PART 1 Building Cosmogol999 (esp8266 firmware)

1. Get the sources
```
git clone --recurse-submodules https://github.com/svofski/vector06c-lesshadoks
```

2. Prepare esp-open-sdk
Follow instructions at https://github.com/pfalcon/esp-open-sdk.
Mind the requirements, then get the SDK and build:
```
git clone --recursive --depth=1 https://github.com/pfalcon/esp-open-sdk.git
cd esp-open-sdk
make STANDALONE=y
```

After the toolchain is ready, do as it says:
```
export PATH=$HOME/esp-open-sdk/xtensa-lx106-elf/bin:$PATH
```
Also export SDK_BASE:
```
export SDK_BASE=$HOME/esp-open-sdk/ESP8266_NONOS_SDK-2.1.0-18-g61248df
```

3. Actually build

```
cd $HOME/vector06c-lesshadoks/esp8266/cosmogol999
make 
``` 

The firmware will be found in 
```
    rboot/firmware/rboot.bin
    firmware/shadki.bin
    blank_config.bin
```

6. Flash
```
make flash
```
This probably will do something destructive to your esp12f.


### PART 2 Building floppy emulator firmware

This one should be easy.
```
cd $HOME/vector06c-lesshadoks/6502
make
```

### PART 3 Building the FPGA programming files

1. Get and install Quartus Lite that supports Cyclone IV E (e.g. 17.1)

2. For the time being there is no makefile, so just open the project 
quartus/pompaient.qpf and press Ctrl-L.

### PART 4 Put it all together

???


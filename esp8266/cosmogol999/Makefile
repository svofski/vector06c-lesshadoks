BUILD_ID=$(shell echo `git describe --all --long | cut -f 3 -d '-'` on `uname -n` `date -R` using `xtensa-lx106-elf-gcc --version|head -1` )

CFLAGS=

# SPIFFS
SPIFFS_SINGLETON = 0
SPIFFS_LOG_PAGE_SIZE ?= 256
SPIFFS_LOG_BLOCK_SIZE ?= 8192

spiffs_CFLAGS += -DSPIFFS_SINGLETON=$(SPIFFS_SINGLETON)
spiffs_CFLAGS += -DSPIFFS_LOG_PAGE_SIZE=$(SPIFFS_LOG_PAGE_SIZE)
spiffs_CFLAGS += -DSPIFFS_LOG_BLOCK_SIZE=$(SPIFFS_LOG_BLOCK_SIZE)
spiffs_CFLAGS += -DSPIFFS_IX_MAP=0

CFLAGS += $(spiffs_CFLAGS)


#SPI flash size, in K
ESP_SPI_FLASH_SIZE_K=4096
#0: QIO, 1: QOUT, 2: DIO, 3: DOUT
ESP_FLASH_MODE=2
#0: 40MHz, 1: 26MHz, 2: 20MHz, 15: 80MHz
ESP_FLASH_FREQ_DIV=0


# Output directors to store intermediate compiled files
# relative to the project directory
BUILD_BASE	= build
FW_BASE		= firmware

# Base directory for the compiler. Needs a / at the end; if not set it'll use the tools that are in
# the PATH.
XTENSA_TOOLS_ROOT ?= 

# base directory of the ESP8266 SDK package, absolute
SDK_BASE	?= /opt/Espressif/ESP8266_SDK

# Opensdk patches stdint.h when compiled with an internal SDK. If you run into compile problems pertaining to
# redefinition of int types, try setting this to 'yes'.
USE_OPENSDK?=yes

#Esptool.py path and port
ESPTOOL		?= esptool.py
ESPPORT		?= /dev/ttyUSB0
#ESPDELAY indicates seconds to wait between flashing the two binary images
ESPDELAY	?= 3
ESPBAUD		?= 460800

#Appgen path and name
APPGEN		?= $(SDK_BASE)/tools/gen_appbin.py

# name for the target project
TARGET		= httpd

# which modules (subdirectories) of the project to include in compiling
MODULES		= driver user rboot/appcode spiffs spiffs/spiffs/src
EXTRA_INCDIR	= include user libesphttpd/include rboot rboot/appcode spiffs spiffs/spiffs/src .

# libraries used in this project, mainly provided by the SDK (libmain2 with weakened Cache_Read_Enable_New)
LIBS		= c gcc hal phy pp net80211 wpa lwip crypto
#Add in esphttpd lib
LIBS += esphttpd

# compiler flags using during compilation of source files
CFLAGS		+= -Os -std=gnu99 -Werror -Wpointer-arith -Wundef -Wall -Wl,-EL -fno-inline-functions \
		-Wl,-gc-sections \
		-nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH \
		-Wno-address -DBUILD_ID="\"$(BUILD_ID)\"" -u _printf_float -u _scanf_float 

#\
#		-ffunction-sections -fdata-sections

# linker flags used to generate the main object file
LDFLAGS		=  -nostdlib -Wl,--no-check-sections -u Cache_Read_Enable_New -u call_user_start -Wl,-static -Wl,-Map=$(BUILD_BASE)/shadki.map


# various paths from the SDK used in this project
SDK_LIBDIR	= lib
SDK_LDDIR	= ld
SDK_INCDIR	= include include/json

# select which tools to use as compiler, librarian and linker
CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCOPY	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy

#Additional (maybe generated) ld scripts to link in
EXTRA_LD_SCRIPTS:=


####
#### no user configurable options below here
####
SRC_DIR		:= $(MODULES)
BUILD_DIR	:= $(addprefix $(BUILD_BASE)/,$(MODULES))

SDK_LIBDIR	:= $(addprefix $(SDK_BASE)/,$(SDK_LIBDIR))
SDK_INCDIR	:= $(addprefix -I$(SDK_BASE)/,$(SDK_INCDIR))

SRC		:= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.c))
ASMSRC		= $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.S))
OBJ		= $(patsubst %.c,$(BUILD_BASE)/%.o,$(SRC))
OBJ		+= $(patsubst %.S,$(BUILD_BASE)/%.o,$(ASMSRC))
APP_AR		:= $(addprefix $(BUILD_BASE)/,$(TARGET)_app.a)


V ?= $(VERBOSE)
ifeq ("$(V)","1")
Q :=
vecho := @true
else
Q := @
vecho := @echo
endif

ifeq ("$(USE_OPENSDK)","yes")
CFLAGS		+= -DUSE_OPENSDK
else
CFLAGS		+= -D_STDINT_H
endif

ifeq ("$(GZIP_COMPRESSION)","yes")
CFLAGS		+= -DGZIP_COMPRESSION
endif

ifeq ("$(USE_HEATSHRINK)","yes")
CFLAGS		+= -DESPFS_HEATSHRINK
endif

ifeq ("$(ESPFS_POS)","")
#No hardcoded espfs position: link it in with the binaries.
LIBS += webpages-espfs
else
#Hardcoded espfs location: Pass espfs position to rest of code
CFLAGS += -DESPFS_POS=$(ESPFS_POS) -DESPFS_SIZE=$(ESPFS_SIZE)
endif

CFLAGS += -DOTA_FLASH_SIZE_K=$(ESP_SPI_FLASH_SIZE_K)


#Define default target. If not defined here the one in the included Makefile is used as the default one.
default-tgt: all

define maplookup
$(patsubst $(strip $(1)):%,%,$(filter $(strip $(1)):%,$(2)))
endef

LD_SCRIPT	= eagle.app.v6.ld

TARGET_OUT	:= $(addprefix $(BUILD_BASE)/,$(TARGET).out)

BLANKPOS="$$(printf "0x%X" $$(($(ESP_SPI_FLASH_SIZE_K)*1024-0x2000)))"
INITDATAPOS="$$(printf "0x%X" $$(($(ESP_SPI_FLASH_SIZE_K)*1024-0x4000)))"

$(TARGET_OUT): $(APP_AR) $(EXTRA_LD_SCRIPTS) $(BUILD_BASE)/libmain2.a
	$(vecho) "LD $@"
	$(Q) $(LD) -Llibesphttpd -L$(SDK_LIBDIR) $(LD_SCRIPT) $(EXTRA_LD_SCRIPTS) $(LDFLAGS) \
	    -Wl,--start-group $(LIBS) $(BUILD_BASE)/libmain2.a $(APP_AR) \
	    $(BUILD_BASE)/rboot/appcode/rboot-bigflash.o -Wl,--end-group -o $@ 


$(FW_BASE): $(TARGET_OUT)
	$(vecho) "FW $@"
	$(Q) mkdir -p $@
	$(Q) $(ESPTOOL) elf2image --version=2 $(ESPTOOL_FLASHDEF) $(TARGET_OUT) --output $@/shadki.bin

flash:	$(TARGET_OUT) $(FW_BASE) rboot/firmware/rboot.bin
	@echo BLANKPOS=$(BLANKPOS) INITDATAPOS=$(INITDATAPOS)
	$(Q) $(ESPTOOL) $(ESPTOOL_OPTS) write_flash $(ESPTOOL_FLASHDEF) 0x0000 rboot/firmware/rboot.bin 0x1000 blank_config.bin \
	    0x2000 $(FW_BASE)/shadki.bin \
	    $(BLANKPOS) $(SDK_BASE)/bin/blank.bin $(INITDATAPOS) $(SDK_BASE)/bin/esp_init_data_default.bin

rboot/firmware/rboot.bin:	.FORCE
	make -C rboot 

#Add all prefixes to paths
LIBS		:= $(addprefix -l,$(LIBS))
LD_SCRIPT := -Trboot.ld
INCDIR	:= $(addprefix -I,$(SRC_DIR))
EXTRA_INCDIR	:= $(addprefix -I,$(EXTRA_INCDIR))
MODULE_INCDIR	:= $(addsuffix /include,$(INCDIR))

ESP_FLASH_SIZE_IX=$(call maplookup,$(ESP_SPI_FLASH_SIZE_K),512:0 1024:2 2048:5 4096:6)
ESPTOOL_FREQ=$(call maplookup,$(ESP_FLASH_FREQ_DIV),0:40m 1:26m 2:20m 0xf:80m 15:80m)
ESPTOOL_MODE=$(call maplookup,$(ESP_FLASH_MODE),0:qio 1:qout 2:dio 3:dout)
ESPTOOL_SIZE=$(call maplookup,$(ESP_SPI_FLASH_SIZE_K),512:4m 256:2m 1024:8m 2048:16m 4096:32m)

ESPTOOL_OPTS=--port $(ESPPORT) --baud $(ESPBAUD)
ESPTOOL_FLASHDEF=--flash_freq $(ESPTOOL_FREQ) --flash_mode $(ESPTOOL_MODE) --flash_size $(ESPTOOL_SIZE)

vpath %.c $(SRC_DIR)
vpath %.S $(SRC_DIR)

define compile-objects
$1/%.o: %.c
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@

$1/%.o: %.S
	$(vecho) "CC $$<"
	$(Q) $(CC) $(INCDIR) $(MODULE_INCDIR) $(EXTRA_INCDIR) $(SDK_INCDIR) $(CFLAGS)  -c $$< -o $$@
endef

.PHONY: all checkdirs clean libesphttpd default-tgt

all: checkdirs $(TARGET_OUT) $(FW_BASE)

libesphttpd/Makefile:
	$(Q) echo "No libesphttpd submodule found. Using git to fetch it..."
	$(Q) git submodule init
	$(Q) git submodule update

libesphttpd: libesphttpd/Makefile
	$(Q) make -C libesphttpd USE_OPENSDK=$(USE_OPENSDK)

$(APP_AR): libesphttpd $(OBJ)
	$(vecho) "AR $@"
	$(Q) $(AR) cru $@ $(OBJ)

checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	$(Q) mkdir -p $@

clean:
	$(Q) make -C libesphttpd clean
	$(Q) make -C rboot clean
	$(Q) rm -f $(APP_AR)
	$(Q) rm -f $(TARGET_OUT)
	$(Q) find $(BUILD_BASE) -type f | xargs rm -f
	$(Q) rm -rf $(FW_BASE)
	
	# cgi.c has BUILD_ID template so rebuild it every time
build/user/cgi.o:	.FORCE

.FORCE:

$(BUILD_BASE)/libmain2.a:	$(SDK_LIBDIR)/libmain.a
	$(Q) xtensa-lx106-elf-objcopy -W Cache_Read_Enable_New $< $@

$(foreach bdir,$(BUILD_DIR),$(eval $(call compile-objects,$(bdir))))

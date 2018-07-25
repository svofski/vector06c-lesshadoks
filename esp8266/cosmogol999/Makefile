ESPPORT ?= /dev/tty.SLAB_USBtoUART
ESPBAUD ?= 460800
ESP_OPEN_RTOS_SDK ?= /Users/svo/tmpsrc/esp-open-rtos
PROGRAM = esphttpd
#EXTRA_COMPONENTS = extras/dhcpserver extras/rboot-ota extras/libesphttpd extras/spiffs
EXTRA_COMPONENTS = extras/dhcpserver extras/rboot-ota ./libesphttpd extras/spiffs

SPIFFS_BASE_ADDR = 0x200000
SPIFFS_SIZE = 0x200000
SPIFFS_SINGLETON = 0

## chip id 4016, 32 mbit / 4 megabytes
FLASH_SIZE = 32
# Lie about flash size, ota only builds well with 1 meg size
OTA_FLASH_SIZE_K=1024

ESP_IP ?= 192.168.4.1

#Tag for OTA images. 0-27 characters. Change to eg your projects title.
LIBESPHTTPD_OTA_TAGNAME ?= generic

LIBESPHTTPD_MAX_CONNECTIONS ?= 8
LIBESPHTTPD_STACKSIZE ?= 2048

BUILD_ID=$(shell echo `xtensa-lx106-elf-gcc --version|head -1` on `uname -n` `date -R`)

PROGRAM_CFLAGS += -DFREERTOS -DLIBESPHTTPD_OTA_TAGNAME="\"$(LIBESPHTTPD_OTA_TAGNAME)\"" \
		  -DFLASH_SIZE=$(FLASH_SIZE) -DOTA_FLASH_SIZE_K=$(OTA_FLASH_SIZE_K) -DRBOOT_OTA=1 \
		  -DBUILD_ID="\"$(BUILD_ID)\""
EXTRA_CFLAGS += -DMEMP_NUM_NETCONN=$(LIBESPHTTPD_MAX_CONNECTIONS)

include $(ESP_OPEN_RTOS_SDK)/common.mk

#$(eval $(call make_spiffs_image,files))

/*
   Some flash handling cgi routines. Used for updating the ESPFS/OTA image.
   */

/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <string.h>

#include <etstimer.h>
#include <espressif/esp_timer.h>
#include <espressif/user_interface.h>
#include <espressif/spi_flash.h>
#include <espressif/osapi.h>
//#include <osapi.h>

#include "rboot-api.h"

#include <libesphttpd/cgiflash.h>
#include <libesphttpd/espfs.h>
#include <libesphttpd/cgiflash.h>

#ifndef UPGRADE_FLAG_FINISH
#define UPGRADE_FLAG_FINISH     0x02
#endif

#define FW_SIZE (((OTA_FLASH_SIZE_K*1024)/2)-0x2000)

// Check that the header of the firmware blob looks like actual firmware...
static int ICACHE_FLASH_ATTR checkBinHeader(void *buf) {
    uint8_t *cd = (uint8_t *)buf;
    if (cd[0] != 0xEA) return 0;
    if (cd[1] != 4 || cd[2] > 3 || cd[3] > 0x40) return 0;
    if (((uint16_t *)buf)[3] != 0x4010) return 0;
    if (((uint32_t *)buf)[2] != 0) return 0;
    return 1;
}

// Cgi to query which firmware needs to be uploaded next
int ICACHE_FLASH_ATTR cgiGetFirmwareNext2(HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }
    rboot_config bootconf = rboot_get_config();
    uint8 id = bootconf.current_rom;

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdHeader(connData, "Content-Length", "9");
    httpdEndHeaders(connData);
    char *next = id == 1 ? "user1.bin" : "user2.bin";
    httpdSend(connData, next, -1);
    printf("Next firmware: %s (got %d)\n", next, id);
    return HTTPD_CGI_DONE;
}


#define FLST_START 0
#define FLST_WRITE 1
#define FLST_DONE 3
#define FLST_ERROR 4

typedef struct {
    int state;
    char *err;
} UploadState;

rboot_config bootconf;
rboot_write_status rboot_status;

int ICACHE_FLASH_ATTR cgiUploadFirmware2(HttpdConnData *connData) {
    UploadState *state=(UploadState *)connData->cgiData;

    if (connData->conn == NULL) {
        //Connection aborted. Clean up.
        if (state != NULL) free(state);
        return HTTPD_CGI_DONE;
    }

    if (state==NULL) {
        //First call. Allocate and initialize state variable.
        printf("Firmware upload cgi start.\n");
        state=malloc(sizeof(UploadState));
        if (state==NULL) {
            printf("Can't allocate firmware upload struct!\n");
            return HTTPD_CGI_DONE;
        }
        memset(state, 0, sizeof(UploadState));
        state->state=FLST_START;
        connData->cgiData=state;
        state->err="Premature end";
    }

    uint8_t slot;

    if (state->state==FLST_START) {
        //First call. Assume the header of whatever we're uploading 
        //already is in the POST buffer.
        if (checkBinHeader(connData->post->buff)) {
            if (connData->post->len > FW_SIZE) {
                state->err="Firmware image too large";
                state->state=FLST_ERROR;
            } else {
                bootconf = rboot_get_config();
                slot = bootconf.current_rom;
                state->state = FLST_WRITE;
                slot = slot == 1 ? 0 : 1;
                printf("Flashing slot %i to @%x\n", slot, bootconf.roms[slot]);
                rboot_status = rboot_write_init(bootconf.roms[slot]);
            }
        } 
        else {
            state->err = "Invalid flash image type!";
            state->state = FLST_ERROR;
            printf("Did not recognize flash image type!\n");
        }
    } 

    if (state->state == FLST_WRITE) {
        if (!rboot_write_flash(&rboot_status, 
                    (uint8_t *)connData->post->buff,
                    connData->post->buffLen)) {
            state->err = "Error writing to flash";
            state->state = FLST_ERROR;
        } 
        else if (connData->post->len == connData->post->received) {
            state->err = "No error";
            state->state = FLST_DONE;
        }
    }

    if (state->state != FLST_WRITE) {
        //We're done! Format a response.
        printf("Upload done. Sending response.\n");
        httpdStartResponse(connData, state->state==FLST_ERROR ? 400 : 200);
        httpdHeader(connData, "Content-Type", "text/plain");
        httpdEndHeaders(connData);
        if (state->state != FLST_DONE) {
            httpdSend(connData, "Firmware image error:", -1);
            httpdSend(connData, state->err, -1);
            httpdSend(connData, "\n", -1);
        } else {
            bootconf.current_rom = bootconf.current_rom == 0 ? 1 : 0;
        }
        free(state);
        return HTTPD_CGI_DONE;
    }

    return HTTPD_CGI_MORE;
}

static ETSTimer resetTimer;

static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
    sdk_system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
    rboot_set_current_rom(bootconf.current_rom);
    sdk_system_restart();
    //rboot_set_config(&bootconf);
    //sdk_system_upgrade_reboot();
}

// Handle request to reboot into the new firmware
int ICACHE_FLASH_ATTR cgiRebootFirmware2(HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    //Do reboot in a timer callback so we still have time to send the response.
    sdk_os_timer_disarm(&resetTimer);
    sdk_os_timer_setfn(&resetTimer, resetTimerCb, NULL);
    sdk_os_timer_arm(&resetTimer, 200, 0);

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);
    httpdSend(connData, "Rebooting...", -1);
    return HTTPD_CGI_DONE;
}

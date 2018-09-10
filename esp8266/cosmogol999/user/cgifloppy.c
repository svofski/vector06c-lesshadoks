#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <esp8266.h>
#include "httpd.h"

#include "spiffs.h"
#include "esp_spiffs.h"
#include "slave.h"
#include "inifile.h"

char mode_str[8];
uint8_t mode_code;

enum _modecode {
    MODE_FDD = 0,
    MODE_EDD = 1,
    MODE_ROM = 2,
    MODE_BOOT = 3,
};

static void master_dir(HttpdConnData * con);
static enum _modecode str2modecode();
static void update_mode_code();

ICACHE_FLASH_ATTR 
enum _modecode str2modecode()
{
    if (strcmp(mode_str, "fdd")) return MODE_FDD;
    if (strcmp(mode_str, "edd")) return MODE_EDD;
    if (strcmp(mode_str, "rom")) return MODE_ROM;
    if (strcmp(mode_str, "boot")) return MODE_BOOT;
    printf("str2modecode: weird mode_str [%s], defaulting to fdd\n", 
            mode_str);
    return MODE_FDD;
}

ICACHE_FLASH_ATTR
void update_mode_code()
{
    mode_code = str2modecode();
}

int ICACHE_FLASH_ATTR cgiFloppyCatalog(HttpdConnData *connData) {
    char mode[sizeof(mode_str)];

    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    slave_setstate(SLAVE_CATALOG);

    httpdGetHeader(connData, "X-Mode", mode, sizeof(mode)-1);

    // if no mode provided, get the current mode from inifile
    if (strlen(mode) == 0) {
        const char * inimode = inifile_get_mode();
        if (inimode) {
            strncpy(mode_str, inimode, sizeof(mode_str));
        }
    }
    // update inifile mode if the new mode differs
    if (strncmp(mode_str, mode, sizeof(mode_str)) != 0) {
        strncpy(mode_str, mode, sizeof(mode_str));
        printf("cgiFloppyCatalog: set new mode: [%s]\n", mode);
        inifile_set_mode(mode);
    }
    update_mode_code();

    printf("cgiFloppyCatalog: requesting catalog [%s]\n", mode_str);

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);

    master_dir(connData);

    printf("cgiFloppyCatalog: catalog done\n");

    slave_setstate(SLAVE_VOID);
    
    return HTTPD_CGI_DONE;
}

static int catalog_num = 0;

void ICACHE_FLASH_ATTR master_dir(HttpdConnData * con)
{
    httpdSend(con, "{\"mode\":\"",-1);
    httpdSend(con, mode_str, -1);
    httpdSend(con, "\"", -1);
    
    httpdSend(con, "\"files\":[", -1);

    // post a request, expect quick answer
    // TODO implement using callbacks with HTTPD_CGI_MORE
    catalog_num = 0;

    // post request: STM_CATALOG [len=1] [mode_code]
    slave_post_user_command(STM_CATALOG, &mode_code, 1);

    uint32_t start = system_get_time();

    int ntimeouts = 0;
    int npolls = 0;

    master_status_t ms;
    int comma = 0;
    do {
        uint8_t * data; 
        uint8_t len;
        uint8_t token;
        ms = slave_poll_response(&data, &len, &token);

        if (ms == MSTAT_POLL) {
            printf("unexpected mts_poll %d\n", ++npolls);
            if (npolls > 10) break;
        }

        system_soft_wdt_feed();
        if (len > 0) {
            start = system_get_time();

            printf("DATA: %d %s %02x\n", len, data, token);

            httpdSend(con, comma ? ",\"" : "\"", -1);
            httpdSend(con, (char *)data, len);
            httpdSend(con, "\"", 1);
            slave_ready(token);
            ++comma;
        }

        if (system_get_time() - start > 500000) {
            start = system_get_time();
            printf("TIMEOUT\n");
            //slave_ready(0);
            ++ntimeouts;
            //if (ntimeouts == 4) slave_ready(0);
            //if (++ntimeouts > 8) break;
        }
    } while (ms != MSTAT_END);
    system_soft_wdt_restart();

    httpdSend(con, "]}", -1);
}

void ICACHE_FLASH_ATTR wait_mts_end()
{
    master_status_t ms;
    do {
        uint8_t * data; 
        uint8_t len, token;
        ms = slave_poll_response(&data, &len, &token);
        system_soft_wdt_feed();
    } while (ms != MSTAT_END);
    system_soft_wdt_restart();
}

int ICACHE_FLASH_ATTR cgiFloppySelect(HttpdConnData *connData) {
    if (connData->conn==NULL) {
        return HTTPD_CGI_DONE;
    }

    slave_setstate(SLAVE_SELECT);

    printf("cgiFloppySelect\n");
    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);

    char cmdbuf[32];

    httpdGetHeader(connData, "X-Drive", cmdbuf, 2);
    httpdGetHeader(connData, "X-FileName", cmdbuf+1, 29);

    printf("X-Drive: %c", cmdbuf[0]);
    printf("X-FileName: %s", cmdbuf+1);

    slave_post_user_command(STM_SELECT, (uint8_t *)cmdbuf, strlen(cmdbuf));
    /* make sure that the next poll comes before we request catalog */
    printf("waiting for mts_end\n");
    wait_mts_end();
    printf("okay!\n");

    slave_setstate(SLAVE_VOID);

    return HTTPD_CGI_DONE;
}



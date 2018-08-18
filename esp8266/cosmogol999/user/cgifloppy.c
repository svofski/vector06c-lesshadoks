#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <esp8266.h>
#include "httpd.h"

#include "spiffs.h"
#include "esp_spiffs.h"
#include "slave.h"

static void master_dir(HttpdConnData * con);

//int ICACHE_FLASH_ATTR 
//tplFloppyMain(HttpdConnData *connData, char *token, void **arg) 
//{
//    char buff[128];
//    if (token==NULL) return HTTPD_CGI_DONE;
//
//    strcpy(buff, "Unknown");
//    if (strcmp(token, "spiffs_size") == 0) {
//        strncpy(buff, "bloody much", sizeof(buff));
//    } else if (strcmp(token, "mount_status") == 0) {
//        strncpy(buff, mount_msg, sizeof(buff));
//    } else if (strcmp(token, "directory") == 0) {
//        return spiffs_dir(connData);
//    }
//    httpdSend(connData, buff, -1);
//    return HTTPD_CGI_DONE;
//}


int ICACHE_FLASH_ATTR cgiFloppyCatalog(HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }

    printf("cgiFloppyCatalog: requesting catalog\n");
    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);

    master_dir(connData);

    printf("cgiFloppyCatalog: catalog done\n");
    
    return HTTPD_CGI_DONE;
}

static int catalog_num = 0;

void ICACHE_FLASH_ATTR master_dir(HttpdConnData * con)
{
    httpdSend(con, "{\"files\":[", -1);

    // post a request, expect quick answer
    // TODO implement using callbacks nad HTTPD_CGI_MORE
    catalog_num = 0;
    slave_post_user_command(STM_CATALOG, NULL, 0);

    uint32_t start = system_get_time();

    int ntimeouts = 0;

    master_status_t ms;
    do {
        uint8_t * data; 
        uint8_t len;
        uint8_t token;
        ms = slave_poll_response(&data, &len, &token);

        system_soft_wdt_feed();
        if (len > 0) {
            start = system_get_time();

            printf("DATA: %d %s %02x\n", len, data, token);
            httpdSend(con, "\"", 1);
            httpdSend(con, (char *)data, len);
            httpdSend(con, "\"", 1);
            if (ms == MSTAT_MORE) {
                httpdSend(con, ",", 1);
            }

            slave_ready(token);
        }

        if (system_get_time() - start > 500000) {
            printf("TIMEOUT\n");
            slave_ready(0);
            if (++ntimeouts > 8) break;
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

    return HTTPD_CGI_DONE;
}



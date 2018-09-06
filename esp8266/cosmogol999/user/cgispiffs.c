#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <esp8266.h>
#include "httpd.h"

#include "spiffs.h"
#include "esp_spiffs.h"
#include "inifile.h"

const char * mount_msg;

static const char * M_MOUNTED_OK = "успешно смонтирована";
static const char * M_REMOUNTED_OK = "отформатирована заново";
static const char * M_FORMATTED_BUT_FAIL = "отформатирована, но не смонтирована";
static const char * M_FORMAT_FAILED = "не получилось отформатировать";

extern spiffs fs;

ICACHE_FLASH_ATTR void spiffs_init()
{
    esp_spiffs_init(0x200000,0x200000-0x4000);

    mount_msg = M_MOUNTED_OK;
    if (esp_spiffs_mount() != SPIFFS_OK) {
        printf("Could not mount SPIFFS, will try to reformat\n");
        if (SPIFFS_format(&fs) == SPIFFS_OK) {
            if (esp_spiffs_mount() != SPIFFS_OK) {
                printf("Could not remount SPIFFS\n");
                mount_msg = M_FORMATTED_BUT_FAIL;
            } else {
                mount_msg = M_REMOUNTED_OK;
            }
        } 
        else {
            printf("SPIFFS_open errno %d\n", SPIFFS_errno(&fs));
            printf("ERROR: could not reformat SPIFFS. This is bad.\n");
            mount_msg = M_FORMAT_FAILED;
        } 
    } 
    printf("SPIFFS %s\n", mount_msg);
}

ICACHE_FLASH_ATTR void spiffs_set_boot(const char * name) 
{
    printf("Trying to set boot to: %s\n", name);
#if 0
    spiffs_file fdw = SPIFFS_open(&fs, ".config", 
            SPIFFS_O_CREAT|SPIFFS_O_WRONLY, 0);
    if (fdw < 0) {
        printf("SPIFFS_open errno %d\n", SPIFFS_errno(&fs));
        return;
    }

    char buf[256];
    snprintf(buf, 256, "boot=%s\n", name);
    if (SPIFFS_write(&fs, fdw, &buf, strlen(buf)) != strlen(buf)) {
        printf("SPIFFS_write errno %d\n", SPIFFS_errno(&fs));
    }
    SPIFFS_close(&fs, fdw);
#else
    inifile_set_boot(name);
#endif
}

#if 0
ICACHE_FLASH_ATTR void spiffs_get_boot(char * name, int len)
{
    *name = '\0';
    spiffs_file fdr = SPIFFS_open(&fs, ".config", SPIFFS_O_RDONLY, 0);
    if (fdr < 0) return;

    char buf[256];
    int nread = SPIFFS_read(&fs, fdr, buf, sizeof(buf));
    if (nread > 0) {
        char * boot = strstr(buf, "boot=");
        if (boot != NULL) {
            boot += strlen("boot="); 
            char * eol = strchr(buf, '\n');
            if (eol != NULL) {
                *eol = '\0';
            } else {
                buf[sizeof(buf)-1] = '\0';
            }

            strncpy(name, boot, len);
        }
    }
    SPIFFS_close(&fs, fdr);

    /* verify that file exists */
    fdr = SPIFFS_open(&fs, name, SPIFFS_O_RDONLY, 0);
    if (fdr >= 0) {
        SPIFFS_close(&fs, fdr);
    }
    else {
        *name = '\0';
    }
}
#endif

ICACHE_FLASH_ATTR size_t free_space()
{
    uint32_t total, used;

    SPIFFS_info(&fs, &total, &used);
    return total-used;
}

int ICACHE_FLASH_ATTR spiffs_dir(HttpdConnData * con)
{
    char buf[256];
    uint32_t total, used;

    spiffs_DIR d;
    struct spiffs_dirent e;
    struct spiffs_dirent *pe = &e;

    SPIFFS_info(&fs, &total, &used);

#if 0
    char boot[64];
    spiffs_get_boot(boot, 64);
#endif
    const char * boot = inifile_get_boot();

    snprintf(buf, sizeof(buf), 
            "{\"info\":{\"total\":%u,\"used\":%u,\"boot\":\"%s\"},"
            "\"files\":[", total, used, boot);
    httpdSend(con, buf, -1);

    SPIFFS_opendir(&fs, "/", &d);
    int first = 1;
    while ((pe = SPIFFS_readdir(&d, pe))) {
        snprintf(buf, sizeof(buf), 
                "%s{\"fsid\":%u,\"name\":\"%s\",\"size\":%u}", 
                first ? "" : ",", pe->obj_id, pe->name, pe->size);
        httpdSend(con, buf, -1);
        first = 0;
    }
    SPIFFS_closedir(&d);
    httpdSend(con, "]}", -1);

    return HTTPD_CGI_DONE;
}

// general template parameters
int ICACHE_FLASH_ATTR tplFPGA(HttpdConnData *connData, char *token, void **arg) 
{
    char buff[128];
    if (token==NULL) return HTTPD_CGI_DONE;

    strcpy(buff, "Unknown");
    if (strcmp(token, "spiffs_size") == 0) {
        strncpy(buff, "bloody much", sizeof(buff));
    } else if (strcmp(token, "mount_status") == 0) {
        strncpy(buff, mount_msg, sizeof(buff));
    } else if (strcmp(token, "directory") == 0) {
        return spiffs_dir(connData);
    }
    httpdSend(connData, buff, -1);
    return HTTPD_CGI_DONE;
}

#define S_START 0
#define S_WRITE 1
#define S_ERROR 10
#define S_DONE  11
typedef struct _fus {
    int state;
    const char * err;
    spiffs_file fd;
} upload_state_t;

int ICACHE_FLASH_ATTR cgiUploadFile(HttpdConnData *connData) {
    //CgiUploadFlashDef *def=(CgiUploadFlashDef*)connData->cgiArg;
    upload_state_t *state=(upload_state_t *)connData->cgiData;
    char buf[128];

    if (connData->conn == NULL) {
        //Connection aborted. Clean up.
        if (state!=NULL) free(state);
        return HTTPD_CGI_DONE;
    }

    if (state == NULL) {
        //First call. Allocate and initialize state variable.
        printf("File upload cgi start.\n");
        state = malloc(sizeof(upload_state_t));
        if (state == NULL) {
            printf("Can't allocate file upload struct!\n");
            return HTTPD_CGI_DONE;
        }
        memset(state, 0, sizeof(upload_state_t));
        state->state = S_START;
        connData->cgiData = state;
        state->err = "Premature end";

        httpdGetHeader(connData, "X-FileName", buf, 128);
        printf("X-FileName: %s\n", buf);
        printf("Reported file size: %u\n", connData->post->len);

        if (connData->post->len > free_space()) {
            state->err = "Not enough free space";
            state->state = S_ERROR;
        }
        else {
            spiffs_file fd = SPIFFS_open(&fs, buf, SPIFFS_CREAT | 
                    SPIFFS_TRUNC | SPIFFS_WRONLY, 0);
            if (fd < 0) {
                printf("SPIFFS_open errno %d\n", SPIFFS_errno(&fs));
                state->err = "File creation error";
                state->state = S_ERROR;
            } else {
                state->state = S_WRITE;
                state->fd = fd;
            }
        }
    }

    char *data = connData->post->buff;
    int dataLen = connData->post->buffLen;

    if (state->state == S_WRITE && dataLen != 0) {
        if (SPIFFS_write(&fs, state->fd, data, dataLen) < 0) {
            printf("SPIFFS_write errno %d\n", SPIFFS_errno(&fs));
            state->err = "File write error";
            state->state = S_ERROR;
        }
    }

    if (connData->post->len == connData->post->received) {
        SPIFFS_close(&fs, state->fd);

        if (state->state != S_ERROR) {
            state->state = S_DONE;
            state->err = "No error";
        }

        printf("Upload done. Sending response. state=%d\n", state->state);
        httpdStartResponse(connData, state->state == S_ERROR ? 400 : 200);
        httpdHeader(connData, "Content-Type", "text/plain");
        httpdEndHeaders(connData);
        if (state->state != S_DONE) {
            httpdSend(connData, "Firmware image error:", -1);
            httpdSend(connData, state->err, -1);
            httpdSend(connData, "\n", -1);
        } else {
            //httpdSend(connData, "File uploaded ok", -1);
            httpdSend(connData, "Файл успешно закачан", -1);
            httpdSend(connData, "\n", -1);
        }
        free(state);
        return HTTPD_CGI_DONE;
    }

    return HTTPD_CGI_MORE;
}

int ICACHE_FLASH_ATTR cgiDirectory(HttpdConnData *connData) {
    if (connData->conn==NULL) {
        //Connection aborted. Clean up.
        return HTTPD_CGI_DONE;
    }
    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    //httpdHeader(connData, "Content-Length", "9");
    httpdEndHeaders(connData);

    spiffs_dir(connData);

    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR delete_by_ids(char * ids) 
{
    spiffs_DIR d;
    struct spiffs_dirent e;
    struct spiffs_dirent *pe = &e;

    printf("Deleting files. Id list: %s\n", ids);
    char * aid;

    while ((aid = strsep(&ids, ";")) != NULL) {
        int id = atoi(aid);
        printf("Looking for file with id=%d...", id);

        SPIFFS_opendir(&fs, "/", &d);
        while ((pe = SPIFFS_readdir(&d, pe))) {
            printf("%d...", pe->obj_id);
            if (pe->obj_id == id) {
                printf("found, removing %s\n", pe->name);
                SPIFFS_remove(&fs, (const char *)pe->name);
                break;
            }
        }
        SPIFFS_closedir(&d);
        printf("next\n");
    }

    return 1;
}

int ICACHE_FLASH_ATTR cgiDelete(HttpdConnData *connData) {
    char buf[256];

    if (connData->conn == NULL) {
        return HTTPD_CGI_DONE;
    }
    httpdGetHeader(connData, "X-SPIFFS-ids", buf, sizeof(buf));
    
    delete_by_ids(buf);

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);
    // rien a dire
    return HTTPD_CGI_DONE;
}

int ICACHE_FLASH_ATTR cgiSelectBoot(HttpdConnData *connData) {
    char buf[256];

    if (connData->conn == NULL) {
        return HTTPD_CGI_DONE;
    }
    httpdGetHeader(connData, "X-FileName", buf, sizeof(buf));
    
    spiffs_set_boot(buf); 

    httpdStartResponse(connData, 200);
    httpdHeader(connData, "Content-Type", "text/plain");
    httpdEndHeaders(connData);
    // rien a dire
    return HTTPD_CGI_DONE;
}

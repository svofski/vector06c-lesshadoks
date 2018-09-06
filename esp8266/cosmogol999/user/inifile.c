#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <esp8266.h>
#include "httpd.h"

#include "spiffs.h"
#include "esp_spiffs.h"

extern spiffs fs;

typedef enum _cb_result {
    CBR_CONT,
    CBR_STOP,
} cb_result_t;

#define ITER_ENDED   0
#define ITER_STOPPED 1

#define REWRITE_NOT_HAPPEN 0
#define REWRITE_HAPPEN 1

typedef cb_result_t (*line_iter_cb)(spiffs_file, 
        char *, const char *, const char *);

static int rewrite_result;
static char buf[256];

ICACHE_FLASH_ATTR
int iterate_lines(spiffs_file fdr, spiffs_file fdw, line_iter_cb line_cb,
        const char * key, const char * value)
{
    uint32_t pos = 0; // file position
    int nread = 0;
    do {
        nread = SPIFFS_read(&fs, fdr, buf, sizeof(buf) - 2);
        if (nread > 0) {
            char * nl = memchr(buf, '\n', sizeof(buf) - 2);
            if (nl) {
                *nl = '\0';
            } else {
                buf[nread] = '\0';
            }
            size_t linelen = strlen(buf);
            pos += linelen + 1;
            SPIFFS_lseek(&fs, fdr, pos, SPIFFS_SEEK_SET);
            if (line_cb(fdw, buf, key, value) == CBR_STOP) {
                return ITER_STOPPED;
            }
        }
    } while(nread > 0);
    return ITER_ENDED;
}

ICACHE_FLASH_ATTR
int append_value(spiffs_file fdw, const char * key, const char * value) 
{
    strcpy(buf, key);
    strcpy(buf + strlen(key), value);
    size_t len = strlen(buf);
    buf[len] = '\n';
    int res = SPIFFS_write(&fs, fdw, buf, len + 1);
    if (res != len + 1) {
        printf("append_value: write errno %d\n", SPIFFS_errno(&fs));
    }
    return res;
}

ICACHE_FLASH_ATTR
cb_result_t copyline(spiffs_file fdw, char * s, const char * key, 
        const char * value)
{
    size_t len = strlen(s);
    *(s + len) = '\n';
    if (SPIFFS_write(&fs, fdw, s, len + 1) != len + 1) {
        printf("copyline: write errno %d\n", SPIFFS_errno(&fs));
    }
    return CBR_CONT;
}

ICACHE_FLASH_ATTR
cb_result_t rewrite_cb(spiffs_file fdw, char * s, const char * key, 
        const char * value)
{
    const int keylen = strlen(key);

    if (strstr(s, key) == s) {
        strcpy(s + keylen, value);
        *(s + keylen + strlen(value)) = '\0';
        rewrite_result = REWRITE_HAPPEN;
    } 
    return copyline(fdw, s, key, value);
}

ICACHE_FLASH_ATTR
cb_result_t retr_cb(spiffs_file fdw, char * s, const char * key, 
        const char * value)
{
    if (strstr(s, key) == s) {
        return CBR_STOP;
    }
    return CBR_CONT;
}


ICACHE_FLASH_ATTR
void inifile_set_value(const char * key, const char * value)
{
    spiffs_file fdw = SPIFFS_open(&fs, ".config.tmp", 
            SPIFFS_O_CREAT|SPIFFS_O_WRONLY, 0);
    if (fdw < 0) {
        printf("inifile_set_value: fdw open error %d\n", SPIFFS_errno(&fs));
        return;
    }

    spiffs_file fdr = SPIFFS_open(&fs, ".config", SPIFFS_O_RDONLY, 0);
    if (fdr < 0) {
        printf("inifile_set_value: fdr open error %d\n", SPIFFS_errno(&fs));
        return;
    }

    rewrite_result = REWRITE_NOT_HAPPEN;
    iterate_lines(fdr, fdw, rewrite_cb, key, value);

    if (rewrite_result == REWRITE_NOT_HAPPEN) {
        append_value(fdw, key, value);
    }

    SPIFFS_close(&fs, fdr);
    SPIFFS_close(&fs, fdw);

    SPIFFS_remove(&fs, ".config");
    int res = SPIFFS_rename(&fs, ".config.tmp", ".config");
    if (res < 0) {
        printf("inifile_set_value: rename error %d\n", SPIFFS_errno(&fs));
    }

}

ICACHE_FLASH_ATTR
void inifile_get_value(const char * key, const char ** value)
{
    spiffs_file fdr = SPIFFS_open(&fs, ".config", SPIFFS_O_RDONLY, 0);
    if (fdr < 0) {
        printf("inifile_get_value: open error %d\n", SPIFFS_errno(&fs));
    }

    if (iterate_lines(fdr, 0, retr_cb, key, NULL) == ITER_STOPPED) {
        *value = buf + strlen(key);
    } else {
        *value = NULL;
    }

    SPIFFS_close(&fs, fdr);
}

ICACHE_FLASH_ATTR
void inifile_set_fdd(const char * name)
{
    inifile_set_value("fdd=", name);
}

ICACHE_FLASH_ATTR
void inifile_set_boot(const char * name)
{
    inifile_set_value("boot=", name);
}

ICACHE_FLASH_ATTR
void inifile_set_rom(const char * name)
{
    inifile_set_value("rom=", name);
}

ICACHE_FLASH_ATTR
void inifile_set_mode(const char * mode)
{
    inifile_set_value("mode=", mode);
}

ICACHE_FLASH_ATTR
const char * inifile_get_boot()
{
    const char * result;
    inifile_get_value("boot=", &result);
    return result;
}

ICACHE_FLASH_ATTR
const char * inifile_get_mode()
{
    const char * result;
    inifile_get_value("mode=", &result);
    return result;
}

ICACHE_FLASH_ATTR
const char * inifile_get_fdd()
{
    const char * result;
    inifile_get_value("fdd=", &result);
    return result;
}

ICACHE_FLASH_ATTR
const char * inifile_get_rom()
{
    const char * result;
    inifile_get_value("rom=", &result);
    return result;
}

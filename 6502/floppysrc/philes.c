// ====================================================================
//                         VECTOR-06C FPGA REPLICA
//
//                  Copyright (C) 2007,2008 Viacheslav Slavinsky
//
// This code is distributed under modified BSD license. 
// For complete licensing information see LICENSE.TXT.
// -------------------------------------------------------------------- 
//
// An open implementation of Vector-06C home computer
//
// Author: Viacheslav Slavinsky, http://sensi.org/~svo
// 
// Source file: philes.c
//
// FAT FS toplevel interface
//
// --------------------------------------------------------------------

#include "integer.h"
#include "philes.h"
#include "diskio.h"
#include "tff.h"

#include <string.h>

FATFS 		fatfs;
FILINFO 	finfo;
DIR 		dir;

char * fdda = "/V06C/FDD/xxxxxxxx.xxx\0\0\0";
char * fdda_name;
char * fddb = "/V06C/FDD/xxxxxxxx.xxx\0\0\0";
char * fddb_name;
char * edd  = "/V06C/EDD/xxxxxxxx.xxx\0\0\0";
char * edd_name;
char * rom  = "/V06C/ROM/xxxxxxxx.xxx\0\0\0";
char * rom_name;
char * fixup= "/V06C/ROM/xxxxxxxx.xxx\0\0\0";
char * fixup_name;
char * scratch_full = "/V06C/xxx/xxxxxxxx.xxx\0\0\0";
char * scratch_sub;
char * scratch_name;

BYTE endsWith(char *s1, const char *suffix) {
    int s1len = strlen(s1);
    int sulen = strlen(suffix);

    if (sulen > s1len) return 0;

    return strcmp(&s1[s1len - sulen], suffix) == 0;
}

FRESULT philes_mount() {
    FRESULT result = FR_NO_FILESYSTEM;

    scratch_sub = scratch_full + 6;
    scratch_name = scratch_full + 10;
    fdda_name = fdda + 10;
    fddb_name = fddb + 10;
    edd_name = edd + 10;
    rom_name = rom + 10;
    fixup_name = fixup + 10;

    disk_initialize(0); 
    return f_mount(0, &fatfs);
}

FRESULT philes_opendir(const char * sub) {
    FRESULT result;
    int ofs;

    strcpy(scratch_sub, sub);
    result = f_opendir(&dir, scratch_full);
    ofs = strlen(scratch_full);
    scratch_full[ofs] = '/';
    scratch_name = scratch_full + ofs + 1;

    return result;
}

static void strxcpy(char *dst, char *src) {
    uint8_t i = 12;
    while (*src != 0 && i--) *dst++ = *src++;
}

// fill in file name in buffer pointed by filename
FRESULT philes_nextfile(char *filename, uint8_t terminate) {
    while ((f_readdir(&dir, &finfo) == FR_OK) && finfo.fname[0]) {
        if (finfo.fattrib & AM_DIR) {
            // nowai
        } else {
            if (filename != 0) {
                if (terminate) {
                    strncpy(filename, finfo.fname, 13);
                } else {
                    strxcpy(filename, finfo.fname);
                }
            }
            return 0;
        }
    }

    return FR_NO_FILE;
}

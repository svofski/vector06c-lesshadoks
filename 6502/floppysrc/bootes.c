#include "integer.h"
#include "philes.h"
#include "specialio.h"
#include <string.h>

static FIL bootfile;
static char filename[14];

static void enable_fakerom(void);

unsigned char * sdram_window = (unsigned char *)(0x5000);

void load_boot()
{
    philes_mount();
    if (philes_opendir() == FR_OK) {
        while(philes_nextfile(filename, 1) == FR_OK) {
            if (strcmp(filename, "BOOT.BIN") == 0) {
                enable_fakerom(); 
            }
        }
    }
}

void enable_fakerom()
{
    UINT bytesread;
    SDRAM_PAGE = 8;
    if (f_open(&bootfile, filename, FA_READ) == FR_OK) {
        f_read(&bootfile, sdram_window, 2048, &bytesread);
        if (bytesread == 2048) {
            // great success, tell kvaz to spoof read access in the low 2K 
            // until rus/lat starts blinking
            FAKEROM = 1;
        }
    }
}

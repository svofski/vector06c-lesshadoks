#include "integer.h"
#include "philes.h"
#include "specialio.h"
#include "serial.h"
#include <string.h>

static FIL bootfile;
extern char * scratch_full;
extern char * scratch_name;

static void enable_fakerom(void);

unsigned char * sdram_window = (unsigned char *)(0x5000);

void load_boot()
{
    philes_mount();
    if (philes_opendir(SUB_BOOT) == FR_OK) {
        while(philes_nextfile(scratch_name, 1) == FR_OK) {
            ser_puts(scratch_full);
            if (strcmp(scratch_name, "BOOTS.BIN") == 0) {
                ser_puts(" enabling fakerom: ");
                enable_fakerom(); 
                break;
            }
            ser_nl();
        }
    }
}

void enable_fakerom()
{
    UINT bytesread;

    // should be 8 but it requires routing an extra address line between 
    // kvaz and sdram_arbitre. keep it simple for now.
    SDRAM_PAGE = 0;  

    if (f_open(&bootfile, scratch_full, FA_READ) == FR_OK) {
        ser_puts("open");
        f_read(&bootfile, sdram_window, 2048, &bytesread);
        ser_puts("read: "); print_hex(bytesread>>8); print_hex(bytesread);
        if (bytesread == 2048) {
            ser_puts(" FAKEROM=1");
            // great success, tell kvaz to spoof read access in the low 2K 
            // until rus/lat starts blinking
            FAKEROM = 1;
            
            //ser_nl();
            //print_buff(sdram_window);
            //print_buff(sdram_window+1024);
        }
    }
    ser_nl();
}

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <esp8266.h>
#include "spiffs.h"
#include "esp_spiffs.h"
#include "driver/spi.h"

#include "cgispiffs.h"
#include "io.h"

extern spiffs fs;

ICACHE_FLASH_ATTR static void assume_spi_master()
{
    // "minimal pins" is espspeak for leaving CS0 alone in master mode
//    spi_init(
//            /* SPI1 */ 1, 
//            /* clock rests at L, data stable on posedge */ SPI_MODE0, 
//            /* why not faster? */ SPI_FREQ_DIV_10M, 
//            /* bit order: LSB first */ 0, 
//            /* endian */ SPI_BIG_ENDIAN,
//            /* minimal pins */ 1);
//    spi_clear_dummy(1);
//    spi_clear_command(1);
//    spi_clear_address(1);
    spi_init(HSPI);
    spi_clock(HSPI, 4, 2); // 10M
    spi_mode(HSPI, 0, 0);
    spi_tx_bit_order(HSPI, 0);
}

ICACHE_FLASH_ATTR void spiffs_configure_fpga()
{
    char boot[128];

    assume_spi_master();
    spiffs_get_boot(boot, sizeof(boot));
    int rbf = SPIFFS_open(&fs, boot, SPIFFS_O_RDONLY, 0);
    if (rbf < 0) {
        printf("SPIFFS_open rbf: '%s' errno %d\n", boot, SPIFFS_errno(&fs));
        return;
    }
    printf("RBF: %s\n", boot);

    if (read_conf_done() == 1) {
        printf("CONF_DONE=1, pulling nCONFIG low to enter config mode\n");
    } else {
        printf("CONF_DONE=0, FPGA needs reset\n");
    }

    // will configure the fpga via passive serial
    // 1. pull nCONFIG low for Tcfg (500ns min)
    nconfig_set_input(0);
    nconfig_set(0);
    os_delay_us(1);
    nconfig_set(1);
    nconfig_set_input(1);

    if (read_conf_done() == 1) {
        printf("Unable to enter configuration mode. Abort.\n");
        SPIFFS_close(&fs, rbf);
        return;
    }

    // 2. wait for CONF_DONE to become low
    // 3. wait Tstatus+Tst2ck -- up to 230us
    os_delay_us(230);

    // 4. begin pumping bitstream, lsb first
    //   02 1B EE 01 FA->0100-0000 1101-1000 0111-0111 1000-0000 0101-1111

    uint32_t csum = 0, ci = 0;
    for(;;) {
        int len = SPIFFS_read(&fs, rbf, boot, sizeof(boot));
        if (len > 0) {
            //spi_transfer(1, /* out_data */ boot, /* in_data */ NULL, len, 
            //        SPI_8BIT);
            for (int i = 0; i < len; ++i) {
                csum = csum + (boot[i] ^ ci); ++ci;
                spi_tx8(HSPI, boot[i]);
            }
        } else {
            break;
        }
    }
    SPIFFS_close(&fs, rbf);
    printf("Checksum=%x\n", csum);

    // 5. CONF_DONE becomes high
    if (read_conf_done() == 0) {
        printf("CONF_DONE stays low. Something is wrong.\n");
        return;
    }
    // 6. pump 2x DCLK
    // (probably not a problem to pump 8
    //spi_transfer(1, boot, NULL, 1, SPI_8BIT);
    spi_tx8(HSPI, 0);
    // 7. wait Tcd2um (300us)
    os_delay_us(300);
    // 8. we're in user mode!
    printf("FPGA is configured and in user mode.\n");
}

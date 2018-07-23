#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <espressif/esp_common.h>
#include <libesphttpd/httpd.h>
#include <spiffs.h>
#include <esp_spiffs.h>

#include "cgispiffs.h"

void spiffs_configure_fpga()
{
    char boot[128];
    spiffs_get_boot(boot, sizeof(boot));
    if (boot[0] != 0) {
        // will configure the fpga via passive serial
        // 1. pull nCONFIG low for Tcfg (500ns min)
        // 2. wait for CONF_DONE to become low
        // 3. wait Tstatus+Tst2ck -- up to 230us
        // 4. begin pumping bitstream, lsb first
        //   02 1B EE 01 FA->0100-0000 1101-1000 0111-0111 1000-0000 0101-1111
        // 5. CONF_DONE becomes high
        // 6. pump 2x DCLK
        // 7. wait Tcd2um
        // 8. we're in user mode!
    }
}

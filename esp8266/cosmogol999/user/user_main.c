/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

/*
This is example code for the esphttpd library. It's a small-ish demo showing off 
the server, including WiFi connection management capabilities, some IO and
some pictures of cats.
*/

#include <string.h>
#include <stdio.h>

#include <esp8266.h>
#include <httpd.h>
#include <httpdespfs.h>
#include <cgiwifi.h>
#include <auth.h>
#include <espfs.h>
#include <captdns.h>
#include <webpages-espfs.h>
#include <cgiwebsocket.h>

#include "rboot-api.h"
#include "stdout.h"
#include "io.h"
#include "cgi.h"
#include "cgispiffs.h"
#include "cgirboot.h"

#include "uart.h"

#define AP_SSID "ЛЕШАДОК ПОМПЕ"
#define AP_PSK "pompaient"

HttpdBuiltInUrl builtInUrls[]={
	{"*", cgiRedirectApClientToHostname, "esp8266.nonet"},
	{"/", cgiRedirect, "/index.tpl"},
        {"/fpga/", cgiRedirect, "/fpga/index.tpl"},
        {"/fpga/index.tpl", cgiEspFsTemplate, tplFPGA},
        {"/fpga/upload", cgiUploadFile, NULL},
        {"/fpga/dir", cgiDirectory, NULL},
        {"/fpga/delete", cgiDelete, NULL},
        {"/fpga/bootname", cgiSelectBoot, NULL},

	{"/led.tpl", cgiEspFsTemplate, tplLed},
	{"/index.tpl", cgiEspFsTemplate, tplCounter},
	{"/led.cgi", cgiLed, NULL},

	{"/flash/", cgiRedirect, "/flash/index.html"},
	{"/flash/upload", cgiUploadFirmware2, NULL},
	{"/flash/reboot", cgiRebootFirmware2, NULL},

	{"/wifi", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/", cgiRedirect, "/wifi/wifi.tpl"},
	{"/wifi/wifiscan.cgi", cgiWiFiScan, NULL},
	{"/wifi/wifi.tpl", cgiEspFsTemplate, tplWlan},
	{"/wifi/connect.cgi", cgiWiFiConnect, NULL},
	{"/wifi/connstatus.cgi", cgiWiFiConnStatus, NULL},
	{"/wifi/setmode.cgi", cgiWiFiSetMode, NULL},

	{"*", cgiEspFsHook, NULL}, //Catch-all cgi function for the filesystem
	{NULL, NULL, NULL}
};

#if 1
void ICACHE_FLASH_ATTR wifiInit() {
    struct ip_info ap_ip;
    uint8_t wifi_get_opmode();
    system_phy_set_max_tpw(0);
    switch(wifi_get_opmode()) {
        case STATIONAP_MODE:
        case SOFTAP_MODE:
            IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
            IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
            IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
            wifi_set_ip_info(1, &ap_ip);

            struct softap_config ap_config = {
                .ssid = AP_SSID,
                .ssid_hidden = 0,
                .channel = 3,
                .ssid_len = strlen(AP_SSID),
                .authmode = AUTH_WPA_WPA2_PSK,
                .password = AP_PSK,
                .max_connection = 3,
                .beacon_interval = 250,
            };
            wifi_softap_set_config(&ap_config);

            //ip_addr_t first_client_ip;
            //IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
            //dhcpserver_start(&first_client_ip, 4);
            //dhcpserver_set_dns(&ap_ip.ip);
            //dhcpserver_set_router(&ap_ip.ip);
            break;
        case STATION_MODE:
            break;
        default:
            break;
    }
}
#endif

ICACHE_FLASH_ATTR void setup_rboot()
{
    printf("rboot_get_current_rom: %d\n", rboot_get_current_rom());
    rboot_config rconfig = rboot_get_config();
    printf("rboot magic=%x version=%d mode=%d current_rom=%d gpio_rom=%d "
            "count=%d\n", rconfig.magic, rconfig.version, rconfig.mode, 
            rconfig.current_rom, rconfig.gpio_rom, rconfig.count);
    for (int i = 0; i < MAX_ROMS; ++i) {
        printf("%c @%08x\n", (i==rconfig.current_rom)?'*':' ', rconfig.roms[i]);
    }

    if (rconfig.roms[1] != 0x102000) {
        printf("rboot config is probably correct for someone else. Fixing it.\n");
        rconfig.roms[1] = 0x102000;
        rboot_set_config(&rconfig);
    }
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
ICACHE_FLASH_ATTR void user_init(void) {
    stdoutInit();
    printf("\nbloody hell\n");
    wifiInit();

    ioInit();
    spiffs_init();

    spiffs_configure_fpga();
    io_shutdown_spi();

    setup_rboot();
    captdnsInit();
    espFsInit((void*)(webpages_espfs_start));
    httpdInit(builtInUrls, 80);

    wifi_set_sleep_type(MODEM_SLEEP_T);

    //sdk_system_deep_sleep(1000); -- what to do after deep sleep? 

    printf("\nuser_init() done\n");
}


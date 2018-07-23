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

#include <espressif/esp_common.h>
#include <etstimer.h>
#include <libesphttpd/httpd.h>
#include <libesphttpd/httpdespfs.h>
#include <libesphttpd/cgiwifi.h>
#include <libesphttpd/cgiflash.h>
#include <libesphttpd/auth.h>
#include <libesphttpd/espfs.h>
#include <libesphttpd/captdns.h>
#include <libesphttpd/webpages-espfs.h>
#include <libesphttpd/cgiwebsocket.h>
#include <dhcpserver.h>

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>
#include <esp/uart.h>

#include "io.h"
#include "cgi.h"
#include "cgispiffs.h"

#define AP_SSID "Вектор-06Ц SoftAP"
#define AP_PSK "pompaient"

CgiUploadFlashDef uploadParams={
	.type=CGIFLASH_TYPE_FW,
	.fw1Pos=0x2000,
	.fw2Pos=((FLASH_SIZE*1024*1024)/2)+0x2000,
	.fwSize=((FLASH_SIZE*1024*1024)/2)-0x2000,
	.tagName=LIBESPHTTPD_OTA_TAGNAME
};


/*
This is the main url->function dispatching data struct.
In short, it's a struct with various URLs plus their handlers. The handlers can
be 'standard' CGI functions you wrote, or 'special' CGIs requiring an argument.
They can also be auth-functions. An asterisk will match any url starting with
everything before the asterisks; "*" matches everything. The list will be
handled top-down, so make sure to put more specific rules above the more
general ones. Authorization things (like authBasic) act as a 'barrier' and
should be placed above the URLs they protect.
*/
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
	{"/flash/next", cgiGetFirmwareNext, &uploadParams},
	{"/flash/upload", cgiUploadFirmware, &uploadParams},
	{"/flash/reboot", cgiRebootFirmware, NULL},

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

void wifiInit() {
    struct ip_info ap_ip;
    uint8_t sdk_wifi_get_opmode();
    switch(sdk_wifi_get_opmode()) {
        case STATIONAP_MODE:
        case SOFTAP_MODE:
            IP4_ADDR(&ap_ip.ip, 172, 16, 0, 1);
            IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
            IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);
            sdk_wifi_set_ip_info(1, &ap_ip);

            struct sdk_softap_config ap_config = {
                .ssid = AP_SSID,
                .ssid_hidden = 0,
                .channel = 3,
                .ssid_len = strlen(AP_SSID),
                .authmode = AUTH_WPA_WPA2_PSK,
                .password = AP_PSK,
                .max_connection = 3,
                .beacon_interval = 100,
            };
            sdk_wifi_softap_set_config(&ap_config);

            ip_addr_t first_client_ip;
            IP4_ADDR(&first_client_ip, 172, 16, 0, 2);
            dhcpserver_start(&first_client_ip, 4);
            dhcpserver_set_dns(&ap_ip.ip);
            dhcpserver_set_router(&ap_ip.ip);
            break;
        case STATION_MODE:
            break;
        default:
            break;
    }
}

//Main routine. Initialize stdout, the I/O, filesystem and the webserver and we're done.
void user_init(void) {
    uart_set_baud(0, 115200);

    ioInit();
    spiffs_init();
    spiffs_configure_fpga();

    wifiInit();
    captdnsInit();

    espFsInit((void*)(_binary_build_web_espfs_bin_start));
    httpdInit(builtInUrls, 80);

    printf("\nReady\n");
}

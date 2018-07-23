#pragma once

#include <libesphttpd/httpd.h>

void spiffs_init();
int tplFPGA(HttpdConnData *connData, char * token, void ** arg);
int cgiUploadFile(HttpdConnData *connData);
int cgiDirectory(HttpdConnData *connData);
int cgiDelete(HttpdConnData *connData);
int cgiSelectBoot(HttpdConnData *connData);


void spiffs_get_boot(char * name, int len);
void spiffs_configure_fpga();

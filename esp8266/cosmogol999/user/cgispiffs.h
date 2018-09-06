#pragma once

#include <httpd.h>

void spiffs_init();
int tplFPGA(HttpdConnData *connData, char * token, void ** arg);
int cgiUploadFile(HttpdConnData *connData);
int cgiDirectory(HttpdConnData *connData);
int cgiDelete(HttpdConnData *connData);
int cgiSelectBoot(HttpdConnData *connData);

void spiffs_configure_fpga();

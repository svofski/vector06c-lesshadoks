#include <stdint.h>
#include <esp8266.h>
#include "httpd.h"

int ICACHE_FLASH_ATTR cgiFloppyCatalog(HttpdConnData *connData);
int ICACHE_FLASH_ATTR cgiFloppySelect(HttpdConnData *connData);

#ifndef IO_H
#define IO_H

void ICACHE_FLASH_ATTR ioLed(int ena);
void ioInit(void);
int read_conf_done();
void nconfig_set(int value);
void nconfig_set_input(int is_input);
void io_shutdown_spi();
#endif

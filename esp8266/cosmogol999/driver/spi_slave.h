#ifndef SPI_APP_H
#define SPI_APP_H

#include "esp8266.h"

void spi_slave_init(uint8 spi_no, uint8 data_len);
void spi_slave_deinit(uint8 spi_no, uint8 data_len);

extern void (*hspi_rxdata_cb)(void * arg, uint8_t * data, uint8_t len);
extern void (*hspi_txdata_cb)(void * arg);
extern void (*hspi_rxstatus_cb)(void * arg, uint32_t data);
extern void (*hspi_txstatus_cb)(void * arg);

#endif


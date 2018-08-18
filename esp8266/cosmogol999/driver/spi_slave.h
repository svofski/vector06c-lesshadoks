#ifndef SPI_APP_H
#define SPI_APP_H

#include "esp8266.h"

void spi_slave_init(uint8_t data_len, void * context);
void spi_slave_deinit();
void spi_slave_set_data(uint8_t * data, uint8_t len);
void spi_slave_set_status(uint32_t status);

extern void (*hspi_rxdata_cb)(void * arg, uint8_t * data, uint8_t len);
extern void (*hspi_txdata_cb)(void * arg);
extern void (*hspi_rxstatus_cb)(void * arg, uint32_t data);
extern void (*hspi_txstatus_cb)(void * arg);

#endif


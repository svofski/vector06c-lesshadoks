#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#define TESTBENCH

#define ICACHE_FLASH_ATTR

#define s32_t   int32_t
#define u8_t    uint8_t

typedef enum {
    SPI_FLASH_RESULT_OK,
    SPI_FLASH_RESULT_ERR,
    SPI_FLASH_RESULT_TIMEOUT,
} SpiFlashOpResult;

SpiFlashOpResult spi_flash_read(uint32_t addr, uint32_t * dst, size_t sz)
{
    printf("RD @%06x sz=%zu: ", addr, sz);

    uint8_t * d8 = (uint8_t *)dst;
    for (int i = 0; i < sz; ++i) {
        uint8_t value = (addr & 0xff) + i + 1;
        d8[i] = value;
        printf(" %02x", value);
    }
    printf("\n");
    return SPI_FLASH_RESULT_OK;
}

SpiFlashOpResult spi_flash_write(uint32_t addr, uint32_t * src, size_t sz)
{
    printf("WR @%06x: ", addr);
    for (int i = 0; i < sz; ++i) printf(" %02x", ((uint8_t *)src)[i]);
    printf("\n");
    return SPI_FLASH_RESULT_OK;
}

#define WRBUF_SIZE 8
#include "spiffs/esp_spiffs.c"

void test1()
{
    uint8_t data[] = {1,2,3,4};
    spi_align_write(0x1000, &data[0], 2);
    spi_align_write(0x1001, &data[0], 2);
    spi_align_write(0x1002, &data[0], 2);
    spi_align_write(0x1003, &data[0], 2);
    spi_align_write(0x1004, &data[0], 2);
    spi_align_write(0x1005, &data[0], 2);
    printf("\n");
}

void test2() {
    uint8_t data[] = {1,2,3,4};
    spi_align_write(0x1000, &data[0], 4);
    spi_align_write(0x1001, &data[0], 4);
    spi_align_write(0x1002, &data[0], 4);
    spi_align_write(0x1003, &data[0], 4);
    spi_align_write(0x1004, &data[0], 4);
    spi_align_write(0x1005, &data[0], 4);
    printf("\n");
}

void test3() {
    uint8_t data[] = {1,2,3,4,5,6,7,8};
    spi_align_write(0x1000, &data[0], 8);
    spi_align_write(0x1001, &data[0], 8);
    spi_align_write(0x1002, &data[0], 8);
    spi_align_write(0x1003, &data[0], 8);
    spi_align_write(0x1004, &data[0], 8);
    spi_align_write(0x1005, &data[0], 8);
    printf("\n");
}

void test4() {
    uint8_t data[33];
    for (int i = 0; i < sizeof(data); ++i) data[i] = i + 1;
    spi_align_write(0x1001, &data[0], sizeof(data));
}

void dump_data(uint8_t * data, size_t s)
{
    printf("DATA: ");
    for(int i = 0; i < s; ++i) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

void readrun(int ofs, int len)
{
    uint8_t data[32];

    memset(data, 255, sizeof(data));
    spi_align_read(0x1000 + ofs, &data[0], len);
    dump_data(data, 32);

}

void rest1() {
    printf("\n");
    readrun(1, 17);
//    readrun(1, 4);
//    readrun(2, 4);
}

int main()
{
    printf("testbench for esp_spiffs.c hal routines\n");
    test1();
    test2();
    test3();
    test4();

    rest1();
}



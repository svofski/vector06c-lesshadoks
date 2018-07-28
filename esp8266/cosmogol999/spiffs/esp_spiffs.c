/**
 * ESP8266 SPIFFS HAL configuration.
 *
 * Part of esp-open-rtos
 * Copyright (c) 2016 sheinz https://github.com/sheinz
 * MIT License
 */
#ifndef TESTBENCH
#include <esp8266.h>
#include "esp_spiffs.h"
#include "spiffs.h"
#include <spi_flash.h>
#include <stdbool.h>
//#include <esp/uart.h>
#include <fcntl.h>
#endif

#ifndef SPI_FLASH_SECTOR_SIZE
#define SPI_FLASH_SECTOR_SIZE 4096
#endif

#ifndef TESTBENCH
spiffs fs;

typedef struct {
    void *buf;
    uint32_t size;
} fs_buf_t;

static spiffs_config config = {0};
static fs_buf_t work_buf = {0};
static fs_buf_t fds_buf = {0};
static fs_buf_t cache_buf = {0};
#endif

static SpiFlashOpResult 
spi_align_write(uint32_t addr, const uint8_t * src, size_t size);

static SpiFlashOpResult 
spi_align_read(uint32_t addr, uint8_t * dst, size_t size);

/**
 * Number of file descriptors opened at the same time
 */
#define ESP_SPIFFS_FD_NUMBER       5

#define ESP_SPIFFS_CACHE_PAGES     5

#ifndef TESTBENCH

static ICACHE_FLASH_ATTR s32_t esp_spiffs_read(u32_t addr, u32_t size, u8_t *dst)
{
    //printf("esp_spiffs_read(%x,%x)", addr, size);
    if (spi_align_read(addr, dst, size) != SPI_FLASH_RESULT_OK) {
        //printf("ERROR\n");
        return SPIFFS_ERR_INTERNAL;
    }
    //for (int i = 0; i < size; ++i) {
    //    printf(" %02x", dst[i]);
    //}
    //printf(" OK\n");

    return SPIFFS_OK;
}

static ICACHE_FLASH_ATTR s32_t esp_spiffs_write(u32_t addr, u32_t size, u8_t *src)
{
    //printf("esp_spiffs_write(%x,%x)", addr, size);
    if (spi_align_write(addr, src, size) != SPI_FLASH_RESULT_OK) {
        //printf("ERROR\n");
        return SPIFFS_ERR_INTERNAL;
    }

    //for (int i = 0; i < size; ++i) {
    //    printf(" %02x", src[i]);
    //}
    //printf("OK\n");
    return SPIFFS_OK;
}

static ICACHE_FLASH_ATTR s32_t esp_spiffs_erase(u32_t addr, u32_t size)
{
    uint32_t sectors = size / SPI_FLASH_SECTOR_SIZE;

    //printf("esp_spiffs_erase(%x,%x)", addr, size);

    for (uint32_t i = 0, sector = addr/SPI_FLASH_SECTOR_SIZE; i < sectors; i++) {
        //printf("sector %d\n", sector);
        if (spi_flash_erase_sector(sector) != SPI_FLASH_RESULT_OK) {
            //printf("eggog\n");
            return SPIFFS_ERR_INTERNAL;
        }
    }

    return SPIFFS_OK;
}

#if SPIFFS_SINGLETON == 1
void ICACHE_FLASH_ATTR esp_spiffs_init()
{
#else
void ICACHE_FLASH_ATTR esp_spiffs_init(uint32_t addr, uint32_t size)
{
    config.phys_addr = addr;
    config.phys_size = size;

    config.phys_erase_block = SPIFFS_ESP_ERASE_SIZE;
    config.log_page_size = SPIFFS_LOG_PAGE_SIZE;
    config.log_block_size = SPIFFS_LOG_BLOCK_SIZE;

    // Initialize fs.cfg so the following helper functions work correctly
    memcpy(&fs.cfg, &config, sizeof(spiffs_config));
#endif
    work_buf.size = 2 * SPIFFS_LOG_PAGE_SIZE;
    fds_buf.size = SPIFFS_buffer_bytes_for_filedescs(&fs, ESP_SPIFFS_FD_NUMBER);
    cache_buf.size= SPIFFS_buffer_bytes_for_cache(&fs, ESP_SPIFFS_CACHE_PAGES);

    work_buf.buf = malloc(work_buf.size);
    fds_buf.buf = malloc(fds_buf.size);
    cache_buf.buf = malloc(cache_buf.size);

    config.hal_read_f = esp_spiffs_read;
    config.hal_write_f = esp_spiffs_write;
    config.hal_erase_f = esp_spiffs_erase;

    config.fh_ix_offset = SPIFFS_FILEHDL_OFFSET;
}

void ICACHE_FLASH_ATTR esp_spiffs_deinit()
{
    free(work_buf.buf);
    work_buf.buf = 0;

    free(fds_buf.buf);
    fds_buf.buf = 0;

    free(cache_buf.buf);
    cache_buf.buf = 0;
}

int32_t ICACHE_FLASH_ATTR esp_spiffs_mount()
{
    printf("SPIFFS memory, work_buf_size=%d, fds_buf_size=%d, cache_buf_size=%d\n",
            work_buf.size, fds_buf.size, cache_buf.size);

    int32_t err = SPIFFS_mount(&fs, &config, (uint8_t*)work_buf.buf,
            (uint8_t*)fds_buf.buf, fds_buf.size,
            cache_buf.buf, cache_buf.size, 0);

    if (err != SPIFFS_OK) {
        printf("Error mounting SPIFFS: %d\n", err);
    }

    return err;
}

// This implementation replaces implementation in core/newlib_syscals.c
long ICACHE_FLASH_ATTR _write_filesystem_r(struct _reent *r, int fd, const char *ptr, int len )
{
    return SPIFFS_write(&fs, (spiffs_file)fd, (char*)ptr, len);
}

// This implementation replaces implementation in core/newlib_syscals.c
long ICACHE_FLASH_ATTR _read_filesystem_r( struct _reent *r, int fd, char *ptr, int len )
{
    return SPIFFS_read(&fs, (spiffs_file)fd, ptr, len);
}

int ICACHE_FLASH_ATTR _open_r(struct _reent *r, const char *pathname, int flags, int mode)
{
    uint32_t spiffs_flags = SPIFFS_RDONLY;

    if (flags & O_CREAT)    spiffs_flags |= SPIFFS_CREAT;
    if (flags & O_APPEND)   spiffs_flags |= SPIFFS_APPEND;
    if (flags & O_TRUNC)    spiffs_flags |= SPIFFS_TRUNC;
    if (flags & O_RDONLY)   spiffs_flags |= SPIFFS_RDONLY;
    if (flags & O_WRONLY)   spiffs_flags |= SPIFFS_WRONLY;
    if (flags & O_EXCL)     spiffs_flags |= SPIFFS_EXCL;
    /* if (flags & O_DIRECT)   spiffs_flags |= SPIFFS_DIRECT; no support in newlib */

    return SPIFFS_open(&fs, pathname, spiffs_flags, mode);
}

// This implementation replaces implementation in core/newlib_syscals.c
int ICACHE_FLASH_ATTR _close_filesystem_r(struct _reent *r, int fd)
{
    return SPIFFS_close(&fs, (spiffs_file)fd);
}

int ICACHE_FLASH_ATTR _unlink_r(struct _reent *r, const char *path)
{
    return SPIFFS_remove(&fs, path);
}

int ICACHE_FLASH_ATTR _fstat_r(struct _reent *r, int fd, void *buf)
{
    spiffs_stat s;
    struct stat *sb = (struct stat*)buf;

    int result = SPIFFS_fstat(&fs, (spiffs_file)fd, &s);
    sb->st_size = s.size;

    return result;
}

int ICACHE_FLASH_ATTR _stat_r(struct _reent *r, const char *pathname, void *buf)
{
    spiffs_stat s;
    struct stat *sb = (struct stat*)buf;

    int result = SPIFFS_stat(&fs, pathname, &s);
    sb->st_size = s.size;

    return result;
}

off_t ICACHE_FLASH_ATTR _lseek_r(struct _reent *r, int fd, off_t offset, int whence)
{
    return SPIFFS_lseek(&fs, (spiffs_file)fd, offset, whence);
}

#endif

ICACHE_FLASH_ATTR
SpiFlashOpResult spi_align_read(uint32_t addr, uint8_t * dst, size_t size)
{
    SpiFlashOpResult res;
    if (size > (1<<31)) return SPI_FLASH_RESULT_ERR;
    if (size == 0) return SPI_FLASH_RESULT_OK;

    uint32_t begin_aligned = addr & ~3;
    uint32_t begin_misalign = addr & 3;
    int count = (int32_t)size;
    int rc = 0;
    if (begin_misalign) {
        uint8_t a[4] __attribute__((aligned(4)));
        res = spi_flash_read(begin_aligned, (uint32_t *)&a[0], 4);
        if (res != SPI_FLASH_RESULT_OK) return res;
        begin_aligned += 4;
        rc = 4 - begin_misalign;
        if (rc > size) rc = size;
        memcpy(dst, &a[begin_misalign], rc);
        count -= rc;
    }
    if (count == 0) return SPI_FLASH_RESULT_OK;


    uint32_t count_aligned = count & ~3;
    uint32_t count_misalign = count & 3;
    uint32_t * aligned_chunk = (uint32_t *)(dst + rc);
    if (count_aligned) {
        res = spi_flash_read(begin_aligned, aligned_chunk, count_aligned);
        if (res != SPI_FLASH_RESULT_OK) return res;
        begin_aligned += count_aligned;
        rc += count_aligned;
    }

    if (count_misalign) {
        uint8_t a[4] __attribute__((aligned(4)));
        res = spi_flash_read(begin_aligned, (uint32_t *)&a[0], 4);
        if (res != SPI_FLASH_RESULT_OK) return res;
        memcpy(dst + rc, a, count_misalign);
    }
    return SPI_FLASH_RESULT_OK;
}

#ifndef WRBUF_SIZE
#define WRBUF_SIZE 512
#endif

ICACHE_FLASH_ATTR
SpiFlashOpResult spi_align_write(uint32_t addr, const uint8_t * src, size_t size)
{
    SpiFlashOpResult res = SPI_FLASH_RESULT_OK;

    if (size > (1<<31)) return SPI_FLASH_RESULT_ERR;
    if (size == 0) return SPI_FLASH_RESULT_OK;

    uint32_t begin_aligned = addr & ~3;
    uint32_t begin_misalign = addr & 3;
    int32_t count = (int32_t)size;

    int wc = 0;
    if (begin_misalign) {
        uint8_t a[4] __attribute__ ((aligned(4))) = {0377,0377,0377,0377};
        wc = (4 - begin_misalign) > size ? size : 4 - begin_misalign;
        memcpy(&a[begin_misalign], src, wc);
        res = spi_flash_write(begin_aligned, (uint32_t *)a, 4);
        if (res != SPI_FLASH_RESULT_OK) return res;
        begin_aligned += 4;
        count -= wc;
    }

    if (count <= 0) return SPI_FLASH_RESULT_OK;

    uint32_t * middle = (uint32_t *)(src + wc);
    uint32_t remains = count & ~3;
    uint32_t end_misalign = count & 3;

    for (;remains;) {
        uint8_t buf[WRBUF_SIZE] __attribute__((aligned(4)));
        int chunk_len = remains > sizeof(buf) ? sizeof(buf) : remains;
        memcpy(buf, middle, chunk_len);  
        res = spi_flash_write(begin_aligned, (uint32_t *)&buf[0], chunk_len);
        if (res != SPI_FLASH_RESULT_OK) return res;
        middle += chunk_len >> 2;
        remains -= chunk_len;
        begin_aligned += chunk_len;
    }

    if (end_misalign) {
        uint8_t a[4] __attribute__ ((aligned(4))) = {0377,0377,0377,0377};
        memcpy(a, middle, end_misalign);
        res = spi_flash_write(begin_aligned, (uint32_t *)a, 4);
    }

    return res;
}

// FPGA SoC's thrall

#include "esp8266.h"
#include "driver/spi_slave.h"

#include "slave.h"

uint8_t txbuf[32];
uint8_t rxbuf[32];

volatile uint8_t slave_state;

master_status_t master_status;

void rxdata_kolbask(void * context, uint8_t * buffer, uint8_t len);

void ICACHE_FLASH_ATTR
slave_init()
{
    hspi_rxdata_cb = rxdata_kolbask;
    slave_ready(0);
    slave_setstate(SLAVE_VOID);

    spi_slave_init(32, NULL);
}

void ICACHE_FLASH_ATTR
dump_rxdata()
{
    printf("rxdata:");
    for(int i = 0; i < 16; ++i) {
        printf("%02x ", rxbuf[i]);
    }
    printf("\n");
}

//void ICACHE_FLASH_ATTR
void rxdata_kolbask(void * context, uint8_t * buffer, uint8_t len)
{
    if (len > 32) len = 32;
    memcpy(rxbuf, buffer, len);

    switch (buffer[0]) {
        case MTS_POLL_USER_COMMAND:
            master_status = MSTAT_POLL;
            break;
        case MTS_GET_VALUE:
            // ?
            break;
        case MTS_REPLY_MORE:
            master_status = MSTAT_MORE;
            if (slave_state == SLAVE_VOID) {
                // post dummy response
                slave_ready(rxbuf[2]);
                dump_rxdata();
                printf("void->dummy\n");
            }
            break;
        case MTS_REPLY_END:
            master_status = MSTAT_END;
            if (slave_state == SLAVE_VOID) {
                slave_ready(rxbuf[2]);
                dump_rxdata();
                printf("void->dummy\n");
            }
            break;
        default:
            break;
    }
//    os_delay_us(140);
}

// put user req in mailbox
void ICACHE_FLASH_ATTR
slave_post_user_command(stm_t cmd, const uint8_t * data, uint8_t len)
{
    if (len > 30) len = 30;

    txbuf[0] = (uint8_t)cmd;
    txbuf[1] = (uint8_t)len;
    memset(txbuf+2, (uint8_t)0, (size_t)30);
    if (data != NULL) {
        memcpy(txbuf + 2, data, len);
    } 
    spi_slave_set_data((uint8_t *)txbuf, len + 2);
}

master_status_t ICACHE_FLASH_ATTR
slave_poll_response(uint8_t ** data, uint8_t * len, uint8_t * token)
{
    master_status_t ms = master_status;
    master_status = MSTAT_NONE;

    if (ms == MSTAT_MORE || ms == MSTAT_END) {
        *data = rxbuf+3;
        *token = rxbuf[2];
        *len = rxbuf[1];
    } else {
        *data = NULL;
        *len = 0;
        *token = 0;
    }

    return ms;
}

void ICACHE_FLASH_ATTR slave_ready(uint8_t token)
{
    uint8_t seq[] = {STM_READY, token};
    spi_slave_set_data(seq, 2);
}

void ICACHE_FLASH_ATTR slave_setstate(uint8_t state)
{
    slave_state = state;
}

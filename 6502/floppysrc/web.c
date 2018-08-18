#include "integer.h"
#include "menu.h"
#include "philes.h"
#include "serial.h"
#include <string.h>

typedef enum _master_to_slave {
    /* a dummy packet to get response data */
    CMD_DUMMY = 0,
    /* SoC asks if user sends a command */
    MTS_POLL_USER_COMMAND = 1, // cmd8, xxx expect answer in the next cmd
    MTS_REPLY_MORE = 2,
    MTS_REPLY_END = 3,
} mts_t;

typedef enum _slave_to_master {
    STM_NONE = 0,
    STM_CATALOG = 6,
    STM_SELECT = 7,
    STM_WAIT = 32,
    STM_READY = 33,
} stm_t;

typedef enum _mastercall {
    MSTAT_NONE = 0,
    MSTAT_MORE = 1,     // expect more data
    MSTAT_END = 2,      // data end
} master_status_t;

#define loop_until_bit_set(x,b)         {for(;((x)&(b))!=0;);}
#define rcvr_spi_m(dst) {SPDR=0xFF; loop_until_bit_set(SPSR,SPIF); *(dst)=SPDR;}
#define xmit_spi(dat)   SPDR=(dat); loop_until_bit_set(SPSR,SPIF)

static uint8_t ready_token;

static char txbuf[32];
static char rxbuf[32];

static int16_t drvsel[3] = {0,-1,-1};

static void send_buf(const uint8_t * data);
static void recv_buf(uint8_t * data);
static void handle_request(void);
static void do_catalog(void);
static void do_select(void);
static uint8_t next_token(void);
static int8_t wait_token(uint8_t token);
static int16_t findfileidx(const char * name);
static void solo(uint8_t prio, uint8_t a, uint8_t b);

void menu_init()
{
}

uint8_t menu_dispatch(uint8_t tick)
{
    txbuf[0] = MTS_POLL_USER_COMMAND;
    send_buf(txbuf);
    recv_buf(rxbuf);
    handle_request();

    return MENURESULT_NOTHING;
}

uint8_t menu_busy(uint8_t yes)
{
    return 0;
}

void handle_request()
{
    switch (rxbuf[0]) {
        case STM_CATALOG:
            do_catalog();
            return;
        case STM_SELECT:
            do_select();
            return;
        default:
            break;
    }
}

char sel(int16_t i) {
    if (i == drvsel[0]) {
        return 'A';
    } else if (i == drvsel[1]) {
        return 'B';
    } else if (i == drvsel[2]) {
        return 'R';
    }
    return '_';
}

void do_catalog()
{
    FRESULT res;
    int16_t i;

    ser_puts("web do_catalog(): opendir");
    if (philes_opendir() == FR_OK) {
        ser_puts(" ok"); ser_nl();

        for (i = 0; (res = philes_nextfile(&txbuf[4], 1)) == FR_OK; ++i) {
            do {
                txbuf[3] = sel(i); // mark selection
                txbuf[2] = next_token(); // accept token
                txbuf[1] = strlen(&txbuf[3]); // data size
                txbuf[0] = MSTAT_MORE; // always more

                send_buf(txbuf);
            } while(wait_token(ready_token) == -1); // resend until ok
        }
        txbuf[0] = MSTAT_END;
        txbuf[1] = 0;
        txbuf[2] = next_token();
        send_buf(txbuf);
        wait_token(ready_token);
    }
    ser_puts("do_catalog() done");
}

void do_select()
{
    ser_puts("web do_select():");
    ser_puts(&rxbuf[2]);
    switch (rxbuf[2]) {
        case 'A':   drvsel[0] = findfileidx(rxbuf+3); solo(0, 1, 2); break;
        case 'B':   drvsel[1] = findfileidx(rxbuf+3); solo(1, 0, 2); break;
        case 'R':   drvsel[2] = findfileidx(rxbuf+3); solo(2, 0, 1); break;
    }
    txbuf[0] = MSTAT_END;
    txbuf[1] = 0;
    send_buf(txbuf);
}

int16_t findfileidx(const char * name)
{
    int16_t i;
    if (philes_opendir() == FR_OK) {
        for (i = 0; philes_nextfile(&txbuf[4], 1) == FR_OK; ++i) {
            if (strcmp(&txbuf[4], name) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void solo(uint8_t prio, uint8_t a, uint8_t b)
{
    if (drvsel[a] == drvsel[prio]) drvsel[a] = -1;
    if (drvsel[b] == drvsel[prio]) drvsel[b] = -1;
}

void send_buf(const uint8_t * data)
{
    uint8_t i;

    SELECT_ESP();
    xmit_spi(0x02);
    xmit_spi(0x00);
    for (i = 0; i < 32; ++i) {
        xmit_spi(data[i]);
    }
    DESELECT_ESP();
}

void recv_buf(uint8_t * data)
{
    uint8_t i;

    SELECT_ESP();
    xmit_spi(0x03);   // data transfer from slave
    xmit_spi(0x00);
    for (i = 0; i < 32; ++i) {
        rcvr_spi_m(&data[i]);
    }
    DESELECT_ESP();
}

uint8_t next_token()
{
    uint8_t n = ready_token + 1;
    if (n == 0) n = 1;
    ready_token = n;
    return ready_token;
}

int8_t wait_token(uint8_t token)
{
    do {
        recv_buf(rxbuf);
        if (rxbuf[0] == STM_READY && rxbuf[1] == 0) return -1;
    } while(! (rxbuf[0] == STM_READY && rxbuf[1] == token));
    return 0;
}

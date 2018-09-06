#pragma once

#include <stdint.h>

typedef enum _master_to_slave {
    /* a dummy packet to get response data */
    CMD_DUMMY = 0,
    /* SoC asks if user sends a command */
    MTS_POLL_USER_COMMAND = 1, // cmd8, xxx expect answer in the next cmd
    MTS_REPLY_MORE = 2,
    MTS_REPLY_END = 3,
    MTS_GET_VALUE = 4,
} mts_t;

typedef enum _slave_to_master {
    STM_NONE = 0,
    STM_CATALOG = 6,        // [code] [sub: 0=fdd,1=edd,2=rom,3=boot]
    STM_SELECT = 7,
    STM_WAIT = 32,
    STM_READY = 33,
    STM_DATA = 34,
} stm_t;

typedef enum _mastercall {
    MSTAT_NONE = 0,
    MSTAT_MORE = 1,     // expect more data
    MSTAT_END = 2,      // data end
    MSTAT_POLL = 3,
} master_status_t;

#define SLAVE_VOID      0   // not ready to accept a response
#define SLAVE_CATALOG   1
#define SLAVE_SELECT    2

typedef void (*slave_rxdata_cb_t)(uint8_t *, size_t, void *);

void slave_init(void);
void slave_post_user_command(stm_t cmd, const uint8_t * data, uint8_t len);
master_status_t slave_poll_response(uint8_t ** data, uint8_t * len, uint8_t *tk);

void slave_ready(uint8_t token);
void slave_setstate(uint8_t state);

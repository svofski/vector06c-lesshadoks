#include "integer.h"
#include "specialio.h"
#include "serial.h"

#define SLAVE_CONTROL_BYTE  0x40

#define loop_until_bit_set(x,b)         {for(;((x)&(b))!=0;);}
#define rcvr_spi_m(dst) {SPDR=0xFF; loop_until_bit_set(SPSR,SPIF); *(dst)=SPDR;}
#define xmit_spi(dat)   SPDR=(dat); loop_until_bit_set(SPSR,SPIF)

#define    IOCON     (0x0A)
#define    IODIRA    (0x00)
#define    IODIRB    (0x01)
#define    GPIOA     (0x12)
#define    GPIOB     (0x13)

#define    GPINTENA  (0x04)
#define    DEFVALA   (0x06)
#define    INTCONA   (0x08)

static uint8_t readByte(uint8_t reg);
static void writeByte(uint8_t reg, uint8_t data);

void joy_init()
{
    DESELECT_JOY();

    // enable hardware address pins; bank=0 addressing, sequential on
    writeByte(IOCON,  0x08); 
    // all inputs
    writeByte(IODIRA, 0xFF);
    writeByte(IODIRB, 0xFF);
}

void joy_tick(uint8_t tick)
{
    uint8_t player1_bits;
    uint8_t player2_bits;

    SELECT_JOY();

    player1_bits = readByte(GPIOA);
    player2_bits = readByte(GPIOB);
    //ser_puts("JOY: "); print_hex(player1_bits); print_hex(player2_bits); ser_nl();

    PLAYER1 = player1_bits;
    PLAYER2 = player2_bits;
    DESELECT_JOY();
}


/*
 * Command: write a single byte to the specified register
 */ 
void writeByte(uint8_t reg, uint8_t data) {
    SELECT_JOY();

    xmit_spi(SLAVE_CONTROL_BYTE);
    xmit_spi(reg);
    xmit_spi(data);

    DESELECT_JOY();
}


/*
 * Returns: byte read from specified register
 */
uint8_t readByte(uint8_t reg) {
    uint8_t data;

    SELECT_JOY();

    xmit_spi(SLAVE_CONTROL_BYTE | 1);
    xmit_spi(reg);
    rcvr_spi_m(&data);

    DESELECT_JOY();

    return data;
}

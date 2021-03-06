#include "esp8266.h"
#include "driver/spi_slave.h"
#include "driver/spi_register.h"

#define SPI 0
#define HSPI 1

void (*hspi_rxdata_cb)(void * arg, uint8_t * data, uint8_t len) = NULL;
void (*hspi_txdata_cb)(void * arg) = NULL;
void (*hspi_rxstatus_cb)(void * arg, uint32_t data) = NULL;
void (*hspi_txstatus_cb)(void * arg) = NULL;

static uint8_t buffer[32];
static uint8_t txbuf[32];

void spi_slave_isr_handler(void *para);

void ICACHE_FLASH_ATTR
spi_slave_init(uint8_t data_len, void * context)
{
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);//configure io to spi mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);//configure io to spi mode    
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);//configure io to spi mode    
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);//configure io to spi mode    

    WRITE_PERI_REG(SPI_SLAVE(HSPI), 
            SPI_SLAVE_MODE|SPI_SLV_WR_RD_BUF_EN|
            SPI_SLV_WR_BUF_DONE_EN|SPI_SLV_RD_BUF_DONE_EN|
            SPI_SLV_WR_STA_DONE_EN|SPI_SLV_RD_STA_DONE_EN|
            SPI_TRANS_DONE_EN);

    WRITE_PERI_REG(SPI_USER(HSPI),
            SPI_USR_COMMAND|        // Command phase
            SPI_USR_MISO_HIGHPART|  // MISO phase uses W8-W15
            SPI_CK_I_EDGE);         // data on rising edge of clk

    //SET_PERI_REG_MASK(SPI_CTRL2(HSPI), 
    //        (0x2&SPI_MOSI_DELAY_NUM)<<SPI_MOSI_DELAY_NUM_S);

    os_printf("SPI_CTRL2 is %08x\n",READ_PERI_REG(SPI_CTRL2(HSPI)));

    WRITE_PERI_REG(SPI_CLOCK(HSPI), 0);

    // (SPILCOMMAND), seems to be command length = 8-1
    //SET_PERI_REG_BITS(SPI_USER2(HSPI), SPI_USR_COMMAND_BITLEN,
    //        7, SPI_USR_COMMAND_BITLEN_S);
    WRITE_PERI_REG(SPI_USER2(HSPI), 7<<SPI_USR_COMMAND_BITLEN_S);

    WRITE_PERI_REG(SPI_SLAVE1(HSPI), 0);
    // (SPIS1LWBA) address is 8-1 bits
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_WR_ADDR_BITLEN,
            7, SPI_SLV_WR_ADDR_BITLEN_S);

    // (SPIS1LRBA) address is 8-1 bits
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_RD_ADDR_BITLEN,
            7, SPI_SLV_RD_ADDR_BITLEN_S);

    // slave buffer bits: 255 (SPIS1LBUF)
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_BUF_BITLEN,
            255, SPI_SLV_BUF_BITLEN_S);

    // status length: 32-1 (SPIS1LSTA in arduino)
    SET_PERI_REG_BITS(SPI_SLAVE1(HSPI), SPI_SLV_STATUS_BITLEN,
                                      31, SPI_SLV_STATUS_BITLEN_S);

    //SET_PERI_REG_MASK(SPI_PIN(HSPI),BIT19);//BIT19 mystery bit
    WRITE_PERI_REG(SPI_PIN(HSPI),BIT19);//BIT19 mystery bit

    // "maybe enable slave transmission liston" (sic!)
    //SET_PERI_REG_MASK(SPI_CMD(HSPI), SPI_USR);
    WRITE_PERI_REG(SPI_CMD(HSPI), SPI_USR);

    ETS_SPI_INTR_ATTACH(spi_slave_isr_handler, context);
    ETS_SPI_INTR_ENABLE(); 
}

void ICACHE_FLASH_ATTR
spi_slave_deinit()
{
    ETS_SPI_INTR_ATTACH(NULL,NULL);
    ETS_SPI_INTR_DISABLE(); 

    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 0); //GPIO12 is GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 0); //GPIO13 is GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 0); //GPIO14 is GPIO
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 0); //GPIO15 is GPIO

    WRITE_PERI_REG(SPI_SLAVE(HSPI), 0);
    
    WRITE_PERI_REG(SPI_USER1(HSPI), SPI_USR_COMMAND | SPI_CK_I_EDGE);
    WRITE_PERI_REG(SPI_SLAVE1(HSPI), 0);
    WRITE_PERI_REG(SPI_PIN(HSPI), 6);
}

//ICACHE_FLASH_ATTR
void spi_slave_isr_handler(void *para)
{
    if (READ_PERI_REG(0x3ff00020)&BIT4) {
        //following 3 lines is to clear isr signal
        CLEAR_PERI_REG_MASK(SPI_SLAVE(SPI), 0x3ff);
    }
    else if (READ_PERI_REG(0x3ff00020) & BIT7) { //bit7 is for hspi isr,
        uint32_t status = READ_PERI_REG(SPI_SLAVE(HSPI));
        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI),    // disable interrupts
                SPI_TRANS_DONE_EN|
                SPI_SLV_WR_STA_DONE_EN|
                SPI_SLV_RD_STA_DONE_EN|
                SPI_SLV_WR_BUF_DONE_EN|
                SPI_SLV_RD_BUF_DONE_EN);


        // there is no explanation why this must be done
        // if it is done like so, unless there is a delay, 
        // subsequent transaction will be ruined
        //if (status & SPI_SLV_WR_BUF_DONE) {
        // removing this used to work on arduino testbench but fails on the real
        // thing not sure why
        //    SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET); // reset
        //}

        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI),    // clear interrupt flags
                SPI_TRANS_DONE|
                SPI_SLV_WR_STA_DONE|
                SPI_SLV_RD_STA_DONE|
                SPI_SLV_WR_BUF_DONE|
                SPI_SLV_RD_BUF_DONE); 
        SET_PERI_REG_MASK(SPI_SLAVE(HSPI),      // enable interrupts
                SPI_TRANS_DONE_EN|
                SPI_SLV_WR_STA_DONE_EN|
                SPI_SLV_RD_STA_DONE_EN|
                SPI_SLV_WR_BUF_DONE_EN|
                SPI_SLV_RD_BUF_DONE_EN);

        if ((status & SPI_SLV_RD_BUF_DONE) && hspi_txdata_cb) {
            hspi_txdata_cb(para);
        }
        if ((status & SPI_SLV_RD_STA_DONE) && hspi_txstatus_cb) {
            hspi_txstatus_cb(para);
        }
        if ((status & SPI_SLV_WR_STA_DONE) && hspi_rxstatus_cb) {
            uint32 s = READ_PERI_REG(SPI_RD_STATUS(HSPI));
            hspi_rxstatus_cb(para, s);
        }
        if (status & SPI_SLV_WR_BUF_DONE) {
            uint32 data; 
            for (int i = 0; i < 8; ++i) {
                data = READ_PERI_REG(SPI_W0(HSPI) + (i<<2));
                buffer[i<<2] = data & 0xff;
                buffer[(i<<2)+1] = (data >> 8) & 0xff;
                buffer[(i<<2)+2] = (data >> 16) & 0xff;
                buffer[(i<<2)+3] = (data >> 24) & 0xff;
            }

            if (hspi_rxdata_cb) {
                hspi_rxdata_cb(para, buffer, 32);
                //os_delay_us(150); // fml
            }
        }
    }
    else if (READ_PERI_REG(0x3ff00020)&BIT9) { //bit7 is for i2s isr,
    }
}

void ICACHE_FLASH_ATTR
spi_slave_set_data(uint8_t * data, uint8_t len)
{
    if (len > 32) len = 32;

    printf("spi_slave_set_data: %02x %02x len=%d\n", data[0], data[1], len);

    memcpy(txbuf, data, len);
    memset(txbuf + len, 0, 32-len);
    //for (int i = 0; i < 32; ++i) txbuf[i] = i;
    memcpy((uint32_t *)SPI_W8(HSPI), txbuf, 32);

// arduino-style diarrhea, why? 
//    for (int i = 0, wi = 8, bi = 0; i < len; ++i) {
//        out |= (i<len) ? (data[i] << (bi * 8)) : 0;
//        bi = (bi + 1) & 3;
//        if (!bi || (i + 1) == len) {
//            WRITE_PERI_REG(SPI_W0(HSPI) + (wi<<2), (uint32)(data));
//            out = 0;
//            ++wi;
//        }
//    }
}

void ICACHE_FLASH_ATTR
spi_slave_set_status(uint32_t status)
{
    WRITE_PERI_REG(SPI_WR_STATUS(HSPI), status);
}

// unsanitised espressif's diarrhea below

#if 0
/******************************************************************************
 * FunctionName : spi_byte_write_espslave
 * Description  : SPI master 1 byte transmission function for esp8266 slave,
 *     transmit 1byte data to esp8266 slave buffer needs 16bit transmission ,
 *     first byte is command 0x04 to write slave buffer, second byte is data
 * Parameters   :   
 *      uint8 spi_no - SPI module number, Only "SPI" and "HSPI" are valid
 *              uint8 data- transmitted data
 ******************************************************************************/
void ICACHE_FLASH_ATTR 
spi_byte_write_espslave(uint8 spi_no,uint8 data)
{
    if(spi_no>1) return; //handle invalid input number

    while(READ_PERI_REG(SPI_CMD(spi_no))&SPI_USR);
    SET_PERI_REG_MASK(SPI_USER(spi_no), SPI_USR_MOSI);
    CLEAR_PERI_REG_MASK(SPI_USER(spi_no), SPI_USR_MISO|SPI_USR_ADDR|SPI_USR_DUMMY);

    //SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
    // bit15-0 is cmd value.
    //0x70000000 is for 8bits cmd, 0x04 is eps8266 slave write cmd value
    WRITE_PERI_REG(SPI_USER2(spi_no), 
            ((7&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|4);
    WRITE_PERI_REG(SPI_W0(spi_no), (uint32)(data));
    SET_PERI_REG_MASK(SPI_CMD(spi_no), SPI_USR);
}

/******************************************************************************
 * FunctionName : spi_byte_read_espslave
 * Description  : SPI master 1 byte read function for esp8266 slave,
 *      read 1byte data from esp8266 slave buffer needs 16bit transmission ,
 *      first byte is command 0x06 to read slave buffer, second byte is 
 *      recieved data
 * Parameters   :   
 *      uint8 spi_no - SPI module number, Only "SPI" and "HSPI" are valid
 *              uint8* data- recieved data address
 *******************************************************************************/
void ICACHE_FLASH_ATTR
spi_byte_read_espslave(uint8 spi_no,uint8 *data)
{
    if(spi_no>1)        return; //handle invalid input number

    while(READ_PERI_REG(SPI_CMD(spi_no))&SPI_USR);

    SET_PERI_REG_MASK(SPI_USER(spi_no), SPI_USR_MISO);
    CLEAR_PERI_REG_MASK(SPI_USER(spi_no), SPI_USR_MOSI|SPI_USR_ADDR|SPI_USR_DUMMY);
    //SPI_FLASH_USER2 bit28-31 is cmd length,cmd bit length is value(0-15)+1,
    // bit15-0 is cmd value.
    //0x70000000 is for 8bits cmd, 0x06 is eps8266 slave read cmd value
    WRITE_PERI_REG(SPI_USER2(spi_no), 
            ((7&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S)|6);
    SET_PERI_REG_MASK(SPI_CMD(spi_no), SPI_USR);

    while(READ_PERI_REG(SPI_CMD(spi_no))&SPI_USR);
    *data=(uint8)(READ_PERI_REG(SPI_W0(spi_no))&0xff);
}
#endif




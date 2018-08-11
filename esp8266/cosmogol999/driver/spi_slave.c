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
static void spi_slave_isr_handler(void *para);

/******************************************************************************
 * FunctionName : spi_slave_init
 * Description  : SPI slave mode initial funtion, including mode setting,
 *      IO setting, transmission interrupt opening, interrupt function 
 *      registration
 * Parameters   :   
 *      uint8 spi_no - SPI module number, SPI or HSPI
 *      uint8 data_len - read&write data pack length,using byte as unit,
 *              the range is 1-32
 *******************************************************************************/
void ICACHE_FLASH_ATTR
spi_slave_init(uint8 spi_no, uint8 data_len)
{
    uint32 data_bit_len;
    if(spi_no>1)
        return; //handle invalid input number

    if (data_len<=1) {
        data_bit_len=7;
    } 
    else if (data_len>=32) {
        data_bit_len=0xff;
    }
    else {
        data_bit_len=(data_len<<3)-1;
    }

    //clear bit9,bit8 of reg PERIPHS_IO_MUX
    //bit9 should be cleared when HSPI clock doesn't equal CPU clock
    //bit8 should be cleared when SPI clock doesn't equal CPU clock
    ////WRITE_PERI_REG(PERIPHS_IO_MUX, 0x105); //clear bit9//TEST
    if(spi_no==SPI){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CLK_U, 1);//configure io to spi mode
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CMD_U, 1);//configure io to spi mode  
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA0_U, 1);//configure io to spi mode    
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_DATA1_U, 1);//configure io to spi mode    
    }else if(spi_no==HSPI){
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 2);//configure io to spi mode
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 2);//configure io to spi mode    
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 2);//configure io to spi mode    
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 2);//configure io to spi mode    
    }

    //regvalue=READ_PERI_REG(SPI_FLASH_SLAVE(spi_no));
    //slave mode,slave use buffers which are register "SPI_FLASH_C0~C15", 
    //enable trans done isr
    //set bit 30 bit 29 bit9,bit9 is trans done isr mask
    SET_PERI_REG_MASK(SPI_SLAVE(spi_no), 
            SPI_SLAVE_MODE|SPI_SLV_WR_RD_BUF_EN|
            SPI_SLV_WR_BUF_DONE_EN|SPI_SLV_RD_BUF_DONE_EN|
            SPI_SLV_WR_STA_DONE_EN|SPI_SLV_RD_STA_DONE_EN|
            SPI_TRANS_DONE_EN);
    //disable general trans intr 
    //CLEAR_PERI_REG_MASK(SPI_SLAVE(spi_no),SPI_TRANS_DONE_EN);

    //disable flash operation mode
    CLEAR_PERI_REG_MASK(SPI_USER(spi_no), SPI_FLASH_MODE);
    //SLAVE SEND DATA BUFFER IN C8-C15 
    //"MISO phase uses W8-W15"
    SET_PERI_REG_MASK(SPI_USER(spi_no),
            SPI_USR_COMMAND|        // Command phase
            SPI_USR_MISO_HIGHPART|  // MISO phase uses W8-W15
            SPI_CK_I_EDGE);         // data on rising edge of clk


    //////**************RUN WHEN SLAVE RECIEVE*******************///////
    //tow lines below is to configure spi timing.
    SET_PERI_REG_MASK(SPI_CTRL2(spi_no),
            (0x2&SPI_MOSI_DELAY_NUM)<<SPI_MOSI_DELAY_NUM_S) ;//delay num
    os_printf("SPI_CTRL2 is %08x\n",READ_PERI_REG(SPI_CTRL2(spi_no)));
    WRITE_PERI_REG(SPI_CLOCK(spi_no), 0);

    //set 8 bit slave command length, because slave must have at least one 
    //bit addr, 
    //8 bit slave+8bit addr, so master device first 2 bytes can be regarded 
    //as a command and the  following bytes are datas, 
    //32 bytes input wil be stored in SPI_FLASH_C0-C7
    //32 bytes output data should be set to SPI_FLASH_C8-C15
    WRITE_PERI_REG(SPI_USER2(spi_no), 
            (0x7&SPI_USR_COMMAND_BITLEN)<<SPI_USR_COMMAND_BITLEN_S); //0x70000000

    //set 8 bit slave recieve buffer length, the buffer is SPI_FLASH_C0-C7
    //set 8 bit slave status register, which is the low 8 bit of register 
    //"SPI_FLASH_STATUS"
    SET_PERI_REG_MASK(SPI_SLAVE1(spi_no),  
            ((data_bit_len&SPI_SLV_BUF_BITLEN)<< SPI_SLV_BUF_BITLEN_S)|
            ((0x7&SPI_SLV_STATUS_BITLEN)<<SPI_SLV_STATUS_BITLEN_S)|
            ((0x7&SPI_SLV_WR_ADDR_BITLEN)<<SPI_SLV_WR_ADDR_BITLEN_S)|
            ((0x7&SPI_SLV_RD_ADDR_BITLEN)<<SPI_SLV_RD_ADDR_BITLEN_S));

    SET_PERI_REG_MASK(SPI_PIN(spi_no),BIT19);//BIT19   

    // "maybe enable slave transmission liston" (sic!)
    SET_PERI_REG_MASK(SPI_CMD(spi_no), SPI_USR);

    ETS_SPI_INTR_ATTACH(spi_slave_isr_handler,NULL);
    ETS_SPI_INTR_ENABLE(); 
}

void ICACHE_FLASH_ATTR
spi_slave_deinit(uint8 spi_no, uint8 data_len)
{
    if (spi_no == HSPI) {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, 0); //GPIO12 is GPIO
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, 0); //GPIO13 is GPIO
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, 0); //GPIO14 is GPIO
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, 0); //GPIO15 is GPIO
    }
    WRITE_PERI_REG(SPI_SLAVE(spi_no), 0);
    // SPIUSSE | SPIUCOMMAND
    WRITE_PERI_REG(SPI_USER1(spi_no), SPI_USR_COMMAND | SPI_CK_I_EDGE);
    WRITE_PERI_REG(SPI_SLAVE1(spi_no), 0);
    WRITE_PERI_REG(SPI_PIN(spi_no), 6);
}

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
        SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SYNC_RESET); // reset
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
        if ((status & SPI_SLV_WR_BUF_DONE) && hspi_rxdata_cb) {
            uint32 data;
            for (int i = 0; i < 8; ++i) {
                data = READ_PERI_REG(SPI_W0(HSPI) + i);
                buffer[i<<2] = data & 0xff;
                buffer[(i<<2)+1] = (data >> 8) & 0xff;
                buffer[(i<<2)+2] = (data >> 16) & 0xff;
                buffer[(i<<2)+3] = (data >> 24) & 0xff;
            }

            hspi_rxdata_cb(para, buffer, 32);
        }
    }
    else if (READ_PERI_REG(0x3ff00020)&BIT9) { //bit7 is for i2s isr,
    }
}

void ICACHE_FLASH_ATTR
spi_slave_setdata(uint8_t * data, uint8_t len)
{
    uint32_t out;
    for (int i = 0, wi = 8, bi = 0; i < 32; ++i) {
        out |= (i<len) ? (data[i] << (bi * 8)) : 0;
        bi = (bi + 1) & 3;
        if (!bi) {
            WRITE_PERI_REG(SPI_W0(HSPI) + wi, (uint32)(data));
            out = 0;
            ++wi;
        }
    }
}

void ICACHE_FLASH_ATTR
spi_slave_setstatus(uint32_t status)
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




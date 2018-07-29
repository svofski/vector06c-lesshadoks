
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */


#include <esp8266.h>

#include "driver/gpio16.h"
#include "driver/spi.h"

#define LEDGPIO 2
#define BTNGPIO 0
#define CONF_DONEGPIO 5
#define nCONFIGGPIO 16

static ETSTimer resetBtntimer;

ICACHE_FLASH_ATTR void ioLed(int ena) {
	//gpio_output_set is overkill. ToDo: use better mactos
	if (ena) {
		gpio_output_set((1<<LEDGPIO), 0, (1<<LEDGPIO), 0);
	} else {
		gpio_output_set(0, (1<<LEDGPIO), (1<<LEDGPIO), 0);
	}
}

ICACHE_FLASH_ATTR static void resetBtnTimerCb(void *arg) {
	static int resetCnt=0;
	if ((GPIO_REG_READ(GPIO_IN_ADDRESS)&(1<<BTNGPIO))==0) {
		resetCnt++;
	} else {
		if (resetCnt>=6) { //3 sec pressed
			wifi_station_disconnect();
			wifi_set_opmode(STATIONAP_MODE); //reset to AP+STA mode
			printf("Reset to AP mode. Restarting system...\n");
			system_restart();
		}
		resetCnt=0;
	}
}


ICACHE_FLASH_ATTR void ioInit() {
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
        // CONF_DONE on GPIO5 is input
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5);
	gpio_output_set(0, 0, (1<<LEDGPIO), 
                (1<<BTNGPIO) || (1<<CONF_DONEGPIO));

        // nCONFIG on GPIO16 is input while not in use
        // gpio16 is a special snowflake
        gpio16_input_conf();

	os_timer_disarm(&resetBtntimer);
	os_timer_setfn(&resetBtntimer, resetBtnTimerCb, NULL);
	os_timer_arm(&resetBtntimer, 500, 1);
}

ICACHE_FLASH_ATTR int read_conf_done() 
{
    return GPIO_REG_READ(GPIO_IN_ADDRESS) & (1<<CONF_DONEGPIO);
}

ICACHE_FLASH_ATTR void nconfig_set(int value)
{
    gpio16_output_set(value);
}

ICACHE_FLASH_ATTR void nconfig_set_input(int is_input)
{
    if (is_input) {
        gpio16_input_conf();
    } else {
        gpio16_output_conf();
    }
}

ICACHE_FLASH_ATTR void io_shutdown_spi()
{
    spi_deinit(HSPI);
//    gpio_set_iomux_function(12, IOMUX_GPIO12_FUNC_GPIO);
//    gpio_set_iomux_function(13, IOMUX_GPIO13_FUNC_GPIO);
//    gpio_set_iomux_function(14, IOMUX_GPIO14_FUNC_GPIO);
//    gpio_disable(12);
//    gpio_disable(13);
//    gpio_disable(14);
}

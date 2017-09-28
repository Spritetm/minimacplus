#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_heap_alloc_caps.h"

#define ADNS_MOSI 19
#define ADNS_MISO 34ULL
#define ADNS_CLK 23
#define ADNS_CS 22

//#define DELAY() asm("nop; nop; nop; nop;nop; nop; nop; nop;nop; nop; nop; nop;nop; nop; nop; nop;")
#define DELAY() ets_delay_us(20);

static void adnsWrite(int adr, int val) {
	int data=((adr|0x80)<<8)|val;
	gpio_set_level(ADNS_CS, 0);
	DELAY();
	for (int mask=0x8000; mask!=0; mask>>=1) {
		gpio_set_level(ADNS_MOSI, (data&mask)?1:0);
		gpio_set_level(ADNS_CLK, 0);
		DELAY();
		gpio_set_level(ADNS_CLK, 1);
		DELAY();
	}
	gpio_set_level(ADNS_CS, 1);
}

static int adnsRead(int adr) {
	int data=((adr&0x7F)<<8)|0xff;
	int out=0;
	gpio_set_level(ADNS_CS, 0);
	DELAY();
	for (int mask=0x8000; mask!=0; mask>>=1) {
		gpio_set_level(ADNS_MOSI, (data&mask)?1:0);
		gpio_set_level(ADNS_CLK, 0);
		DELAY();
		if (gpio_get_level(ADNS_MISO)) out|=mask;
		gpio_set_level(ADNS_CLK, 1);
		DELAY();
	}
	gpio_set_level(ADNS_CS, 1);
	DELAY();
	return out&0xff;
}

int adns9500_init() {
	volatile int delay;
	int t;
	gpio_config_t gpioconf[2]={
		{
			.pin_bit_mask=(1<<ADNS_MOSI)|(1<<ADNS_CS)|(1<<ADNS_CLK), 
			.mode=GPIO_MODE_OUTPUT, 
			.pull_up_en=GPIO_PULLUP_DISABLE, 
			.pull_down_en=GPIO_PULLDOWN_DISABLE, 
			.intr_type=GPIO_PIN_INTR_DISABLE
		},{
			.pin_bit_mask=(1ULL<<ADNS_MISO), 
			.mode=GPIO_MODE_INPUT, 
//			.pull_up_en=GPIO_PULLUP_ENABLE, 
//			.pull_down_en=GPIO_PULLDOWN_DISABLE, 
			.intr_type=GPIO_PIN_INTR_DISABLE
		}
	};
	gpio_config(&gpioconf[0]);
	gpio_config(&gpioconf[1]);
	
	int tout=5;
	int r;
	do {
		adnsWrite(0,0);
		adnsWrite(0x3A,0x5a); //Power-Up Reset
		vTaskDelay(50/portTICK_RATE_MS);
		adnsRead(0x2);
		adnsRead(0x3);
		adnsRead(0x4);
		adnsRead(0x5);
		adnsRead(0x6);

		r=adnsRead(0);
		printf("ADNS: Read %X\n", r);
		tout--;
	} while (r!=0x33 && tout!=0);
	if (tout==0) return 0;

	adnsWrite(0x20, 0); //Main laser turn on !!!

	return 1;
}

void adns900_get_dxdybtn(int *x, int *y, int *btn) {
	int16_t sx, sy;
	sx=adnsRead(0x3);
	sx|=adnsRead(0x4)<<8;
	sy=adnsRead(0x5);
	sy|=adnsRead(0x6)<<8;
	ets_delay_us(100);
	*btn=gpio_get_level(ADNS_MISO)?0:1;
	if (sx!=0 || sy!=0) printf("Mouse: %hd %hd %d\n", sx, sy, *btn);
	*x=sx;
	*y=sy;
}

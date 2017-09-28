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
#include "mouse.h"
#include "adns9500.h"

#include "mipi.h"
#include "mipi_dsi.h"

//We need speed here!
#pragma GCC optimize ("O2")

typedef struct {
	uint8_t type;
	uint8_t addr;
	uint8_t len;
	uint8_t data[16];
} DispPacket;

//Copied from the X163QLN01 application note.
static const DispPacket initPackets[]={
	{0x39, 0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x00}},
	{0x39, 0xBD, 5, {0x01, 0x90, 0x14, 0x14, 0x00}},
	{0x39, 0xBE, 5, {0x01, 0x90, 0x14, 0x14, 0x01}},
	{0x39, 0xBF, 5, {0x01, 0x90, 0x14, 0x14, 0x00}},
	{0x39, 0xBB, 3, {0x07, 0x07, 0x07}},
	{0x39, 0xC7, 1, {0x40}},
	{0x39, 0xF0, 5, {0x55, 0xAA, 0x52, 0x80, 0x02}},
	{0x39, 0xFE, 2, {0x08, 0x50}},
	{0x39, 0xC3, 3, {0xF2, 0x95, 0x04}},
	{0x15, 0xCA, 1, {0x04}},
	{0x39, 0xF0, 5, {0x55, 0xAA, 0x52, 0x08, 0x01}},
	{0x39, 0xB0, 3, {0x03, 0x03, 0x03}},
	{0x39, 0xB1, 3, {0x05, 0x05, 0x05}},
	{0x39, 0xB2, 3, {0x01, 0x01, 0x01}},
	{0x39, 0xB4, 3, {0x07, 0x07, 0x07}},
	{0x39, 0xB5, 3, {0x05, 0x05, 0x05}},
	{0x39, 0xB6, 3, {0x53, 0x53, 0x53}},
	{0x39, 0xB7, 3, {0x33, 0x33, 0x33}},
	{0x39, 0xB8, 3, {0x23, 0x23, 0x23}},
	{0x39, 0xB9, 3, {0x03, 0x03, 0x03}},
	{0x39, 0xBA, 3, {0x13, 0x13, 0x13}},
	{0x39, 0xBE, 3, {0x22, 0x30, 0x70}},
	{0x39, 0xCF, 7, {0xFF, 0xD4, 0x95, 0xEF, 0x4F, 0x00, 0x04}},
	{0x15, 0x35, 1, {0x01}}, //
	{0x15, 0x36, 1, {0x00}}, //
	{0x15, 0xC0, 1, {0x20}}, //
	{0x39, 0xC2, 6, {0x17, 0x17, 0x17, 0x17, 0x17, 0x0B}},
	{0x32, 0, 0, {0}},
	{0x05, 0x11, 1, {0x00}}, //exit_sleep_mode
	{0x05, 0x29, 1, {0x00}}, //turn display on
	{0x15, 0x3A, 1, {0x55}}, //16-bit mode
//	{0x29, 0x2B, 4, {0x00, 0x00, 0x00, 0xEF}},
	{0,0,0,{0}}
};




#define SCALE_FACT 51 //Floating-point number, actually x/32. Divide mac reso by this to get lcd reso.

static uint8_t mask[512];

static void calcLut() {
	for (int i=0; i<512; i++) mask[i]=(1<<(7-(i&7)));
}

//Returns 0-1024
static int IRAM_ATTR findMacVal(uint8_t *data, int x, int y) {
	int a,b,c,d;
	int v=0;
	int rx=x/32;
	int ry=y/32;

	if (ry>=342) return 0;

	a=data[ry*(512/8)+rx/8]&mask[rx];
	rx++;
	b=data[ry*(512/8)+rx/8]&mask[rx];
	rx--; ry++;
	if (ry<342) {
		c=data[ry*(512/8)+rx/8]&mask[rx];
		rx++;
		d=data[ry*(512/8)+rx/8]&mask[rx];
	} else {
		c=1;
		d=1;
	}

	if (!a) v+=(31-(x&31))*(31-(y&31));
	if (!b) v+=(x&31)*(31-(y&31));
	if (!c) v+=(31-(x&31))*(y&31);
	if (!d) v+=(x&31)*(y&31);

	return v;
}


// Even pixels: a
//  RRBB
//   GG
//
// Odd pixels: b
//   GG
//  RRBB
//
// Even lines start with an even pixel, odd lines with an odd pixel.
//
// Due to the weird buildup, a horizontal subpixel actually is 1/3rd real pixel wide!

static int IRAM_ATTR findPixelVal(uint8_t *data, int x, int y) {
	int sx=(x*SCALE_FACT); //32th is 512/320 -> scale 512 mac screen to 320 width
	int sy=(y*SCALE_FACT);
	//sx and sy are now 27.5 fixed point values for the 'real' mac-like components
	int r,g,b;
	if (((x+y)&1)) {
		//pixel a
		r=findMacVal(data, sx, sy);
		b=findMacVal(data, sx+(SCALE_FACT/3)*2, sy);
		g=findMacVal(data, sx+(SCALE_FACT/3), sy+(SCALE_FACT/2));
	} else {
		//pixel b
		r=findMacVal(data, sx, sy+10);
		b=findMacVal(data, sx+(SCALE_FACT/3)*2, sy+(SCALE_FACT/1));
		g=findMacVal(data, sx+(SCALE_FACT/3), sy);
	}
	return ((r>>5)<<0)|((g>>4)<<5)|((b>>5)<<11);
}

volatile static uint8_t *currFbPtr=NULL;
SemaphoreHandle_t dispSem = NULL;

#define LINESPERBUF 32

static void initLcd() {
	mipiInit();
	vTaskDelay(20/portTICK_RATE_MS); //give screen a sec
	for (int j=0; j<2; j++) { //Init multiple times; for mysterious reasons it sometimes doesn't catch first time.
	for (int i=0; initPackets[i].type!=0; i++) {
		mipiResync();
		if (initPackets[i].type==0x39 || initPackets[i].type==0x29) {
			uint8_t data[17];
			data[0]=initPackets[i].addr;
			memcpy(data+1, initPackets[i].data, 16);
			mipiDsiSendLong(initPackets[i].type, data, initPackets[i].len+1);
		} else {
			uint8_t data[2]={initPackets[i].addr, initPackets[i].data[0]};
			mipiDsiSendShort(initPackets[i].type, data, initPackets[i].len+1);
			if (initPackets[i].type==5) vTaskDelay(300/portTICK_RATE_MS);
		}
	}
	}
	printf("Display inited.\n");
}

static void IRAM_ATTR displayTask(void *arg) {
	uint8_t *img=malloc((LINESPERBUF*320*2)+1);
	assert(img);
	calcLut();

	int firstrun=1;
	while(1) {
		int l=0;
		mipiResync();
		xSemaphoreTake(dispSem, portMAX_DELAY);
		uint8_t *myData=(uint8_t*)currFbPtr;
		img[0]=0x2c;
		uint8_t *p=&img[1];
		for (int j=0; j<320; j++) {
			for (int i=0; i<320; i++) {
				int v=findPixelVal(myData, i, j);
				*p++=(v&0xff);
				*p++=(v>>8);
			}
			l++;
			if (l>=LINESPERBUF || j==319) {
				mipiDsiSendLong(0x39, img, (LINESPERBUF*320*2)+1);
				img[0]=0x3c;
				l=0;
				p=&img[1];
				if (!firstrun && j>=200) break; //no need to render black bar in subsequent times
				firstrun=0;
			}
		}
	}
}



void dispDraw(uint8_t *mem) {
	int dx, dy, btn;
	currFbPtr=mem;
	xSemaphoreGive(dispSem);
	adns900_get_dxdybtn(&dx, &dy, &btn);
	mouseMove(dx, dy, btn);
//	printf("Mouse: %d %d\n", dx, dy);
}


void dispInit() {
	initLcd();
	printf("spi_lcd_init()\n");
	int ret=adns9500_init();
	if (!ret) printf("No mouse found!\n");

    dispSem=xSemaphoreCreateBinary();
#if CONFIG_FREERTOS_UNICORE
	xTaskCreatePinnedToCore(&displayTask, "display", 3000, NULL, 5, NULL, 0);
#else
	xTaskCreatePinnedToCore(&displayTask, "display", 3000, NULL, 5, NULL, 1);
#endif
}

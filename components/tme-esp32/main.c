#include "esp_attr.h"

#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"

#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/efuse_reg.h"
#include "soc/rtc_cntl_reg.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include "esp_err.h"
#include "nvs_flash.h"
#include "esp_partition.h"

#include "emu.h"
#include "tmeconfig.h"

#include "mpu6050.h"
#include "mpumouse.h"

unsigned char *romdata;

void emuTask(void *pvParameters)
{
	void *ram=malloc(TME_RAMSIZE);
	if (ram==NULL) {
		printf("Couldn't allocate main ram.\n");
		abort();
	}
	tmeStartEmu(ram, romdata);
}

void mouseTask(void *pvParameters)
{
	printf("Starting mouse task...\n");
	while(!mpu6050_init()) {
		printf("Can't init MPU....\n");
		vTaskDelay(100);
	}

	mpuMouseEmu();
}


void app_main()
{
	int i;
	const esp_partition_t* part;
	spi_flash_mmap_handle_t hrom;
	esp_err_t err;

	part=esp_partition_find_first(0x40, 0x1, NULL);
	if (part==0) printf("Couldn't find bootrom part!\n");
	err=esp_partition_mmap(part, 0, 128*1024, SPI_FLASH_MMAP_DATA, (const void**)&romdata, &hrom);
	if (err!=ESP_OK) printf("Couldn't map bootrom part!\n");
	printf("Starting emu...\n");
//	xTaskCreatePinnedToCore(&mouseTask, "mouse", 6*1024, NULL, 6, NULL, 0);
	xTaskCreatePinnedToCore(&emuTask, "emu", 8*1024, NULL, 5, NULL, 0);
}


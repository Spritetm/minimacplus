#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <driver/i2s.h>

static QueueHandle_t soundQueue;

int sndDone() {
	return uxQueueSpacesAvailable(soundQueue)<2;
}


#define SND_CHUNKSZ 32
int sndPush(uint8_t *data, int volume) {
	uint32_t tmpb[SND_CHUNKSZ];
	int i=0;
	int len=370;
	while (i<len) {
		int plen=len-i;
		if (plen>SND_CHUNKSZ) plen=SND_CHUNKSZ;
		for (int j=0; j<plen; j++) {
			int s=*data;
			s=((s-128)>>(7-volume));
//			s=s/16;
			tmpb[j]=((s+128)<<8)+((s+128)<<24);
			data+=2;
		}
		i2s_write_bytes(0, (char*)tmpb, plen*4, portMAX_DELAY);
		i+=plen;
	}
	return 1;
}

void sndInit() {
	i2s_config_t cfg={
		.mode=I2S_MODE_DAC_BUILT_IN|I2S_MODE_TX|I2S_MODE_MASTER,
		.sample_rate=22000,
		.bits_per_sample=16,
		.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format=I2S_COMM_FORMAT_I2S_MSB,
		.intr_alloc_flags=0,
		.dma_buf_count=4,
		.dma_buf_len=1024/4
	};
	i2s_driver_install(0, &cfg, 4, &soundQueue);
	i2s_set_pin(0, NULL);
	i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
	i2s_set_sample_rates(0, cfg.sample_rate);

#if 1
	//I2S enables *both* DAC channels; we only need DAC2. DAC1 is connected to the select button.
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_DAC_XPD_FORCE_M);
	CLEAR_PERI_REG_MASK(RTC_IO_PAD_DAC1_REG, RTC_IO_PDAC1_XPD_DAC_M);
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_INPUT,
		.pull_up_en=1,
		.pin_bit_mask=(1<<25)
	};
	gpio_config(&io_conf);
#endif
}



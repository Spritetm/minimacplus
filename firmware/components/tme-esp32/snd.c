#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <driver/i2s.h>

static QueueHandle_t soundQueue;

//powers of 2 plzthx
#define BUFLEN 2048

#define IO_AMP_DIS 2

static uint8_t buf[BUFLEN];
static volatile int wp=256, rp=0;

static int ampPowerTimeout=0;

static int bufLen() {
	return (wp-rp)&(BUFLEN-1);
}

int sndDone() { //1 when stuff can be written to buffer
//	printf("sndpoll %d\n", bufLen());
	return bufLen()<(BUFLEN-400);
}

volatile static int myVolume;

#define SND_CHUNKSZ 128
void sndTask(void *arg) {
	uint32_t tmpb[SND_CHUNKSZ]={0};
	printf("Sound task started.\n");
	while (1) {
		int volume=(int)myVolume;
		for (int j=0; j<SND_CHUNKSZ; j++) {
			int s=buf[rp];
			s=((s-128)>>(7-volume));
			s=s/16;
			tmpb[j]=((s+128)<<8)+((s+128)<<24);
			rp++;
			if (rp>=BUFLEN) rp=0;
		}
		i2s_write_bytes(0, (char*)tmpb, sizeof(tmpb), portMAX_DELAY);
//		printf("snd %d\n", rp);
	}
}

int sndPush(uint8_t *data, int volume) {
	while (!sndDone()) usleep(1000);
	myVolume=volume;
	if (volume) {
		for (int i=0; i<370; i++) {
			buf[wp]=*data;
			data+=2;
			wp++;
			if (wp>=BUFLEN) wp=0;
		}
		gpio_set_level(IO_AMP_DIS, 0);
		ampPowerTimeout=10;
	} else {
		//muted
		for (int i=0; i<370; i++) {
			buf[wp++]=128;
			if (wp>=BUFLEN) wp=0;
		}
		ampPowerTimeout--;
		if (ampPowerTimeout==0) gpio_set_level(IO_AMP_DIS, 1);
	}
	return 1;
}

void sndInit() {
	i2s_config_t cfg={
		.mode=I2S_MODE_DAC_BUILT_IN|I2S_MODE_TX|I2S_MODE_MASTER,
		.sample_rate=22200,
		.bits_per_sample=16,
		.channel_format=I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format=I2S_COMM_FORMAT_I2S_MSB,
		.intr_alloc_flags=0,
		.dma_buf_count=8,
		.dma_buf_len=1024/8
	};
	i2s_driver_install(0, &cfg, 4, &soundQueue);
	i2s_set_pin(0, NULL);
	i2s_set_dac_mode(I2S_DAC_CHANNEL_LEFT_EN);
	i2s_set_sample_rates(0, cfg.sample_rate);

	gpio_config_t io_conf_amp={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_OUTPUT,
		.pull_up_en=1,
		.pin_bit_mask=(1<<IO_AMP_DIS) //Amp enable line
	};
	gpio_config(&io_conf_amp);


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
	xTaskCreatePinnedToCore(&sndTask, "snd", 3*1024, NULL, 6, NULL, 1);
}



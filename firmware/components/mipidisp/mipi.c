/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
/*
Thing to emulate single-lane MIPI using a flipflop and a bunch of resistors.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/spi_common.h"
#include "soc/gpio_struct.h"
#include "soc/spi_struct.h"
#include "soc/spi_reg.h"
#include "driver/gpio.h"
#include "esp_heap_alloc_caps.h"
#include "mipi.h"
#include "hexdump.h"

//IO pins
#define GPIO_D0N_LS 4
#define GPIO_D0P_LS 33
#define GPIO_D0_HS 32
#define GPIO_CLKP_LS 25
#define GPIO_CLKN_LS 27
#define GPIO_FF_NRST 14
#define GPIO_FF_CLK 12
#define GPIO_NRST 5



#define HOST HSPI_HOST
#define IRQSRC ETS_SPI2_DMA_INTR_SOURCE
#define DMACH 2
#define DESCCNT 8

#define SOTEOTWAIT() asm volatile("nop; nop; nop; nop")
//#define SOTEOTWAIT() ets_delay_us(10);

static spi_dev_t *spidev;
static int cur_idle_desc=0;
static lldesc_t idle_dmadesc[3];
static lldesc_t data_dmadesc[DESCCNT];
static SemaphoreHandle_t sem=NULL;

//ToDo: move these to spi_common...

static int spi_freq_for_pre_n(int fapb, int pre, int n) {
	return (fapb / (pre * n));
}
/*
 * Set the SPI clock to a certain frequency. Returns the effective frequency set, which may be slightly
 * different from the requested frequency.
 */
static int spi_set_clock(spi_dev_t *hw, int fapb, int hz, int duty_cycle) {
	int pre, n, h, l, eff_clk;

	//In hw, n, h and l are 1-64, pre is 1-8K. Value written to register is one lower than used value.
	if (hz>((fapb/4)*3)) {
		//Using Fapb directly will give us the best result here.
		hw->clock.clkcnt_l=0;
		hw->clock.clkcnt_h=0;
		hw->clock.clkcnt_n=0;
		hw->clock.clkdiv_pre=0;
		hw->clock.clk_equ_sysclk=1;
		eff_clk=fapb;
	} else {
		//For best duty cycle resolution, we want n to be as close to 32 as possible, but
		//we also need a pre/n combo that gets us as close as possible to the intended freq.
		//To do this, we bruteforce n and calculate the best pre to go along with that.
		//If there's a choice between pre/n combos that give the same result, use the one
		//with the higher n.
		int bestn=-1;
		int bestpre=-1;
		int besterr=0;
		int errval;
		for (n=2; n<=64; n++) { //Start at 2: we need to be able to set h/l so we have at least one high and one low pulse.
			//Effectively, this does pre=round((fapb/n)/hz).
			pre=((fapb/n)+(hz/2))/hz;
			if (pre<=0) pre=1;
			if (pre>8192) pre=8192;
			errval=abs(spi_freq_for_pre_n(fapb, pre, n)-hz);
			if (bestn==-1 || errval<=besterr) {
				besterr=errval;
				bestn=n;
				bestpre=pre;
			}
		}

		n=bestn;
		pre=bestpre;
		l=n;
		//This effectively does round((duty_cycle*n)/256)
		h=(duty_cycle*n+127)/256;
		if (h<=0) h=1;

		hw->clock.clk_equ_sysclk=0;
		hw->clock.clkcnt_n=n-1;
		hw->clock.clkdiv_pre=pre-1;
		hw->clock.clkcnt_h=h-1;
		hw->clock.clkcnt_l=l-1;
		eff_clk=spi_freq_for_pre_n(fapb, pre, n);
	}
	return eff_clk;
}

static void spidma_intr(void *arg) {
	BaseType_t xHigherPriorityTaskWoken=0;
	spidev->dma_int_clr.val=0xFFFFFFFF; //clear all ints
	//Data is sent
//	ets_printf("int\n");

	xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
	if (xHigherPriorityTaskWoken) portYIELD_FROM_ISR();
}

/*
Brings up the clock and data lines to LP11, resyncs the flipflop, restarts the clock and DMA engine.
*/
void mipiResync() {
	//Get clock and data transceivers back in idle state
	gpio_set_level(GPIO_CLKN_LS, 1);
	SOTEOTWAIT();
	gpio_set_level(GPIO_CLKP_LS, 1);

	//Stop DMA transfer
	spidev->dma_conf.dma_tx_stop=1;
	while (spidev->ext2.val!=0) ;

	//Clear flipflop
	gpio_set_level(GPIO_FF_NRST, 0);
	ets_delay_us(1);
	gpio_set_level(GPIO_FF_NRST, 1);
	
	//Clock is in LP11 now. We should go LP01, LP00 to enable HS receivers
	gpio_set_level(GPIO_CLKP_LS, 0);
	SOTEOTWAIT();
	gpio_set_level(GPIO_CLKN_LS, 0);
	
	cur_idle_desc=0;
	idle_dmadesc[0].qe.stqe_next=&idle_dmadesc[0];
	//Set SPI to transfer contents of idle dmadesc
	spidev->dma_conf.val |= SPI_OUT_RST|SPI_IN_RST|SPI_AHBM_RST|SPI_AHBM_FIFO_RST;
	spidev->dma_out_link.start=0;
	spidev->dma_in_link.start=0;
	spidev->dma_conf.val &= ~(SPI_OUT_RST|SPI_IN_RST|SPI_AHBM_RST|SPI_AHBM_FIFO_RST);
	spidev->dma_conf.dma_tx_stop=0;
	spidev->dma_conf.dma_continue=1;
	spidev->dma_conf.out_data_burst_en=1;
	spidev->user.usr_mosi_highpart=0;
	spidev->dma_out_link.addr=(int)(&idle_dmadesc[0]) & 0xFFFFF;
	spidev->dma_out_link.start=1;
	spidev->user.usr_mosi=1;

/* HACK for inverted clock */
//	spidev->user.usr_addr=1;
//	spidev->user1.usr_addr_bitlen=0; //1 addr bit
/* End hack */

	spidev->cmd.usr=1;
}

void mipiInit() {
	esp_err_t ret;
	bool io_native=false;
	spi_bus_config_t buscfg={
		.miso_io_num=-1,
		.mosi_io_num=GPIO_D0_HS,
		.sclk_io_num=GPIO_FF_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz=4096*3
	};
	
	gpio_config_t io_conf={
		.intr_type=GPIO_INTR_DISABLE,
		.mode=GPIO_MODE_OUTPUT,
		.pin_bit_mask=(1<<GPIO_D0N_LS)|(1LL<<GPIO_D0P_LS)|(1LL<<GPIO_D0_HS)|(1<<GPIO_CLKP_LS)|
			(1<<GPIO_CLKN_LS)|(1<<GPIO_FF_NRST)|(1<<GPIO_FF_CLK)|(1<<GPIO_NRST),
	};
	gpio_config(&io_conf);

	assert(spicommon_periph_claim(HOST));
	ret=spicommon_bus_initialize_io(HOST, &buscfg,  DMACH, SPICOMMON_BUSFLAG_MASTER, &io_native);
	assert(ret==ESP_OK);
	assert(spicommon_dma_chan_claim(DMACH));
	spidev=spicommon_hw_for_host(HOST);
	
	//Set up idle dma desc
	uint8_t *idle_mem=pvPortMallocCaps(64, MALLOC_CAP_DMA);
	memset(idle_mem, 0, 64);
	for (int i=0; i<3; i++) {
		idle_dmadesc[i].size=64;
		idle_dmadesc[i].length=64;
		idle_dmadesc[i].buf=idle_mem;
		idle_dmadesc[i].eof=0;
		idle_dmadesc[i].sosf=0;
		idle_dmadesc[i].owner=1;
		idle_dmadesc[i].qe.stqe_next = &idle_dmadesc[i];
	}

	esp_intr_alloc(IRQSRC, 0, spidma_intr, NULL, NULL);

	//Reset DMA
	spidev->dma_conf.val|=SPI_OUT_RST|SPI_IN_RST|SPI_AHBM_RST|SPI_AHBM_FIFO_RST;
	spidev->dma_out_link.start=0;
	spidev->dma_in_link.start=0;
	spidev->dma_conf.val&=~(SPI_OUT_RST|SPI_IN_RST|SPI_AHBM_RST|SPI_AHBM_FIFO_RST);
	//Reset timing
	spidev->ctrl2.val=0;
	spi_set_clock(spidev, 80000000, 40000000, 128);
	
	//Configure SPI host
	spidev->ctrl.rd_bit_order=1; //LSB first
	spidev->ctrl.wr_bit_order=1;
	spidev->pin.ck_idle_edge=0;
	spidev->user.ck_out_edge=1; //0 for <40MHz?
	spidev->ctrl2.miso_delay_mode=0;
	spidev->ctrl.val &= ~(SPI_FREAD_DUAL|SPI_FREAD_QUAD|SPI_FREAD_DIO|SPI_FREAD_QIO);
	spidev->user.val &= ~(SPI_FWRITE_DUAL|SPI_FWRITE_QUAD|SPI_FWRITE_DIO|SPI_FWRITE_QIO);
	
	//Disable unneeded ints
	spidev->slave.val=0;
	spidev->dma_int_ena.val=0;
	//Set int on EOF
	spidev->dma_int_clr.val=0xFFFFFFFF; //clear all ints
	spidev->dma_int_ena.out_eof=1;
//	spidev->dma_int_ena.in_suc_eof=1;
	
	//Init GPIO to MIPI idle levels
	gpio_set_level(GPIO_D0N_LS, 1);
	gpio_set_level(GPIO_D0P_LS, 1);
	gpio_set_level(GPIO_CLKN_LS, 1);
	gpio_set_level(GPIO_CLKP_LS, 1);

	//Reset display
	gpio_set_level(GPIO_NRST, 0);
	ets_delay_us(200);
	gpio_set_level(GPIO_NRST, 1);
	//Wait till display lives
	vTaskDelay(100/portTICK_RATE_MS);

	sem=xSemaphoreCreateBinary();
//	xSemaphoreGive(sem);
	mipiResync(0);

}

void mipiSendMultiple(uint8_t **data, int *lengths, int count) {
	esp_err_t ret;
	if (count==0) return;             //no need to send anything
	assert(data[0][0]==0xB8);
//	hexdump(data, count);
	//Set up link to new transfer
	int next_idle_desc=(cur_idle_desc==0)?1:0;

	int last=-1;
	for (int i=0; i<count; i++) {
		last++;
		spicommon_setup_dma_desc_links(&data_dmadesc[last], lengths[i], data[i], false);
		//Look for new end
		for (; data_dmadesc[last].eof!=1; last++) ;
		//Kill eof and make desc point to the next one.
		data_dmadesc[last].eof=0;
		data_dmadesc[last].qe.stqe_next=&data_dmadesc[last+1];
	}
	//Make very last data desc go to the runout idle desc
	data_dmadesc[last].qe.stqe_next=&idle_dmadesc[2];
	//and make the runout go to the end idle desc
	idle_dmadesc[2].qe.stqe_next=&idle_dmadesc[next_idle_desc];
	idle_dmadesc[2].eof=1;
	//Make sure that idle desc keeps spinning
	idle_dmadesc[next_idle_desc].qe.stqe_next=&idle_dmadesc[next_idle_desc];

	gpio_set_level(GPIO_D0P_LS, 0);
	SOTEOTWAIT();
	gpio_set_level(GPIO_D0N_LS, 0);

	//Break the loop on the current idle descriptor
	idle_dmadesc[cur_idle_desc].qe.stqe_next=&data_dmadesc[0];
	//Okay, done.
	cur_idle_desc=next_idle_desc; //for next time

	//Wait until transmission is done
	xSemaphoreTake(sem, portMAX_DELAY);

	gpio_set_level(GPIO_D0N_LS, 1);
	SOTEOTWAIT();
	gpio_set_level(GPIO_D0P_LS, 1);
}

void mipiSend(uint8_t *data, int len) {
	mipiSendMultiple(&data, &len, 1);
}


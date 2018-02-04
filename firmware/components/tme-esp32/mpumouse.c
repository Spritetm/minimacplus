//Failed attempt to emulate a mouse using a MPU
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

#include <stdlib.h>
#include "esp_err.h"

#include "emu.h"
#include "tmeconfig.h"

#include "mpu6050.h"
#include "mpumouse.h"



portMUX_TYPE dxdyMux=portMUX_INITIALIZER_UNLOCKED;
static int dx, dy, btn;

void mpuMouseGetDxDyBtn(int *pDx, int *pDy, int *pBtn) {
	printf("Mouse: %d %d\n", dx, dy);
	portENTER_CRITICAL(&dxdyMux);
	*pDx=dx;
	*pDy=dy;
	*pBtn=0;
	dx=0;
	dy=0;
	portEXIT_CRITICAL(&dxdyMux);
}


#define RECAL_SAMPS 32
static int recal[RECAL_SAMPS][2];
static int recalPos=0;
static int idleAx=0, idleAy=0;
static int curDx, curDy;

//Add sample to correction
static void addToCorr(int ax, int ay) {
	recalPos++;
	if (recalPos>=RECAL_SAMPS) recalPos=0;
	recal[recalPos][0]=ax;
	recal[recalPos][1]=ay;
}


#define DIV_NOMOTION 5000

static void checkCorr() {
	int avgax=0;
	int avgay=0;
	for (int i=0; i<RECAL_SAMPS; i++) {
		avgax+=recal[i][0];
		avgay+=recal[i][1];
	}
	avgax/=RECAL_SAMPS;
	avgay/=RECAL_SAMPS;

	int div=0;
	for (int i=0; i<RECAL_SAMPS; i++) {
		int dax=recal[i][0]-avgax;
		int day=recal[i][1]-avgay;
		div+=dax*dax+day*day;
	}
	div/=RECAL_SAMPS;
	if (div<DIV_NOMOTION) {
		//Accelerometer is not moving.
		idleAx=avgax;
		idleAy=avgay;
		curDx=0;
		curDy=0;
	}
}


void mpuMouseEmu() {
	int calCtr=0;
	mpu6050_accel_tp a[128];
	while(1) {
		int r=mpu6050_read_fifo(a, 128);
		if (r) {
			int cax=0, cay=0;
			for (int i=0; i<r; i++) {
				cax+=a[i].accelx;
				cay+=a[i].accely;
			}
			cax/=r;
			cay/=r;
			addToCorr(cax, cay);
			
			//Subtract bias
			cax-=idleAx;
			cay-=idleAy;

			//Integrate
			curDx+=cax;
			curDy+=cay;

			portENTER_CRITICAL(&dxdyMux);
			dx+=curDx/(1<<12);
			dy+=curDy/(1<<12);
			portEXIT_CRITICAL(&dxdyMux);
//			printf("% 2d samp x % 6d y % 6d\n", r, dx, dy);
		} else {
			printf("Mpu: No samples?\n");
		}
		calCtr++;
		if (calCtr>=10) {
			calCtr=0;
			checkCorr();
		}
		vTaskDelay(1);
	}
}

#include <stdint.h>
#include <stdio.h>


typedef struct {
	int lastClkVal;
	int pos;
	uint8_t cmd;
	uint8_t mem[32];
} Rtc;

static Rtc rtc;

void rtcTick() {
	int x;
	for (x=0; x<3; x++) {
		rtc.mem[x]++;
		if (rtc.mem[x]!=0) return;
	}
}

extern void saveRtcMem(char *mem);

int rtcCom(int en, int dat, int clk) {
	int ret=0;
	clk=clk?1:0;
	if (en) {
		rtc.pos=0;
		rtc.cmd=0;
	} else {
		if (clk!=rtc.lastClkVal && !clk) {
			if (rtc.pos<8 || (rtc.pos<16 && ((rtc.cmd&0x8000)==0)) ) {
				if (dat) rtc.cmd|=(1<<(15-rtc.pos));
			} else if (rtc.cmd&0x8000) {
				if (rtc.pos==8) {
					rtc.cmd|=rtc.mem[(rtc.cmd&0x7C00)>>10];
				}
				ret=((rtc.cmd&(1<<(15-rtc.pos)))?1:0);
			} else if (rtc.pos==15) {
				if ((rtc.cmd&0x8000)==0) {
					rtc.mem[(rtc.cmd&0x7C00)>>10]=rtc.cmd&0xff;
					saveRtcMem(rtc.mem);
				}
				printf("RTC/PRAM CMD %x\n", rtc.cmd);
			}
			rtc.pos++;
		}
	}
	rtc.lastClkVal=clk;
	return ret;
}

void rtcInit(char *mem) {
	memcpy(rtc.mem, mem, 32);
}

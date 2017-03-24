#include <stdio.h>
#include <stdint.h>
#include "scc.h"
#include "m68k.h"

/*
THis is an extremely minimal SCC implementation: it only somewhat implements the interrupt mechanism for the
DCD pins because that is what is needed for the mouse to work.
*/


void sccIrq(int ena);

typedef struct {
	int regptr;
	int intpending;
	int intpendingOld;
	int dcd[2];
	int cts[2];
	int wr1[2];
	int wr15[2];
} Scc;

static Scc scc;

//WR15 is the External/Status Interrupt Control and has the interrupt enable bits.
#define SCC_WR15_BREAK  (1<<7)
#define SCC_WR15_TXU    (1<<6)
#define SCC_WR15_CTS    (1<<5)
#define SCC_WR15_SYNC   (1<<4)
#define SCC_WR15_DCD    (1<<3)
#define SCC_WR15_ZCOUNT (1<<1)

//WR3, when read, gives the Interrupt Pending status.
//This is reflected in scc.intpending.
#define SCC_WR3_CHB_EXT (1<<0)
#define SCC_WR3_CHB_TX  (1<<1)
#define SCC_WR3_CHB_RX  (1<<2)
#define SCC_WR3_CHA_EXT (1<<3)
#define SCC_WR3_CHA_TX  (1<<4)
#define SCC_WR3_CHA_RX  (1<<5)

static void raiseInt(int chan) {
	if ((scc.wr1[chan]&1) && (scc.intpending&(~scc.intpendingOld))) {
		scc.intpendingOld=scc.intpending;
		printf("SCC int, pending %x\n", scc.intpending);
		sccIrq(1);
	}
}

void sccSetDcd(int chan, int val) {
	val=val?1:0;
	if (scc.dcd[chan]!=val) {
		if (chan==SCC_CHANA) {
			scc.intpending|=SCC_WR3_CHA_EXT;
		} else {
			scc.intpending|=SCC_WR3_CHB_EXT;
		}
	}
	if ((scc.intpending&SCC_WR3_CHA_EXT) && (scc.wr15[SCC_CHANA]&SCC_WR15_DCD)) raiseInt(SCC_CHANA);
	if ((scc.intpending&SCC_WR3_CHB_EXT) && (scc.wr15[SCC_CHANB]&SCC_WR15_DCD)) raiseInt(SCC_CHANB);
	if ((scc.intpending&SCC_WR3_CHA_EXT) && (scc.wr15[SCC_CHANA]&SCC_WR15_CTS)) raiseInt(SCC_CHANA);
	if ((scc.intpending&SCC_WR3_CHB_EXT) && (scc.wr15[SCC_CHANB]&SCC_WR15_CTS)) raiseInt(SCC_CHANB);
	scc.dcd[chan]=val;
}

void sccWrite(unsigned int addr, unsigned int val) {
	int chan, reg;
	if (addr & (1<<1)) chan=SCC_CHANA; else chan=SCC_CHANB;
	if (addr & (1<<2)) {
		//Data
		reg=8;
	} else {
		//Control
		reg=scc.regptr;
		scc.regptr=0;
	}
	if (reg==0) {
		scc.regptr=val&0x7;
		if ((val&0x38)==0x8) scc.regptr|=8;
		if ((val&0x38)==0x10) {
			scc.intpending=0;
			scc.intpendingOld=0;
		}
	} else if (reg==1) {
		scc.wr1[chan]=val;
	} else if (reg==15) {
		scc.wr15[chan]=val;
	}
//	printf("SCC: write to addr %x chan %d reg %d val %x\n", addr, chan, reg, val);
}


unsigned int sccRead(unsigned int addr) {
	int chan, reg, val=0xff;
	if (addr & (1<<1)) chan=SCC_CHANA; else chan=SCC_CHANB;
	if (addr & (1<<2)) {
		//Data
		reg=8;
	} else {
		//Control
		reg=scc.regptr;
		scc.regptr=0;
	}
	if (reg==0) {
		val=(1<<2);
		if (scc.dcd[chan]) val|=(1<<3);
		if (scc.cts[chan]) val|=(1<<5);
	} else if (reg==1) {
		val=scc.wr1[chan];
	} else if (reg==2 && chan==SCC_CHANB) {
		//We assume this also does an intack.
		int rsn=0;
		if (scc.intpending & SCC_WR3_CHB_EXT) {
			rsn=1;
			scc.intpending&=~SCC_WR3_CHB_EXT;
		}
		if (scc.intpending & SCC_WR3_CHA_EXT) {
			rsn=5;
			scc.intpending&=~SCC_WR3_CHA_EXT;
		}
		val=rsn<<1;
		if (scc.intpending&0x38) raiseInt(SCC_CHANA);
		if (scc.intpending&0x07) raiseInt(SCC_CHANB);
	} else if (reg==3) {
		if (chan==SCC_CHANA) val=scc.intpending; else val=0;
	} else if (reg==15) {
		val=scc.wr15[chan];
	}
	printf("SCC: read from chan %d reg %d val %x\n", chan, reg, val);
	return val;
}


#include <stdio.h>
#include <stdint.h>
#include "scc.h"
#include "m68k.h"
#include "hexdump.h"
#include "localtalk.h"

/*
Emulation of the Zilog 8530 SCC.

This is an extremely minimal SCC implementation: it only somewhat implements the interrupt mechanism for the
DCD pins because that is what is needed for the mouse to work.
*/


void sccIrq(int ena);

#define BUFLEN 8192

typedef struct {
	int dcd;
	int cts;
	int wr1;
	int sdlcaddr;
	int wr15;
	int hunting;
	int txTimer;
	uint8_t txData[BUFLEN];
	uint8_t rxData[BUFLEN];
	int txPos, rxPos, rxLen;
	int rxDelay;
} SccChan;

typedef struct {
	int regptr;
	int intpending;
	int intpendingOld;
	SccChan chan[2];
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
	if ((scc.chan[chan].wr1&1) && (scc.intpending&(~scc.intpendingOld))) {
		scc.intpendingOld=scc.intpending;
//		printf("SCC int, pending %x\n", scc.intpending);
		sccIrq(1);
	}
}

void sccSetDcd(int chan, int val) {
	val=val?1:0;
	if (scc.chan[chan].dcd!=val) {
		if (chan==SCC_CHANA) {
			if (scc.chan[SCC_CHANA].wr15&SCC_WR15_DCD) {
				scc.intpending|=SCC_WR3_CHA_EXT;
				 raiseInt(SCC_CHANA);
			}
		} else {
			if (scc.chan[SCC_CHANB].wr15&SCC_WR15_DCD) {
				scc.intpending|=SCC_WR3_CHB_EXT;
				raiseInt(SCC_CHANB);
			}
		}
	}
	scc.chan[chan].dcd=val;
}

void sccTxFinished(int chan) {
	hexdump(scc.chan[chan].txData, scc.chan[chan].txPos);
	localtalkSend(scc.chan[chan].txData, scc.chan[chan].txPos);
	scc.chan[chan].txPos=0;
	scc.chan[chan].hunting=1;
}

void sccRecv(int chan, uint8_t *data, int len) {
	memcpy(scc.chan[chan].rxData, data, len);
	scc.chan[chan].rxData[len]=0xA5; //crc1
	scc.chan[chan].rxData[len+1]=0xA5; //crc2
	scc.chan[chan].rxData[len+2]=0; //abort
	scc.chan[chan].rxLen=len+3;
	scc.chan[chan].rxDelay=30;
}

static void triggerRx(int chan) {
	if (!scc.chan[chan].hunting) return;
	printf("Receiving:\n");
	hexdump(scc.chan[chan].rxData, scc.chan[chan].rxLen);
	if (scc.chan[chan].rxData[0]==0xFF || scc.chan[chan].rxData[0]==scc.chan[chan].sdlcaddr) {
		scc.chan[chan].rxPos=0;
		//Sync int
		if (scc.chan[chan].wr15&SCC_WR15_SYNC) {
			scc.intpending|=(chan?SCC_WR3_CHA_EXT:SCC_WR3_CHB_EXT);
			raiseInt(chan);
		}
		//RxD int
		int rxintena=scc.chan[chan].wr1&0x18;
		if (rxintena==0x10 || rxintena==0x08) {
			scc.intpending|=(chan?SCC_WR3_CHA_RX:SCC_WR3_CHB_RX);
			raiseInt(chan);
		}
		scc.chan[chan].hunting=0;
	} else {
		scc.chan[chan].rxLen=0;
	}
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
		if ((val&0x38)==0x18) {
			//SCC abort: parse whatever we sent
			printf("SCC ABORT: Sent data\n");
			sccTxFinished(chan);
		}
		if ((val&0xc0)==0xC0) {
			//reset tx underrun latch
			if (scc.chan[chan].txTimer==0) scc.chan[chan].txTimer=-1;
		}
	} else if (reg==1) {
		scc.chan[chan].wr1=val;
	} else if (reg==3) {
		//bitsperchar1, bitsperchar0, autoenables, enterhuntmode, rxcrcen, addresssearch, synccharloadinh, rxena
		//autoenable: cts = tx ena, dcd = rx ena
		if (val&0x10) scc.chan[chan].hunting=1;
	} else if (reg==6) {
		scc.chan[chan].sdlcaddr=val;
	} else if (reg==8) {
		scc.chan[chan].txData[scc.chan[chan].txPos++]=val;
		printf("TX! Pos %d\n", scc.chan[chan].txPos);
		scc.chan[chan].txTimer+=30;
	} else if (reg==15) {
		scc.chan[chan].wr15=val;
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
		val=(1<<2); //tx buffer always empty
		if (scc.chan[chan].rxLen && !scc.chan[chan].rxDelay) val|=(1<<0);
		if (scc.chan[chan].txTimer==0) val|=(1<<6);
		if (scc.chan[chan].dcd) val|=(1<<3);
		if (scc.chan[chan].cts) val|=(1<<5);
		if (scc.chan[chan].hunting) val|=(1<<4);
		if (scc.chan[chan].rxPos==scc.chan[chan].rxLen-1) val|=(1<<7); //abort
	} else if (reg==1) {
		//EndOfFrame, CRCErr, RXOverrun, ParityErr, Residue0, Residue1, Residue2, AllSent
		val=0x7; //residue code 011, all sent
		if (scc.chan[chan].rxPos==scc.chan[chan].rxLen-2) val|=(1<<7); //end of frame
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
	} else if (reg==8) {
		//rx buffer
		if (scc.chan[chan].rxLen && !scc.chan[chan].rxDelay) {
			val=scc.chan[chan].rxData[scc.chan[chan].rxPos++];
			if (scc.chan[chan].rxPos==scc.chan[chan].rxLen) {
				scc.chan[chan].rxLen=0;
			} else {
				int rxintena=scc.chan[chan].wr1&0x18;
				if (rxintena==0x10) {
					scc.intpending|=(chan?SCC_WR3_CHA_RX:SCC_WR3_CHB_RX);
					raiseInt(chan);
				}
			}
			if (scc.chan[chan].rxPos==scc.chan[chan].rxLen-1) scc.chan[chan].hunting=1;
			if (scc.chan[chan].rxPos==scc.chan[chan].rxLen-1 && scc.chan[chan].wr15&SCC_WR15_BREAK) {
				scc.intpending|=(chan?SCC_WR3_CHA_EXT:SCC_WR3_CHB_EXT);
				raiseInt(chan);
			}
		} else {
			val=0;
		}
	} else if (reg==10) {
		//Misc status (mostly SDLC)
		val=0;
	} else if (reg==15) {
		val=scc.chan[chan].wr15;
	}
//	printf("SCC: read from chan %d reg %d val %x\n", chan, reg, val);
	return val;
}

//Called at about 800KHz
void sccTick() {
	for (int n=0; n<2; n++) {
		if (scc.chan[n].txTimer>0) {
			scc.chan[n].txTimer--;
			if (scc.chan[n].txTimer==0) {
				printf("Tx buffer empty: Sent data\n");
				sccTxFinished(n);
			}
		}
		if (scc.chan[n].rxDelay!=0) {
			scc.chan[n].rxDelay--;
			if (scc.chan[n].rxDelay==0) {
				triggerRx(n);
			}
		}
	}
}

void sccInit() {
	sccSetDcd(1, 1);
	sccSetDcd(2, 1);
	scc.chan[0].txTimer=-1;
	scc.chan[1].txTimer=-1;
}


#include <stdio.h>
#include <stdint.h>
#include "scc.h"
#include "m68k.h"
#include "hexdump.h"
#include "network/localtalk.h"
#include <string.h>
/*
Emulation of the Zilog 8530 SCC.

Supports basic mouse pins plus hacked in LocalTalk
*/


void sccIrq(int ena);

#define BUFLEN 8192
#define NO_RXBUF 4

typedef struct {
	int delay; //-1 if buffer is free, 
	int len;
	uint8_t data[BUFLEN];
} RxBuf;

typedef struct {
	int dcd;
	int cts;
	int wr1, wr15;
	int sdlcaddr;
	int hunting;
	int txTimer;
	uint8_t txData[BUFLEN];
	RxBuf rx[NO_RXBUF];
	int txPos;
	int rxPos;
	int rxBufCur;
	int rxDelay;
} SccChan;

typedef struct {
	int regptr;
	int intpending;
	int intpendingOld;
	SccChan chan[2];
	int wr2, wr9;
} Scc;

static Scc scc;


static void triggerRx(int chan);


static int rxHasByte(int chan) {
	return (scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay==0);
}


static void rxBufIgnoreRest(int chan) {
	scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay=-1;
	scc.chan[chan].rxBufCur++;
	if (scc.chan[chan].rxBufCur>=NO_RXBUF) scc.chan[chan].rxBufCur=0;
	scc.chan[chan].rxPos=0;
}

static int rxByte(int chan, int *bytesLeftInBuf) {
	int ret;
	int curbuf=scc.chan[chan].rxBufCur;
	printf("RxBuf: bufid %d byte %d/%d\n", curbuf, scc.chan[chan].rxPos, scc.chan[chan].rx[curbuf].len);
	if (scc.chan[chan].rx[curbuf].delay!=0) return 0;
	if (bytesLeftInBuf) *bytesLeftInBuf=scc.chan[chan].rx[curbuf].len-scc.chan[chan].rxPos-1;
	ret=scc.chan[chan].rx[curbuf].data[scc.chan[chan].rxPos++];
	if (scc.chan[chan].rxPos==scc.chan[chan].rx[curbuf].len) {
		rxBufIgnoreRest(chan);
	}
	return ret;
}

static int rxBytesLeft(int chan) {
	if (scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay!=0) return 0;
	return scc.chan[chan].rx[scc.chan[chan].rxBufCur].len-scc.chan[chan].rxPos;
}

static int rxBufTick(int chan) {
	if (scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay>0) {
		scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay--;
		if (scc.chan[chan].rx[scc.chan[chan].rxBufCur].delay==0) {
			printf("Feeding buffer %d into SCC\n", scc.chan[chan].rxBufCur);
			return 1;
		}
	}
	return 0;
}


//WR15 is the External/Status Interrupt Control and has the interrupt enable bits.
#define SCC_WR15_BREAK  (1<<7)
#define SCC_WR15_TXU    (1<<6)
#define SCC_WR15_CTS    (1<<5)
#define SCC_WR15_SYNC   (1<<4)
#define SCC_WR15_DCD    (1<<3)
#define SCC_WR15_ZCOUNT (1<<1)

//RR3, when read, gives the Interrupt Pending status.
//This is reflected in scc.intpending.
#define SCC_RR3_CHB_EXT (1<<0)
#define SCC_RR3_CHB_TX  (1<<1)
#define SCC_RR3_CHB_RX  (1<<2)
#define SCC_RR3_CHA_EXT (1<<3)
#define SCC_RR3_CHA_TX  (1<<4)
#define SCC_RR3_CHA_RX  (1<<5)


static void raiseInt(int chan) {
	if ((scc.chan[chan].wr1&1) ){ //&& (scc.intpending&(~scc.intpendingOld))) {
		scc.intpendingOld=scc.intpending;
		printf("SCC int, pending %x\n", scc.intpending);
		sccIrq(1);
	}
}

void sccSetDcd(int chan, int val) {
	val=val?1:0;
	if (scc.chan[chan].dcd!=val) {
		if (chan==SCC_CHANA) {
			if (scc.chan[SCC_CHANA].wr15&SCC_WR15_DCD) {
				scc.intpending|=SCC_RR3_CHA_EXT;
				 raiseInt(SCC_CHANA);
			}
		} else {
			if (scc.chan[SCC_CHANB].wr15&SCC_WR15_DCD) {
				scc.intpending|=SCC_RR3_CHB_EXT;
				raiseInt(SCC_CHANB);
			}
		}
	}
	scc.chan[chan].dcd=val;
}

void sccRecv(int chan, uint8_t *data, int len, int delay) {
	int bufid=scc.chan[chan].rxBufCur;
	int n=0;
	do {
		if (scc.chan[chan].rx[bufid].delay==-1) break;
		bufid++;
		if (bufid>=NO_RXBUF) bufid=0;
		n++;
	} while(bufid!=scc.chan[chan].rxBufCur);

	//check if all buffers are full
	if (scc.chan[chan].rx[bufid].delay!=-1) {
		printf("Eek! Can't queue buffer: full!\n");
		return;
	}

	printf("Serial transmit for chan %d queued; bufidx %d, len=%d delay=%d, %d other buffers in queue\n", chan, bufid, len, delay, n);
	memcpy(scc.chan[chan].rx[bufid].data, data, len);
	scc.chan[chan].rx[bufid].data[len]=0xA5; //crc1
	scc.chan[chan].rx[bufid].data[len+1]=0xA5; //crc2
	scc.chan[chan].rx[bufid].data[len+2]=0; //abort
	scc.chan[chan].rx[bufid].len=len+3;
	scc.chan[chan].rx[bufid].delay=delay;
}

void sccTxFinished(int chan) {
	hexdump(scc.chan[chan].txData, scc.chan[chan].txPos);
	localtalkSend(scc.chan[chan].txData, scc.chan[chan].txPos);
	//Echo back data over Rx of same channel
//	sccRecv(chan, scc.chan[chan].txData, scc.chan[chan].txPos, 0);
//	triggerRx(chan);

	scc.chan[chan].txPos=0;
	scc.chan[chan].hunting=1;
}


static void triggerRx(int chan) {
	if (!scc.chan[chan].hunting) return;
	int bufid=scc.chan[chan].rxBufCur;
	printf("SCC: Receiving bufid %d:\n", bufid);
	hexdump(scc.chan[chan].rx[bufid].data, scc.chan[chan].rx[bufid].len);
	if (scc.chan[chan].rx[bufid].data[0]==0xFF || scc.chan[chan].rx[bufid].data[0]==scc.chan[chan].sdlcaddr) {
		scc.chan[chan].rxPos=0;
		printf("WR15: 0x%X WR1: %X\n", scc.chan[chan].wr15, scc.chan[chan].wr1);
		//Sync int
		if (scc.chan[chan].wr15&SCC_WR15_SYNC) {
			scc.intpending|=(chan?SCC_RR3_CHA_EXT:SCC_RR3_CHB_EXT);
			raiseInt(chan);
		}
		//RxD int
		int rxintena=scc.chan[chan].wr1&0x18;
		if (rxintena==0x10 || rxintena==0x08) {
			scc.intpending|=(chan?SCC_RR3_CHA_RX:SCC_RR3_CHB_RX);
			raiseInt(chan);
		}
		scc.chan[chan].hunting=0;
	} else {
		printf("...Not for us, ignoring.\n");
		rxBufIgnoreRest(chan);
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
//			printf("SCC ABORT: Sent data\n");
			sccTxFinished(chan);
		}
		if ((val&0xc0)==0xC0) {
			//reset tx underrun latch
//			sccTxFinished(chan);
			scc.chan[chan].txTimer=1;
			//if (scc.chan[chan].txTimer==0) scc.chan[chan].txTimer=-1;
		}
	} else if (reg==1) {
		scc.chan[chan].wr1=val;
	} else if (reg==2) {
		scc.wr2=val;
	} else if (reg==3) {
		//bitsperchar1, bitsperchar0, autoenables, enterhuntmode, rxcrcen, addresssearch, synccharloadinh, rxena
		//autoenable: cts = tx ena, dcd = rx ena
		if (val&0x10) scc.chan[chan].hunting=1;
	} else if (reg==6) {
		scc.chan[chan].sdlcaddr=val;
	} else if (reg==8) {
		scc.chan[chan].txData[scc.chan[chan].txPos++]=val;
//		printf("TX! Pos %d\n", scc.chan[chan].txPos);
		scc.chan[chan].txTimer+=30;
	} else if (reg==9) {
		scc.wr9=val;
	} else if (reg==15) {
		scc.chan[chan].wr15=val;
		raiseInt(chan);
	}
	printf("SCC: write to addr %x chan %d reg %d val %x\n", addr, chan, reg, val);
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
		if (rxHasByte(chan)) val|=(1<<0);
		//Bit 1 is zero count - never set
		val=(1<<2); //tx buffer always empty
		if (scc.chan[chan].dcd) val|=(1<<3);
		if (scc.chan[chan].hunting) val|=(1<<4);
		if (scc.chan[chan].cts) val|=(1<<5);
		if (scc.chan[chan].txTimer==0) val|=(1<<6);
		if (rxBytesLeft(chan)<=2) val|=(1<<7); //abort
	} else if (reg==1) {
		//Actually, these come out of the same fifo as the data, so this status should be for the fifo
		//val available.
		//EndOfFrame, CRCErr, RXOverrun, ParityErr, Residue0, Residue1, Residue2, AllSent
		val=0x7; //residue code 011, all sent
		if (rxBytesLeft(chan)==1) val|=(1<<7); //end of frame
	} else if (reg==2 && chan==SCC_CHANB) {
		//We assume this also does an intack.
		int rsn=0;
		if (scc.intpending & SCC_RR3_CHB_EXT) {
			rsn=1;
			scc.intpending&=~SCC_RR3_CHB_EXT;
		} else if (scc.intpending & SCC_RR3_CHA_EXT) {
			rsn=5;
			scc.intpending&=~SCC_RR3_CHA_EXT;
		} else if (scc.intpending&SCC_RR3_CHA_RX) {
			rsn=6;
			scc.intpending&=~SCC_RR3_CHA_RX;
		} else if (scc.intpending&SCC_RR3_CHB_RX) {
			rsn=2;
			scc.intpending&=~SCC_RR3_CHA_RX;
		}

		val=scc.wr2;
		if (scc.wr9&0x10) { //hi/lo
			val=(scc.wr2&~0x70)|rsn<<4;
		} else {
			val=(scc.wr2&~0xD)|rsn<<1;
		}
		if (scc.intpending&0x38) raiseInt(SCC_CHANA);
		if (scc.intpending&0x07) raiseInt(SCC_CHANB);
	} else if (reg==3) {
		if (chan==SCC_CHANA) val=scc.intpending; else val=0;
	} else if (reg==8) {
		//rx buffer

		if (rxHasByte(chan)) {
			int left;
			val=rxByte(chan, &left);
			printf("SCC READ val %x, %d bytes left\n", val, left);
			if (left!=0) {
				int rxintena=scc.chan[chan].wr1&0x18;
				if (rxintena==0x10) {
					scc.intpending|=(chan?SCC_RR3_CHA_RX:SCC_RR3_CHB_RX);
					raiseInt(chan);
				}
			}
			if (left==1) scc.chan[chan].hunting=1;
			if (left==1 && scc.chan[chan].wr15&SCC_WR15_BREAK) {
				scc.intpending|=(chan?SCC_RR3_CHA_EXT:SCC_RR3_CHB_EXT);
				raiseInt(chan);
			}
		} else {
			printf("SCC READ but no data?\n");
			val=0;
		}
	} else if (reg==10) {
		//Misc status (mostly SDLC)
		val=0;
	} else if (reg==15) {
		val=scc.chan[chan].wr15;
	}
	printf("SCC: read from chan %d reg %d val %x\n", chan, reg, val);
	return val;
}

//Called at about 800KHz
void sccTick() {
	for (int n=0; n<2; n++) {
		if (scc.chan[n].txTimer>0) {
			scc.chan[n].txTimer--;
			if (scc.chan[n].txTimer==0) {
//				printf("Tx buffer empty: Sent data\n");
				sccTxFinished(n);
			}
		}
		if (rxBufTick(n)) {
			triggerRx(n);
		}
	}
}

void sccInit() {
	sccSetDcd(1, 1);
	sccSetDcd(2, 1);
	scc.chan[0].txTimer=-1;
	scc.chan[1].txTimer=-1;
	for (int x=0; x<NO_RXBUF; x++) {
		scc.chan[0].rx[x].delay=-1;
		scc.chan[1].rx[x].delay=-1;
	}
}


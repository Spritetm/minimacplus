#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "tmeconfig.h"
#include "m68k.h"
#include "disp.h"
#include "iwm.h"
#include "via.h"
#include "scc.h"
#include "rtc.h"
#include "ncr.h"
#include "hd.h"
#include "mouse.h"
#include <stdbool.h>
#include "esp_heap_alloc_caps.h"

unsigned char *macRom;
unsigned char *macRam;

int rom_remap, video_remap=0, audio_remap=0;

void m68k_instruction() {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	printf("Mon: %x\n", pc);
	int ok=0;
	if (pc < 0x400000) {
		if (rom_remap) {
			ok=1;
		}
	} else if (pc >= 0x400000 && pc<0x500000) {
		ok=1;
	}
	if (!ok) return;
	pc&=0x1FFFF;
	if (pc==0x7DCC) printf("Mon: SCSIReadSectors\n");
	if (pc==0x7E4C) printf("Mon: SCSIReadSectors exit OK\n");
	if (pc==0x7E56) printf("Mon: SCSIReadSectors exit FAIL\n");
}

typedef uint8_t (*PeripAccessCb)(unsigned int address, int data, int isWrite);

uint8_t unhandledAccessCb(unsigned int address, int data, int isWrite) {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	printf("Unhandled %s @ 0x%X! PC=0x%X\n", isWrite?"write":"read", address, pc);
	return 0xff;
}

uint8_t bogusReadCb(unsigned int address, int data, int isWrite) {
	if (isWrite) return 0;
	return address^(address>>8)^(address>>16);
}

uint8_t ncrAccessCb(unsigned int address, int data, int isWrite) {
	if (isWrite) {
		ncrWrite((address>>4)&0x7, (address>>9)&1, data);
		return 0;
	} else {
		return ncrRead((address>>4)&0x7, (address>>9)&1);
	}
}

uint8_t sscAccessCb(unsigned int address, int data, int isWrite) {
	if (isWrite) {
		sccWrite(address, data);
		return 0;
	} else {
		return sccRead(address);
	}
}

uint8_t iwmAccessCb(unsigned int address, int data, int isWrite) {
	if (isWrite) {
		iwmWrite((address>>9)&0xf, data);
		return 0;
	} else {
		return iwmRead((address>>9)&0xf);;
	}
}

uint8_t viaAccessCb(unsigned int address, int data, int isWrite) {
	if (isWrite) {
		viaWrite((address>>9)&0xf, data);
		return 0;
	} else {
		return viaRead((address>>9)&0xf);
	}
}


#define FLAG_RO (1<<0);

typedef struct {
	uint8_t *memAddr;
	union {
		PeripAccessCb cb;
		int flags;
	};
} MemmapEnt;

#define MEMMAP_MAX_ADDR 0x1000000
#define MEMMAP_ES 0x20000
//Memmap describing 128 128K blocks of memory, from 0 to 0x1000000.
MemmapEnt memmap[128];

static void regenMemmap(int remapRom) {
	int i;
	//Default handler
	for (i=0; i<128; i++) {
		memmap[i].memAddr=0;
		memmap[i].cb=unhandledAccessCb;
	}

	//0-0x400000 is RAM, or ROM when remapped
	if (remapRom) {
		memmap[0].memAddr=macRom;
		memmap[0].flags=FLAG_RO;
		for (i=1; i<0x400000/MEMMAP_ES; i++) {
			//Do not point at ROM again, but at... something else. Abuse RAM here.
			//If pointed at ROM again, ROM will think this machine does not have SCSI.
			memmap[i].memAddr=NULL;
			memmap[i].cb=bogusReadCb;
		}
	} else {
		for (i=0; i<0x400000/MEMMAP_ES; i++) {
			memmap[i].memAddr=&macRam[(i*MEMMAP_ES)&(TME_RAMSIZE-1)];
			memmap[i].flags=0;
		}
	}
	
	//0x40000-0x50000 is ROM
	memmap[0x400000/MEMMAP_ES].memAddr=macRom;
	memmap[0x400000/MEMMAP_ES].flags=FLAG_RO;
	for (i=0x400000/MEMMAP_ES+1; i<0x500000/MEMMAP_ES; i++) {
		//Again, point to crap or SCSI won't work.
		memmap[i].memAddr=0;
		memmap[i].cb=bogusReadCb;
	}

	//0x580000-0x600000 is SCSI controller
	for (i=0x580000/MEMMAP_ES; i<0x600000/MEMMAP_ES; i++) {
		memmap[i].memAddr=NULL;
		memmap[i].cb=ncrAccessCb;
	}

	//0x600000-0x700000 is RAM
	for (i=0x600000/MEMMAP_ES; i<0x700000/MEMMAP_ES; i++) {
		memmap[i].memAddr=&macRam[(i*MEMMAP_ES)&(TME_RAMSIZE-1)];
		memmap[i].flags=0;
	}

	//0x800000-0xC00000 is SSC
	for (i=0x800000/MEMMAP_ES; i<0xC00000/MEMMAP_ES; i++) {
		memmap[i].memAddr=NULL;
		memmap[i].cb=sscAccessCb;
	}

	//0xC00000-0xE00000 is IWM
	for (i=0xc00000/MEMMAP_ES; i<0xe00000/MEMMAP_ES; i++) {
		memmap[i].memAddr=NULL;
		memmap[i].cb=iwmAccessCb;
	}
	//0xE80000-0xF00000 is VIA
	for (i=0xE80000/MEMMAP_ES; i<0xF00000/MEMMAP_ES; i++) {
		memmap[i].memAddr=NULL;
		memmap[i].cb=viaAccessCb;
	}
}

const inline static MemmapEnt *getMmmapEnt(const unsigned int address) {
	if (address>=MEMMAP_MAX_ADDR) return &memmap[127];
	return &memmap[address/MEMMAP_ES];
}

unsigned int  m68k_read_memory_8(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		return mmEnt->memAddr[address&(MEMMAP_ES-1)];
	} else {
		return mmEnt->cb(address, 0, 0);
	}
}

unsigned int  m68k_read_memory_16(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p=&mmEnt->memAddr[address&(MEMMAP_ES-1)];
		return (p[0]<<8)|(p[1]);
	} else {
		unsigned int ret;
		ret=mmEnt->cb(address, 0, 0)<<8;
		ret|=mmEnt->cb(address+1, 0, 0);
		return ret;
	}
}

unsigned int  m68k_read_memory_32(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p=&mmEnt->memAddr[address&(MEMMAP_ES-1)];
		return (p[0]<<24)|(p[1]<<16)|(p[2]<<8)|(p[3]);
	} else {
		unsigned int ret;
		ret=mmEnt->cb(address, 0, 0)<<24;
		ret|=mmEnt->cb(address+1, 0, 0)<<16;
		ret|=mmEnt->cb(address+2, 0, 0)<<8;
		ret|=mmEnt->cb(address+3, 0, 0)<<0;
		return ret;
	}
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		mmEnt->memAddr[address&(MEMMAP_ES-1)]=value;
	} else {
		mmEnt->cb(address, value, 1);
	}
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p=&mmEnt->memAddr[address&(MEMMAP_ES-1)];
		p[0]=(value>>8);
		p[1]=(value>>0);
	} else {
		mmEnt->cb(address, (value>>8)&0xff, 1);
		mmEnt->cb(address+1, (value>>0)&0xff, 1);
	}
}


void m68k_write_memory_32(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p=&mmEnt->memAddr[address&(MEMMAP_ES-1)];
		p[0]=(value>>24);
		p[1]=(value>>16);
		p[2]=(value>>8);
		p[3]=(value>>0);
	} else {
		mmEnt->cb(address, (value>>24)&0xff, 1);
		mmEnt->cb(address+1, (value>>16)&0xff, 1);
		mmEnt->cb(address+2, (value>>8)&0xff, 1);
		mmEnt->cb(address+3, (value>>0)&0xff, 1);
	}
}


//Should be called every second. 
void printFps() {
	struct timeval tv;
	static struct timeval oldtv;
	gettimeofday(&tv, NULL);
	if (oldtv.tv_sec!=0) {
		long msec=(tv.tv_sec-oldtv.tv_sec)*1000;
		msec+=(tv.tv_usec-oldtv.tv_usec)/1000;
		printf("Speed: %d%%\n", (int)(100000/msec));
		printf("Mem free: %dKiB 8-bit, %dKiB 32-bit\n", xPortGetFreeHeapSizeCaps(MALLOC_CAP_8BIT)/1024, xPortGetFreeHeapSizeCaps(MALLOC_CAP_32BIT)/1024);
	}
	oldtv.tv_sec=tv.tv_sec;
	oldtv.tv_usec=tv.tv_usec;
}

void tmeStartEmu(void *ram, void *rom) {
	int ca1=0, ca2=0;
	int x, m=0, frame=0;
	macRom=rom;
	macRam=ram;
	printf("Clearing ram...\n");
	for (int x=0; x<TME_RAMSIZE; x++) macRam[x]=0;
	rom_remap=1;
	regenMemmap(1);
	printf("Creating HD and registering it...\n");
	SCSIDevice *hd=hdCreate("hd.img");
	ncrRegisterDevice(6, hd);
	printf("Initializing m68k...\n");
	viaClear(VIA_PORTA, 0x7F);
	viaSet(VIA_PORTA, 0x80);
	viaClear(VIA_PORTA, 0xFF);
	viaSet(VIA_PORTB, (1<<3));
	printf("Initializing m68k...\n");
	m68k_init();
	printf("Setting CPU type and resetting...");
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_pulse_reset();
	printf("Display init...\n");
	dispInit();
	printf("Done! Running.\n");
	while(1) {
		for (x=0; x<8000000/60; x+=10) {
			m68k_execute(10);
			viaStep(1); //should run at 783.36KHz
			m++;
			if (m>=1000) {
				int r=mouseTick();
				if (r&MOUSE_BTN) viaClear(VIA_PORTB, (1<<3)); else viaSet(VIA_PORTB, (1<<3));
				if (r&MOUSE_QXB) viaClear(VIA_PORTB, (1<<4)); else viaSet(VIA_PORTB, (1<<4));
				if (r&MOUSE_QYB) viaClear(VIA_PORTB, (1<<5)); else viaSet(VIA_PORTB, (1<<5));
				sccSetDcd(SCC_CHANA, r&MOUSE_QXA);
				sccSetDcd(SCC_CHANB, r&MOUSE_QYA);
				m=0;
			}
		}
		dispDraw(&macRam[video_remap?TME_SCREENBUF_ALT:TME_SCREENBUF]);
		frame++;
		ca1^=1;
		viaControlWrite(VIA_CA1, ca1);
		if (frame>=60) {
			ca2^=1;
			viaControlWrite(VIA_CA2, ca2);
			rtcTick();
			frame=0;
			printFps();
		}
	}
}

void viaIrq(int req) {
//	printf("IRQ %d\n", req);
	m68k_set_irq(req?1:0);
}

void sccIrq(int req) {
//	printf("IRQ %d\n", req);
	m68k_set_irq(req?2:0);
}


void viaCbPortAWrite(unsigned int val) {
	int oldRomRemap=rom_remap;
	video_remap=(val&(1<<6))?1:0;
	rom_remap=(val&(1<<4))?1:0;
	regenMemmap(rom_remap);
	audio_remap=(val&(1<<3))?1:0;
	if (oldRomRemap!=rom_remap) printf("ROM REMAP %d\n", rom_remap);
	iwmSetHeadSel(val&(1<<5));
}

void viaCbPortBWrite(unsigned int val) {
	int b;
	b=rtcCom(val&4, val&1, val&2);
	if (b) viaSet(VIA_PORTB, 1); else viaClear(VIA_PORTB, 1);
}


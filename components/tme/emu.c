#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include "tmeconfig.h"
#include "m68k.h"
#include "disp.h"
#include "iwm.h"
#include "via.h"
#include "scc.h"
#include "rtc.h"
#include "ncr.h"
#include "hd.h"
#include "snd.h"
#include "mouse.h"
#include <stdbool.h>
#include "esp_heap_caps.h"
#include <byteswap.h>
#include "esp_spiram.h"
#include "network/localtalk.h"


unsigned char *macRom;

#if (TME_CACHESIZE!=0)
#define USE_EXTERNAL_RAM 1
#else
#define USE_EXTERNAL_RAM 0
unsigned char *macRam;
#endif

#define MEMADDR_DUMMY_CACHE (void*)1

int rom_remap, video_remap=0, audio_remap=0, audio_volume=0, audio_en=0;

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

#define MEMMAP_ES 0x20000 //entry size
#define MEMMAP_MAX_ADDR 0x1000000
//Memmap describing 128 128K blocks of memory, from 0 to 0x1000000 (16MiB).
MemmapEnt memmap[MEMMAP_MAX_ADDR/MEMMAP_ES];

static void regenMemmap(int remapRom) {
	int i;
	//Default handler
	for (i=0; i<MEMMAP_MAX_ADDR/MEMMAP_ES; i++) {
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
#if USE_EXTERNAL_RAM
			memmap[i].memAddr=MEMADDR_DUMMY_CACHE;
#else
			memmap[i].memAddr=&macRam[(i*MEMMAP_ES)&(TME_RAMSIZE-1)];
#endif
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
#if USE_EXTERNAL_RAM
		memmap[i].memAddr=MEMADDR_DUMMY_CACHE;
#else
		memmap[i].memAddr=&macRam[(i*MEMMAP_ES)&(TME_RAMSIZE-1)];
#endif
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

uint8_t *macFb[2], *macSnd[2];


#if (USE_EXTERNAL_RAM)

//Keep these things powers-of-2 please.
#define CACHESIZE TME_CACHESIZE
#define CACHEITEMSIZE (1*1024)
#define CACHEENTCNT ((TME_RAMSIZE)/CACHEITEMSIZE)
#define CACHESLOTCNT (CACHESIZE/CACHEITEMSIZE)

#define FBSLOTCNT ((22*1024+(CACHEITEMSIZE-1))/CACHEITEMSIZE)


typedef struct {
	uint8_t *mem;
	int ent;
} CacheSlot;

#define NO_ENT 0xFF

static uint8_t cacheEnt[CACHEENTCNT];
static CacheSlot cacheSlot[CACHESLOTCNT+FBSLOTCNT*2];

static int cacheSwapPos=0;

#define MMAP_RAM_PTR(ent, addr) ((ent->memAddr==MEMADDR_DUMMY_CACHE)?getRamPtr(addr&(TME_RAMSIZE-1)):&ent->memAddr[addr&(MEMMAP_ES-1)])

/*
Warning: This malfunctions if e.g. a 32-bit val starting at an address [1-3] from the end of the region is requested.
Luckily, on the 68000 itself this leads to an exception and should never happen.
*/
static inline uint8_t *getRamPtr(const unsigned int address) {
	assert(address<TME_RAMSIZE);
	uint16_t slot=cacheEnt[address/CACHEITEMSIZE];
	if (slot==NO_ENT) {
		//Invalid entry. Find oldest entry, swap to RAM, load this entry, return ptr.
		//We use a stupid round-robin exchange thing for killing old pages for now... ToDo: make more intelligent.
		cacheSwapPos++;
		if (cacheSwapPos>=CACHESLOTCNT) cacheSwapPos=0;
		slot=cacheSwapPos;
		//Write old data.
		int oldaddr=cacheSlot[slot].ent*CACHEITEMSIZE;
		esp_spiram_write(oldaddr, cacheSlot[slot].mem, CACHEITEMSIZE);
		cacheEnt[cacheSlot[slot].ent]=NO_ENT;
		//Read new data.
		cacheSlot[slot].ent=address/CACHEITEMSIZE;
		int newaddr=cacheSlot[slot].ent*CACHEITEMSIZE;;
		esp_spiram_read(newaddr, cacheSlot[slot].mem, CACHEITEMSIZE);
		cacheEnt[address/CACHEITEMSIZE]=slot;
//		printf("CACHE SWAPOUT: slot %d address %x -> address %x\n", slot, oldaddr, newaddr);
	}
	return cacheSlot[slot].mem+(address&(CACHEITEMSIZE-1));
}

static void ramInit() {
	printf("Using external SPI memory as Mac RAM\n");

#if 1
	char obuf[128], ibuf[128];
	for (int i=0; i<128; i++) obuf[i]=rand();
	esp_spiram_write(0, obuf, 128);
	esp_spiram_read(0, ibuf, 128);
	if (memcmp(obuf, ibuf, 128)!=0) {
		printf("Error: External SPI ram is not stable.\n");
		abort();
	}
#endif

	for (int x=0; x<CACHEENTCNT; x++) {
		cacheEnt[x]=NO_ENT;
	}
	//Initialize the cache to point to the first few slots of memory.
	for (int x=0; x<CACHESLOTCNT; x++) {
		cacheEnt[x]=x;
		cacheSlot[x].ent=x;
		cacheSlot[x].mem=malloc(CACHEITEMSIZE);
		if (!cacheSlot[x].mem) {
			printf("Could not allocate memory for cache slot %d\n", x);
			abort();
		}
		memset(cacheSlot[x].mem, 0, CACHEITEMSIZE);
	}

	//Framebuffer is dedicated memory. Allocate and set in cache set.
	int sz=FBSLOTCNT*CACHEITEMSIZE;
	macFb[0]=malloc(sz);
	macFb[1]=malloc(sz);
	if (!macFb[0] || !macFb[1]) {
		printf("Couldn't allocate framebuffer memory!\n");
		abort();
	}
	memset(macFb[0], 0xF0, sz);
	memset(macFb[1], 0x0F, sz);
	for (int i=0; i<FBSLOTCNT; i++) {
		cacheSlot[CACHESLOTCNT+i].mem=macFb[0]+i*CACHEITEMSIZE;
		cacheEnt[(TME_SCREENBUF/CACHEITEMSIZE)+i]=CACHESLOTCNT+i;
		cacheSlot[CACHESLOTCNT+FBSLOTCNT+i].mem=macFb[1]+i*CACHEITEMSIZE;
		cacheEnt[(TME_SCREENBUF_ALT/CACHEITEMSIZE)+i]=CACHESLOTCNT+FBSLOTCNT+i;
	}
	//Fbs probably are aligned with cache page size, but de-aligned with visibe image. Fix that.
	macFb[0]=getRamPtr(TME_SCREENBUF);
	macFb[1]=getRamPtr(TME_SCREENBUF_ALT);

#if 0
	printf("Doing mem/cache test\n");
	srand(0);
	for (int i=0; i<TME_RAMSIZE; i+=4) {
		uint32_t *p=(uint32_t*)getRamPtr(i^0x25A500);
		*p=rand();
	}
	
	printf("Readback...\n");
	srand(0);
	for (int i=0; i<TME_RAMSIZE; i+=4) {
		uint32_t *p=(uint32_t*)getRamPtr(i^0x25A500);
		uint32_t ex=rand();
		if (*p!=ex) {
			printf("Error!= Addr %x expected %x got %x\n", i, ex, *p);
		}
		*p=0;
	}
#endif
}

#else //!USE_EXTERNAL_RAM
#define MMAP_RAM_PTR(ent, addr) &ent->memAddr[addr&(MEMMAP_ES-1)]
static void ramInit() {
	printf("Using internal (or hw cached) memory as Mac RAM\n");
#if CONFIG_SPIRAM_USE_MEMMAP
	macRam=(void*)0x3F800000;
#else
	macRam=malloc(TME_RAMSIZE);
#endif
	assert(macRam);
	macFb[0]=&macRam[TME_SCREENBUF];
	macFb[1]=&macRam[TME_SCREENBUF_ALT];
	macSnd[0]=&macRam[TME_SNDBUF];
	macSnd[1]=&macRam[TME_SNDBUF_ALT];
	printf("Clearing ram...\n");
	for (int x=0; x<TME_RAMSIZE; x++) macRam[x]=rand();
}
#endif


const inline static MemmapEnt *getMmmapEnt(const unsigned int address) {
	if (address>=MEMMAP_MAX_ADDR) return &memmap[127];
	return &memmap[address/MEMMAP_ES];
}

unsigned int m68k_read_memory_8(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p;
		p=(uint8_t*)MMAP_RAM_PTR(mmEnt, address);
		return *p;
	} else {
		return mmEnt->cb(address, 0, 0);
	}
}

unsigned int m68k_read_memory_16(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if ((address&1)!=0) printf("%s: Unaligned access to %x!\n", __FUNCTION__, address);
	if (mmEnt->memAddr) {
		uint16_t *p;
		p=(uint16_t*)MMAP_RAM_PTR(mmEnt, address);
		return __bswap_16(*p);
	} else {
		unsigned int ret;
		ret=mmEnt->cb(address, 0, 0)<<8;
		ret|=mmEnt->cb(address+1, 0, 0);
		return ret;
	}
}

#if 0
unsigned int m68k_read_memory_32(unsigned int address) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if ((address&3)!=0) printf("%s: Unaligned access to %x!\n", __FUNCTION__, address);
	if (mmEnt->memAddr) {
		uint32_t *p;
		p=(uint32_t*)MMAP_RAM_PTR(mmEnt, address);
		return __bswap_32(*p);
	} else {
		unsigned int ret;
		ret=mmEnt->cb(address, 0, 0)<<24;
		ret|=mmEnt->cb(address+1, 0, 0)<<16;
		ret|=mmEnt->cb(address+2, 0, 0)<<8;
		ret|=mmEnt->cb(address+3, 0, 0)<<0;
		return ret;
	}
}
#else
unsigned int m68k_read_memory_32(unsigned int address) {
	uint16_t a=m68k_read_memory_16(address);
	uint16_t b=m68k_read_memory_16(address+2);
	return (a<<16)|b;
}
#endif

void m68k_write_memory_8(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if (mmEnt->memAddr) {
		uint8_t *p;
		p=(uint8_t*)MMAP_RAM_PTR(mmEnt, address);
		*p=value;
	} else {
		mmEnt->cb(address, value, 1);
	}
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if ((address&1)!=0) printf("%s: Unaligned access to %x!\n", __FUNCTION__, address);
	if (mmEnt->memAddr) {
		uint16_t *p;
		p=(uint16_t*)MMAP_RAM_PTR(mmEnt, address);
		*p=__bswap_16(value);
	} else {
		mmEnt->cb(address, (value>>8)&0xff, 1);
		mmEnt->cb(address+1, (value>>0)&0xff, 1);
	}
}

#if 0
void m68k_write_memory_32(unsigned int address, unsigned int value) {
	const MemmapEnt *mmEnt=getMmmapEnt(address);
	if ((address&3)!=0) printf("%s: Unaligned access to %x!\n", __FUNCTION__, address);
	if (mmEnt->memAddr) {
		uint32_t *p;
		p=(uint32_t*)MMAP_RAM_PTR(mmEnt, address);
		*p=__bswap_32(value);
	} else {
		mmEnt->cb(address, (value>>24)&0xff, 1);
		mmEnt->cb(address+1, (value>>16)&0xff, 1);
		mmEnt->cb(address+2, (value>>8)&0xff, 1);
		mmEnt->cb(address+3, (value>>0)&0xff, 1);
	}
}
#else
void m68k_write_memory_32(unsigned int address, unsigned int value) {
	m68k_write_memory_16(address, value>>16);
	m68k_write_memory_16(address+2, value);
}
#endif

//Should be called every second. 
void printFps() {
	struct timeval tv;
	static struct timeval oldtv;
	gettimeofday(&tv, NULL);
	if (oldtv.tv_sec!=0) {
		long msec=(tv.tv_sec-oldtv.tv_sec)*1000;
		msec+=(tv.tv_usec-oldtv.tv_usec)/1000;
//		printf("Speed: %d%%\n", (int)(100000/msec));
//		printf("Mem free: %dKiB 8-bit, %dKiB 32-bit\n", xPortGetFreeHeapSizeCaps(MALLOC_CAP_8BIT)/1024, xPortGetFreeHeapSizeCaps(MALLOC_CAP_32BIT)/1024);
	}
	oldtv.tv_sec=tv.tv_sec;
	oldtv.tv_usec=tv.tv_usec;
}

void tmeStartEmu(void *rom) {
	int ca1=0, ca2=0;
	int x, m=0, frame=0;
	macRom=rom;
	ramInit();
	rom_remap=1;
	regenMemmap(1);
	printf("Creating HD and registering it...\n");
	SCSIDevice *hd=hdCreate("hd.img");
	ncrRegisterDevice(6, hd);
	viaClear(VIA_PORTA, 0x7F);
	viaSet(VIA_PORTA, 0x80);
	viaClear(VIA_PORTA, 0xFF);
	viaSet(VIA_PORTB, (1<<3));
	sccInit();
	printf("Initializing m68k...\n");
	m68k_init();
	printf("Setting CPU type and resetting...");
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_pulse_reset();
	printf("Display init...\n");
	sndInit();
	dispInit();
	localtalkInit();
	printf("Done! Running.\n");
	while(1) {
		for (x=0; x<8000000/60; x+=10) {
			m68k_execute(10);
			viaStep(1); //should run at 783.36KHz
			sccTick();
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
			//Sound handler keeps track of real time, if its buffer is empty we should be done with the video frame.
			//if (sndDone()) break;
		}
		dispDraw(macFb[video_remap?1:0]);
		sndPush(macSnd[audio_remap?1:0], audio_en?audio_volume:0);
		localtalkTick();
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
	audio_remap=(val&(1<<3))?0:1;
	if (oldRomRemap!=rom_remap) printf("ROM REMAP %d\n", rom_remap);
	iwmSetHeadSel(val&(1<<5));
	audio_volume=(val&7);
}

void viaCbPortBWrite(unsigned int val) {
	int b;
	b=rtcCom(val&4, val&1, val&2);
	if (b) viaSet(VIA_PORTB, 1); else viaClear(VIA_PORTB, 1);
	audio_en=!(val&(1<<7));
}


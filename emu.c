#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include "config.h"
#include "m68k.h"
#include "disp.h"
#include "iwm.h"
#include "via.h"
#include "rtc.h"
#include "ncr.h"
#include "hd.h"

unsigned char *macRom;
unsigned char *macRam;

int rom_remap, video_remap=0, audio_remap=0;

unsigned int  m68k_read_memory_8(unsigned int address) {
	unsigned int ret;
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (address < 0x400000) {
		if (rom_remap) {
			ret=macRom[address & (TME_ROMSIZE-1)];
		} else {
			ret=macRam[address & (TME_RAMSIZE-1)];
		}
	} else if (address >= 0x600000 && address < 0xA00000) {
		ret=macRam[(address-0x600000) & (TME_RAMSIZE-1)];
	} else if (address >= 0x400000 && address<0x500000) {
		int romAdr=address-0x400000;
		if (romAdr>=TME_ROMSIZE) {
			printf("PC %x:Huh? Read from ROM mirror (%x)\n", pc, address);
		} else {
			ret=macRom[romAdr&(TME_ROMSIZE-1)];
		}
	} else if (address >= 0xE80000 && address < 0xf00000) {
		ret=viaRead((address>>9)&0xf);
	} else if (address >= 0xc00000 && address < 0xe00000) {
		ret=iwmRead((address>>9)&0xf);
	} else if (address >= 0x580000 && address < 0x600000) {
		ret=ncrRead((address>>4)&0x7, (address>>7)&1);
	} else {
		printf("PC %x: Read from %x\n", pc, address);
		ret=0xff;
	}
//	printf("Rd %x = %x\n", address, ret);
	return ret;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (address < 0x400000 && !rom_remap) {
		macRam[address & (TME_RAMSIZE-1)]=value;
	} else if (address >= 0x600000 && address < 0xA00000) {
		macRam[(address-0x600000) & (TME_RAMSIZE-1)]=value;
	} else if (address >= 0xE80000 && address < 0xf00000) {
		viaWrite((address>>9)&0xf, value);
	} else if (address >= 0xc00000 && address < 0xe00000) {
		iwmWrite((address>>9)&0xf, value);
	} else if (address >= 0x580000 && address < 0x600000) {
		ncrWrite((address>>4)&0x7, (address>>7)&1, value);
	} else {
		printf("PC %x: Write to %x: %x\n", pc, address, value);
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
		printf("Speed: %d%%\n", 100000/msec);
	}
	oldtv.tv_sec=tv.tv_sec;
	oldtv.tv_usec=tv.tv_usec;
}

#define GRAN 100

void tmeStartEmu(void *rom) {
	int ca1=0, ca2=0;
	int x, frame=0;
	macRom=rom;
	macRam=malloc(TME_RAMSIZE);
	for (int x=0; x<TME_RAMSIZE; x++) macRam[x]=0xaa;
	rom_remap=1;
	SCSIDevice *hd=hdCreate("hd.img");
	ncrRegisterDevice(6, hd);
	viaClear(VIA_PORTA, 0x7F);
	viaSet(VIA_PORTA, 0x80);
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_pulse_reset();
	dispInit();
	while(1) {
		for (x=0; x<8000000/60; x+=GRAN) {
			m68k_execute(GRAN);
			viaStep(GRAN);
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

//Mac uses an 68008, which has an external 16-bit bus. Hence, it should be okay to do everything using 16-bit
//reads/writes.
unsigned int  m68k_read_memory_32(unsigned int address) {
	unsigned int ret;
	ret=m68k_read_memory_16(address)<<16;
	ret|=m68k_read_memory_16(address+2);
	return ret;
}

unsigned int  m68k_read_memory_16(unsigned int address) {
	unsigned int ret;
	ret=m68k_read_memory_8(address)<<8;
	ret|=m68k_read_memory_8(address+1);
	return ret;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
	m68k_write_memory_16(address, value>>16);
	m68k_write_memory_16(address+2, value);
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
	m68k_write_memory_8(address, (value>>8)&0xff);
	m68k_write_memory_8(address+1, value&0xff);
}

void m68k_int_ack(int irq) {
	//Mac has level interrupts; no ack. Fake by raising the irq as soon as
	//it's serviced.
	m68k_set_irq(irq);
}

void viaCbPortAWrite(unsigned int val) {
	video_remap=(val&(1<<6))?1:0;
	rom_remap=(val&(1<<4))?1:0;
	audio_remap=(val&(1<<3))?1:0;
}

void viaCbPortBWrite(unsigned int val) {
	int b;
	b=rtcCom(val&4, val&1, val&2);
	if (b) viaSet(VIA_PORTB, 1); else viaClear(VIA_PORTB, 1);
}


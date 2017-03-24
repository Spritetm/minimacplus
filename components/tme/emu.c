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


unsigned int  m68k_read_memory_8(unsigned int address) {
	unsigned int ret;
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (address < 0x400000) {
		if (rom_remap) {
			ret=macRom[address & (TME_ROMSIZE-1)];
		} else {
			ret=macRam[address & (TME_RAMSIZE-1)];
		}
	} else if (address >= 0x600000 && address < 0x700000) {
		ret=macRam[(address-0x600000) & (TME_RAMSIZE-1)];
	} else if (address >= 0x400000 && address<0x500000) {
		int romAdr=address-0x400000;
		if (romAdr>=TME_ROMSIZE) {
			printf("PC %x:Huh? Read from ROM mirror (%x)\n", pc, address);
			ret=(address>>12); // ROM checks for same contents at 20000 and 40000 to determine if SCSI is present
		} else {
			ret=macRom[romAdr&(TME_ROMSIZE-1)];
		}
	} else if (address >= 0xE80000 && address < 0xf00000) {
		ret=viaRead((address>>9)&0xf);
	} else if (address >= 0xc00000 && address < 0xe00000) {
		ret=iwmRead((address>>9)&0xf);
	} else if (address >= 0x580000 && address < 0x600000) {
		ret=ncrRead((address>>4)&0x7, (address>>9)&1);
	} else if (address >= 0x800000 && address < 0xC00000) {
		ret=sccRead(address);
	} else {
		printf("PC %x: Read from %x\n", pc, address);
		ret=0xff;
	}
//	printf("Rd %x = %x\n", address, ret);
	return ret;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (address < 0x400000) {
		if (!rom_remap) macRam[address & (TME_RAMSIZE-1)]=value;
	} else if (address >= 0x600000 && address < 0xA00000) {
		macRam[(address-0x600000) & (TME_RAMSIZE-1)]=value;
	} else if (address >= 0xE80000 && address < 0xf00000) {
		viaWrite((address>>9)&0xf, value);
	} else if (address >= 0xc00000 && address < 0xe00000) {
		iwmWrite((address>>9)&0xf, value);
	} else if (address >= 0x580000 && address < 0x600000) {
		ncrWrite((address>>4)&0x7, (address>>9)&1, value);
	} else if (address >= 0x800000 && address < 0xC00000) {
		sccWrite(address, value);
		printf("PC %x: Write to %x: %x\n", pc, address, value);
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
		printf("Speed: %d%%\n", (int)(100000/msec));
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

void viaCbPortAWrite(unsigned int val) {
	int oldRomRemap=rom_remap;
	video_remap=(val&(1<<6))?1:0;
	rom_remap=(val&(1<<4))?1:0;
	audio_remap=(val&(1<<3))?1:0;
	if (oldRomRemap!=rom_remap) printf("ROM REMAP %d\n", rom_remap);
	iwmSetHeadSel(val&(1<<5));
}

void viaCbPortBWrite(unsigned int val) {
	int b;
	b=rtcCom(val&4, val&1, val&2);
	if (b) viaSet(VIA_PORTB, 1); else viaClear(VIA_PORTB, 1);
}


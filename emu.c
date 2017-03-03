#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "config.h"
#include "m68k.h"
#include "disp.h"

unsigned char *macRom;
unsigned char *macRam;

int rom_remap;

unsigned int  m68k_read_memory_8(unsigned int address) {
	unsigned int ret;
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (rom_remap && address < 0x400000) {
		ret=macRom[address & (TME_ROMSIZE-1)];
	} else if (rom_remap && address > 0x600000 && address <= 0xA00000) {
		ret=macRam[address & (TME_RAMSIZE-1)];
	} else if (address < 0x400000) {
		ret=macRam[address & (TME_RAMSIZE-1)];
	} else if (address>0x400000 && address<0x41FFFF) {
		int romAdr=address-0x400000;
		if (romAdr>TME_ROMSIZE) printf("PC %x:Huh? Read from ROM mirror (%x)\n", pc, address);
		ret=macRom[romAdr&(TME_ROMSIZE-1)];
//		rom_remap=0; //HACK
	} else {
		printf("PC %x: Read from %x\n", pc, address);
		ret=rand();
	}
//	printf("Rd %x = %x\n", address, ret);
	return ret;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	if (address < 0x400000) {
		macRam[address & (TME_RAMSIZE-1)]=value;
	} else if (rom_remap && address > 0x600000 && address <= 0xA00000) {
		macRam[address & (TME_RAMSIZE-1)]=value;
	} else {
		printf("PC %x: Write to %x: %x\n", pc, address, value);
	}
}


void tmeStartEmu(void *rom) {
	macRom=rom;
	macRam=malloc(TME_RAMSIZE);
	for (int x=0; x<TME_RAMSIZE; x++) macRam[x]=x;
	rom_remap=1;
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	m68k_pulse_reset();
	dispInit();
	while(1) {
		m68k_execute(8000000/60);
		dispDraw(&macRam[0x1A700]);
		printf("Int!\n");
//		m68k_set_irq(2);
	}
}


//Mac uses an 68008, which has an external 8-bit bus. Hence, it should be okay to do everything using 8-bit
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

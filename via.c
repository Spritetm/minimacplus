#include <stdio.h>
#include <stdint.h>
#include "via.h"
#include "m68k.h"

void viaCbPortAWrite(unsigned int val);
void viaCbPortBWrite(unsigned int val);



typedef struct {
	uint8_t ddra;
	uint8_t ddrb;
} Via;

static Via via;

void viaWrite(unsigned int addr, unsigned int val) {
	if (addr==0x0) {
		//ORB
	} else if (addr==0x1) {
		//ORA
		viaCbPortAWrite(val);
	} else if (addr==0x2) {
		//DDRB
		via.ddrb=val;
	} else if (addr==0x3) {
		//DDRA
		via.ddra=val;
	} else if (addr==0x4) {
		//T1L-L
	} else if (addr==0x5) {
		//T1C-H
	} else if (addr==0x6) {
		//T1L-L
	} else if (addr==0x7) {
		//T1L-H
	} else if (addr==0x8) {
		//T2L-l
	} else if (addr==0x9) {
		//T2C-H
	} else if (addr==0xa) {
		//SR
	} else if (addr==0xb) {
		//ACR
	} else if (addr==0xc) {
		//PCR
	} else if (addr==0xd) {
		//IFR
	} else if (addr==0xe) {
		//IER
	} else if (addr==0xf) {
		//ORA
	}
	printf("VIA write %x val %x\n", addr, val);
}


unsigned int viaRead(unsigned int addr) {
	unsigned int pc=m68k_get_reg(NULL, M68K_REG_PC);
	unsigned int val=0;
	if (addr==0x0) {
		//ORB
	} else if (addr==0x1) {
		//ORA
	} else if (addr==0x2) {
		//DDRB
		val=via.ddrb;
	} else if (addr==0x3) {
		//DDRA
		val=via.ddra;
	} else if (addr==0x4) {
		//T1L-L
	} else if (addr==0x5) {
		//T1C-H
	} else if (addr==0x6) {
		//T1L-L
	} else if (addr==0x7) {
		//T1L-H
	} else if (addr==0x8) {
		//T2L-l
	} else if (addr==0x9) {
		//T2C-H
	} else if (addr==0xa) {
		//SR
	} else if (addr==0xb) {
		//ACR
	} else if (addr==0xc) {
		//PCR
	} else if (addr==0xd) {
		//IFR
	} else if (addr==0xe) {
		//IER
	} else if (addr==0xf) {
		//ORA
	}
	printf("PC %x VIA read %x val %x\n", pc, addr, val);
	return val;
}

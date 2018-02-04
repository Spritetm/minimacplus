/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <alloca.h>
#include <stdlib.h>
#include <string.h>
#include <rom/crc.h>
#include "crc16-ccitt.h"
#include "mipi.h"

#define NO_CRC 1

//Reminder; MIPI is very LSB-first.
typedef struct {
	uint8_t sot; //should be 0xB8
	uint8_t datatype;
	uint16_t wordcount;
	uint8_t ecc; //0
	uint8_t data[];
	//Footer is 2 bytes of checksum.
} __attribute__((packed)) DsiLPHdr;


typedef struct {
	uint8_t sot; //should be 0xB8
	uint8_t datatype;
	uint8_t data[];
	//Footer is 1 byte of ECC
} __attribute__((packed)) DsiSPHdr;


// calc XOR parity bit of all '1's in 24 bit value
static char parity (uint32_t val) { 
	uint32_t i,p;
	p=0;
	for(i=0; i!=24; i++) {
		p^=val;
		val>>=1;
	}
	return (p&1);
}

static uint8_t calc_ecc(uint8_t *buf) {
	int ret=0;
	uint32_t cmd=(buf[0])+(buf[1]<<8)+(buf[2]<<16);
	if(parity(cmd & 0b111100010010110010110111)) ret|=0x01;
	if(parity(cmd & 0b111100100101010101011011)) ret|=0x02;
	if(parity(cmd & 0b011101001001101001101101)) ret|=0x04;
	if(parity(cmd & 0b101110001110001110001110)) ret|=0x08;
	if(parity(cmd & 0b110111110000001111110000)) ret|=0x10;
	if(parity(cmd & 0b111011111111110000000000)) ret|=0x20;
	return ret;
}

static uint16_t mipiword(uint16_t val) {
	uint16_t ret;
	uint8_t *pret=(uint8_t*)&ret;
	pret[0]=(val&0xff);
	pret[1]=(val>>8);
	return ret;
}

//Warning: CRC isn't tested (my display does not use it)
void mipiDsiSendLong(int type, uint8_t *data, int len) {
	DsiLPHdr p;
	uint8_t footer[3];
	uint8_t *datas[3]={(uint8_t*)&p, data, footer};
	int lengths[3]={sizeof(DsiLPHdr), len, sizeof(footer)};

	p.sot=0xB8;
	p.datatype=type;
	p.wordcount=mipiword(len);
	p.ecc=calc_ecc((uint8_t*)&p.datatype);
#if NO_CRC
	int crc=0;
#else
	int crc=crc16_ccitt(0xFFFF, data, len);
#endif
	footer[0]=(crc&0xff);
	footer[1]=(crc>>8);
	footer[2]=(crc&0x8000)?0:0xff; //need one last level transition at end
	mipiSendMultiple(datas, lengths, 3);
}

void mipiDsiSendShort(int type, uint8_t *data, int len) {
	DsiSPHdr *p=alloca(sizeof(DsiSPHdr)+2+len);
	p->sot=0xB8;
	p->datatype=type;
	memcpy(p->data, data, len);
	p->data[len]=calc_ecc((uint8_t*)&p->datatype);
	p->data[len+1]=(p->data[len]&0x80)?0:0xff; //need one last level transition at end
	mipiSend((uint8_t*)p, sizeof(DsiSPHdr)+2+len);
}

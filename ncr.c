#include <stdint.h>
#include <stdio.h>


static const char* const regNamesR[]={
	"CURSCSIDATA","INITIATORCMD", "MODE", "TARGETCMD", "CURSCSISTATUS",
	"BUSANDSTATUS", "INPUTDATA", "RESETPARINT"
};

static const char* const regNamesW[]={
	"OUTDATA","INITIATORCMD", "MODE", "TARGETCMD", "SELECTENA",
	"STARTDMASEND", "STARTDMATARRECV", "STARTDMAINITRECV"
};


unsigned int ncrRead(unsigned int addr, unsigned int dack) {
	unsigned int ret=0;

	printf("SCSI: read %s val %x (dack %d)\n", regNamesR[addr], ret, dack);
	return ret;
}


void ncrWrite(unsigned int addr, unsigned int dack, unsigned int val) {
	
	printf("SCSI: write %s val %x (dack %d)\n", regNamesW[addr], val, dack);
}

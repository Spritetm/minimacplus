#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ncr.h"
#include "hd.h"

typedef struct {
	FILE *f;
	int size;
} HdPriv;

const uint8_t inq_resp[95]={
	0, //HD
	0, //0x80 if removable
	0x49, //Obsolete SCSI standard 1 all the way
	0, //response version etc
	31, //extra data
	0,0, //reserved
	0, //features
	'A','P','P','L','E',' ',' ',' ', //vendor id
	'2','0','S','C',' ',' ',' ',' ', //prod id
	'1','.','0',' ',' ',' ',' ',' ', //prod rev lvl
};

static int hdScsiCmd(SCSITransferData *data, unsigned int cmd, unsigned int len, unsigned int lba, void *arg) {
	int ret=0;
	HdPriv *hd=(HdPriv*)arg;
//	for (int x=0; x<32; x++) printf("%02X ", data->cmd[x]);
//	printf("\n");
	if (cmd==0x8 || cmd==0x28) { //read
		fseek(hd->f, lba*512, SEEK_SET);
		fread(data->data, 512, len, hd->f);
//		printf("HD: Read %d bytes.\n", len*512);
		ret=len*512;
	} else if (cmd==0xA || cmd==0x2A) { //write
		fseek(hd->f, lba*512, SEEK_SET);
		fwrite(data->data, 512, len, hd->f);
//		printf("HD: Write %d bytes\n", len*512);
		ret=0;
	} else if (cmd==0x12) { //inquiry
		printf("HD: Inquery\n");
		memcpy(data->data, inq_resp, sizeof(inq_resp));
		return 95;
	} else if (cmd==0x25) { //read capacity
		int lbacnt=hd->size/512;
		data->data[0]=(lbacnt>>24);
		data->data[1]=(lbacnt>>16);
		data->data[2]=(lbacnt>>8);
		data->data[3]=(lbacnt>>0);
		data->data[4]=0;
		data->data[5]=0;
		data->data[6]=2; //512
		data->data[7]=0;
		ret=8;
		printf("HD: Read capacity (%d)\n", lbacnt);
	} else {
		printf("********** hdScsiCmd: unrecognized command %x\n", cmd);
	}
	data->cmd[0]=0; //status
	data->msg[0]=0;
	return ret;
}

SCSIDevice *hdCreate(char *file) {
	SCSIDevice *ret=malloc(sizeof(SCSIDevice));
	memset(ret, 0, sizeof(SCSIDevice));
	HdPriv *hd=malloc(sizeof(HdPriv));
	memset(hd, 0, sizeof(HdPriv));
	hd->f=fopen(file, "r+");
	if (hd->f<=0) {
		perror(file);
		exit(0);
	}
	hd->size=fseek(hd->f, 0, SEEK_END);
	ret->arg=hd;
	ret->scsiCmd=hdScsiCmd;
	return ret;
}

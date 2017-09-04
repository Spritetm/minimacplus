#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hexdump.h"
#include "scc.h"

void localtalkSend(uint8_t *data, int len) {
	uint8_t resp[128];
	if (data[2]==0x81) {
		resp[0]=data[1];
		resp[1]=data[0];
		resp[2]=0x82;
		sccRecv(1, resp, 3);
		printf("LocalTalk: Sent ack\n");
	} else {
		printf("LocalTalk... errm... dunno what to do with this (cmd=%x).\n", data[2]);
	}
}

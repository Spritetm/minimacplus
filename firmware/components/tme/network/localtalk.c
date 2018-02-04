#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "hexdump.h"
#include "scc.h"
#include "ddp.h"
#include "ethertalk.h"
#include <alloca.h>

//LLAP type 0x01-0x7F are for data
#define LLAP_TYPE_ENQ 0x81
#define LLAP_TYPE_ACK 0x82
#define LLAP_TYPE_RTS 0x84
#define LLAP_TYPE_CTS 0x85

#define LLAP_TYPE_DDP_SHORT 1
#define LLAP_TYPE_DDP_LONG 2

typedef struct {
	uint8_t destid; //dest 255 is broadcast
	uint8_t srcid;
	uint8_t type;
	uint8_t data[];
} llap_packet_t;

/*
llap plus short ddp example:
00000000  ff 48 01 00 06 01 01 05  01                       |.H.......|
dest ff
src 48
type 01 (ddp short)
ddp len 00 06
dest sock 01
src sock 01
ddp type 5
data 1
*/

llap_packet_t *bufferedPacket;
int bufferedPacketLen;


void localtalkSend(uint8_t *data, int len) {
	llap_packet_t *p=(llap_packet_t*)data;
	printf("\n\nLOCALTALK PACKET FROM TME MAC\n");
	if (p->type==LLAP_TYPE_ENQ) {
		ethertalk_send_probe(p->destid);
		printf("LocalTalk: enq %hhu->%hhu\n", p->srcid, p->destid);
	} else if (p->type==LLAP_TYPE_RTS) {
		printf("LocalTalk: RTS %hhu->%hhu\n", p->srcid, p->destid);
		if (p->destid!=0xff) {
			printf("Sending CTS.\n");
			uint8_t resp[3];
			llap_packet_t *r=(llap_packet_t*)resp;
			r->destid=p->srcid;
			r->srcid=p->destid;
			r->type=LLAP_TYPE_CTS;
			sccRecv(1, r, sizeof(llap_packet_t), 3);
		}
	} else if (p->type==LLAP_TYPE_CTS) {
		printf("LocalTalk: CTS %hhu->%hhu\n", p->srcid, p->destid);
		if (bufferedPacketLen>0 && p->destid==bufferedPacket->srcid) {
			sccRecv(1, bufferedPacket, bufferedPacketLen, 3);
		}
		bufferedPacketLen=0;
	} else if (p->type==LLAP_TYPE_DDP_SHORT) {
		printf("Localtalk: DDP short %hhu->%hhu\n", p->srcid, p->destid);
		ddp_print(p->data, len-sizeof(llap_packet_t), 0);
		ethertalk_send_short_ddp(p->data, len-sizeof(llap_packet_t), p->srcid, p->destid);
//		hexdump(data, len);
	} else if (p->type==LLAP_TYPE_DDP_LONG) {
		printf("Localtalk: DDP long\n");
		ddp_print(p->data, len-sizeof(llap_packet_t), 1);
		ethertalk_send_long_ddp(p->data, len-sizeof(llap_packet_t));
//		hexdump(data, len);
	} else {
		printf("LocalTalk... errm... dunno what to do with this (cmd=%x).\n", data[2]);
	}
}

void localtalk_send_ddp(uint8_t *data, int len) {
	llap_packet_t rts;
	rts.destid=ddp_get_dest_node(data);
	rts.srcid=ddp_get_src_node(data);
	rts.type=LLAP_TYPE_RTS;
	sccRecv(1, &rts, 3, 60);

	//We assume this is a long ddp packet.
#if 1
	llap_packet_t *p=bufferedPacket;
	p->destid=ddp_get_dest_node(data);
	p->srcid=ddp_get_src_node(data);
	p->type=LLAP_TYPE_DDP_SHORT;
	bufferedPacketLen=ddp_long_to_short(data, p->data, len);
	sccRecv(1, bufferedPacket, bufferedPacketLen, 10);
#else
	printf("Localtalk: Sending ddp of len %d for a total of %d\n", len, len+sizeof(llap_packet_t));
	llap_packet_t *p=bufferedPacket;
	p->destid=ddp_get_dest_node(data);
	p->srcid=ddp_get_src_node(data);
	p->type=LLAP_TYPE_DDP_LONG;
	memcpy(p->data, data, len);
	bufferedPacketLen=len+sizeof(llap_packet_t);
	sccRecv(1, bufferedPacket, bufferedPacketLen, 10);
#endif
}

void localtalk_send_llap_resp(uint8_t node) {
	//ToDo
}

//Called once every now and then.
void localtalkTick() {
	ethertalkTick();
}

void localtalkInit() {
	ethertalkInit();
	bufferedPacketLen=0;
	bufferedPacket=malloc(8192);
}

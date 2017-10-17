#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "basiliskif.h"
#include "hexdump.h"
#include "ddp.h"

//Set to something random. Warning: be sure to initialize rand() first.
static uint8_t myMac[6];
static int myNet=65289;
static int myAddr=0;

//HACK! Officially, we should do stuff with aarp to figure out the address of the other... ahwell.
static uint8_t otherMac[6];
static int otherNet=0;

static const uint8_t snap_atalk[]={0x08, 0x00, 0x07, 0x80, 0x9B};
static const uint8_t snap_aarp[]={0x00, 0x00, 0x00, 0x80, 0xF3};

static const uint8_t mac_atalk_bcast[]={0x09,0x00,0x07,0xFF,0xFF,0xFF};

#define ETH_HDR_LEN 14

typedef struct {
	uint8_t dest[6];
	uint8_t src[6];
	uint16_t len; //of the rest of the packet, starting from dest_sap
	uint8_t dest_sap;
	uint8_t src_sap;
	uint8_t ctrl;
	uint8_t snap_proto[5];
	uint8_t data[];
} __attribute__((packed)) elap_packet_t;

#define AARP_FN_REQUEST 1
#define AARP_FN_RESPONSE 2
#define AARP_FN_PROBE 3

typedef struct {
	uint16_t hw_type;
	uint16_t proto_type;
	uint8_t hw_addr_len;
	uint8_t proto_addr_len;
	uint16_t function;
	uint8_t src_hw[6];
	uint8_t src_atalk[4];
	uint8_t dest_hw[6];
	uint8_t dest_atalk[4];
} __attribute__((packed)) aarp_packet_t;

static void print_mac(uint8_t *mac) {
	printf("%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_atalk(uint8_t *atalk) {
	if (atalk[0]!=0) {
		printf("AtalkAdrCorrupt:%d!=0,", atalk[0]);
	}
	printf("NetID%dDev%d", (atalk[1]<<8)|atalk[2], atalk[3]);
}

static void print_aarp(aarp_packet_t *aarp) {
	printf("hw type %x proto %x ", ntohs(aarp->hw_type), ntohs(aarp->proto_type));

	if (ntohs(aarp->function)==AARP_FN_REQUEST) {
		printf("AARPRequest");
	} else if (ntohs(aarp->function)==AARP_FN_RESPONSE) {
		printf("AARPResponse");
	} else if (ntohs(aarp->function)==AARP_FN_PROBE) {
		printf("AARPProbe");
	} else {
		printf("AARP Unknown fn (%d)", ntohs(aarp->function));
	}
	printf(" Src ");
	print_mac(aarp->src_hw);
	printf(",");
	print_atalk(aarp->src_atalk);
	printf(", dst ");
	print_mac(aarp->dest_hw);
	printf(",");
	print_atalk(aarp->dest_atalk);
}

static void print_elap(elap_packet_t *e, int len) {
	printf("ELAP packet, ");
	print_mac(e->src);
	printf("->");
	print_mac(e->dest);
	printf(", datalen %d", ntohs(e->len));
	if (memcmp(e->snap_proto, snap_atalk, 5)==0) {
		printf(", Atalk ");
		ddp_print(e->data, len-sizeof(elap_packet_t), 1);
	} else if (memcmp(e->snap_proto, snap_aarp, 5)==0) {
		aarp_packet_t *aarp=(aarp_packet_t*)e->data;
		printf(", ");
		print_aarp(aarp);
	} else {
		printf(", unknown (");
		print_mac(e->snap_proto);
		printf(")");
	}
	int plen=ntohs(e->len)+ETH_HDR_LEN;
	if (len!=plen) {
		printf(" !! len mismatch! Packet says %d, we received %d !! ", plen, len);
	}
	printf("\n");
}

//len is size of data
static void create_elap_hdr(elap_packet_t *p, int srcnode, int dstnode, int len) {
	memcpy(p->src, myMac, 6);
	if (dstnode==0xff) {
		memcpy(p->dest, mac_atalk_bcast, 6);
	} else {
		memcpy(p->dest, otherMac, 6);
	}
	p->dest_sap=0xaa;
	p->src_sap=0xaa;
	p->ctrl=0x3;
	memcpy(p->snap_proto, snap_atalk, sizeof(snap_atalk));
	p->len=htons(len+sizeof(elap_packet_t)-ETH_HDR_LEN);
}

int ethertalk_send_long_ddp(uint8_t *data, int size) {
	int newlen=size+sizeof(elap_packet_t);
	elap_packet_t *newp=alloca(newlen);
	create_elap_hdr(newp, ddp_get_src_node(data), ddp_get_dest_node(data), size);
	memcpy(newp->data, data, size);
	printf("Sending ethertalk: ");
	print_elap(newp, newlen);
	basiliskIfSend((uint8_t*)newp, newlen);
}

int ethertalk_send_short_ddp(uint8_t *data, int size, uint8_t srcnode, uint8_t dstnode) {
	//Need to convert to long DDP first
	int newlen=size-DDP_SHORT_HDR_LEN+DDP_LONG_HDR_LEN;
	elap_packet_t *newp=alloca(newlen+sizeof(elap_packet_t));
	create_elap_hdr(newp, srcnode, dstnode, newlen);
	ddp_short_to_long(data, newp->data, newlen, dstnode, otherNet, srcnode, myNet);
	printf("Sending ethertalk: ");
	print_elap(newp, newlen+sizeof(elap_packet_t));
	basiliskIfSend((uint8_t*)newp, newlen+sizeof(elap_packet_t));
	myAddr=srcnode; //assume it's sent from the localtalk seg, so we now know our address.
}


void ethertalk_send_probe(uint8_t id) {
	//ToDo: send probe packet
}

void ethertalkInit() {
	//Generate random MAC for this station
	myMac[0]=2; //locally administrated
	for (int i=1; i<6; i++) myMac[i]=rand();
	basiliskIfInit();
	sniff_open("sniff.pcap");
}

void ethertalkTick() {
	uint8_t buff[1400];
	int r=basiliskIfRecv(buff, sizeof(buff));
	if (r==0) return;

	sniff_write(buff, r);

//	hexdump(buff, r);
	elap_packet_t *elap=(elap_packet_t*)&buff;
	if (memcmp(elap->src, myMac, 6)==0) return; //ignore own packets
	
	printf("\n\nETHERTALK PACKET FROM BASILISKIF (%d bytes)\n", r);
	print_elap(elap, r);
	
	//Try to sniff network ID if needed
	if (otherNet==0 && (memcmp(elap->src, myMac, 6)!=0)) {
		if (memcmp(elap->snap_proto, snap_aarp, 5)==0) {
			aarp_packet_t *aarp=(aarp_packet_t*)elap->data;
			otherNet=(aarp->src_atalk[1]<<8)|aarp->src_atalk[2];
		} else if (memcmp(elap->snap_proto, snap_atalk, 5)==0) {
			otherNet=ddp_get_src_net(elap->data);
		}
		printf("Ethertalk: Sniffed NetID %d\n", otherNet);
		myNet=otherNet; //because... dunno, doesn't work otherwise.
		memcpy(otherMac, elap->src, 6);
	}
	
	if (memcmp(elap->snap_proto, snap_aarp, 5)==0) {
		aarp_packet_t *aarp=(aarp_packet_t*)elap->data;
		if (ntohs(aarp->function)==AARP_FN_RESPONSE) {
			localtalk_send_llap_resp(aarp->src_atalk[3]);
		} else if (ntohs(aarp->function)==AARP_FN_REQUEST) {
			//If the packet is a request for *our* net/addr, respond in kind.
			int net=(aarp->dest_atalk[1]<<8)|aarp->dest_atalk[2];
			int node=aarp->dest_atalk[3];
			printf("Req is for net %d node %d, we are net %d node %d\n", net, node, myNet, myAddr);
			if (net==myNet && node==myAddr) {
				printf("AARP is for us, sending response...\n");
				elap_packet_t *resp=alloca(sizeof(elap_packet_t)+sizeof(aarp_packet_t));
				aarp_packet_t *ra=(aarp_packet_t*)resp->data;
				create_elap_hdr(resp, myAddr, aarp->dest_atalk[3], sizeof(aarp_packet_t));
				//This actuaclly created a DDP packet. Restore to AARP
				memcpy(resp->snap_proto, snap_aarp, sizeof(snap_aarp));
				//Copy rest of stuff. Crib from received aarp packet.
				memcpy(ra, aarp, sizeof(aarp_packet_t));
				ra->function=htons(AARP_FN_RESPONSE);
				memcpy(ra->src_hw, myMac, 6);
				ra->src_atalk[0]=0;
				ra->src_atalk[1]=(myNet>>8);
				ra->src_atalk[2]=(myNet&0xff);
				ra->src_atalk[3]=myAddr;
				memcpy(ra->dest_hw, aarp->src_hw, 6);
				memcpy(ra->dest_atalk, aarp->src_atalk, 6);
				print_elap(resp, sizeof(elap_packet_t)+sizeof(aarp_packet_t));
				basiliskIfSend((uint8_t*)resp, sizeof(elap_packet_t)+sizeof(aarp_packet_t));
			}
		}
	} else if (memcmp(elap->snap_proto, snap_atalk, 5)==0) {
		localtalk_send_ddp(elap->data, r-sizeof(elap_packet_t));
	}
}

#ifdef TESTAPP
int main(int argc, char **argv) {
	ethertalkInit();
	while(1) {
		ethertalkTick();
	}
}
#endif
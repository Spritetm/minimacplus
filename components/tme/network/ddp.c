#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

typedef struct {
	uint16_t len; //Note: of DDP header plus payload
	uint8_t dest_sock;
	uint8_t src_sock;
	uint8_t type;
	uint8_t data[];
} __attribute__((packed)) ddp_short_t;

typedef struct {
	uint16_t len; //Note: of DDP header plus payload
	uint16_t chsum;
	uint16_t dest_net;
	uint16_t src_net;
	uint8_t dest_node;
	uint8_t src_node;
	uint8_t dest_sock;
	uint8_t src_sock;
	uint8_t type;
	uint8_t data[];
} __attribute__((packed)) ddp_long_t;

#define DDP_TYPE_NBP 2

int ddp_get_dest_node(void *ddp) {
	ddp_long_t *p=(ddp_long_t*)ddp;
	return p->dest_node;
}

int ddp_get_src_node(void *ddp) {
	ddp_long_t *p=(ddp_long_t*)ddp;
	return p->src_node;
}

int ddp_get_src_net(void *ddp) {
	ddp_long_t *p=(ddp_long_t*)ddp;
	return ntohs(p->src_net);
}

typedef struct {
	uint8_t fn_count;
	uint8_t id;
} nbd_hdr_t;

typedef struct {
	uint16_t netno;
	uint8_t nodeid;
	uint8_t sockno;
	uint8_t enumerator;
} nbd_tuple_hdr_t;

void ddp_change_nbp_netid_to(uint8_t *nbppacket, int netno) {
	nbd_hdr_t *hdr=(nbd_hdr_t*)nbppacket;
	int ct=hdr->fn_count&0xf;
	uint8_t *p=nbppacket+sizeof(nbd_hdr_t);
	for (int i=0; i<ct; i++) {
		nbd_tuple_hdr_t *th=(nbd_tuple_hdr_t*)p;
		th->netno=htons(netno); //change net
		p+=sizeof(nbd_tuple_hdr_t);
		p+=*p+1; //object field len
		p+=*p+1; //type field len
		p+=*p+1; //zone field len
	}
}

int ddp_short_to_long(void *ddp_short, void *ddp_long, int buflen, uint8_t dest, int destnetno, uint8_t src, int srcnetno) {
	ddp_long_t *l=(ddp_long_t*)ddp_long;
	ddp_short_t *s=(ddp_short_t*)ddp_short;
	if (buflen < ntohs(s->len)-sizeof(ddp_short)+sizeof(ddp_long)) return 0;
	l->len=htons(ntohs(s->len)-sizeof(ddp_short_t)+sizeof(ddp_long_t));
	l->dest_sock=s->dest_sock;
	l->src_sock=s->src_sock;
	l->type=s->type;
	memcpy(l->data, s->data, ntohs(s->len));
	l->dest_node=dest;
	l->src_node=src;
	l->dest_net=(dest==0xff)?0:htons(destnetno);
	l->src_net=htons(srcnetno);
	if (l->type==DDP_TYPE_NBP) {
		ddp_change_nbp_netid_to(l->data, srcnetno);
	}
	l->chsum=0; //we don't perform checksum yet
	return ntohs(l->len);
}

int ddp_long_to_short(void *ddp_long, void *ddp_short, int buflen) {
	ddp_long_t *l=(ddp_long_t*)ddp_long;
	ddp_short_t *s=(ddp_short_t*)ddp_short;
	if (buflen < ntohs(l->len)+sizeof(ddp_short)-sizeof(ddp_long)) return 0;

	s->len=htons(ntohs(l->len)-sizeof(ddp_long_t)+sizeof(ddp_short_t));
	s->dest_sock=l->dest_sock;
	s->src_sock=l->src_sock;
	s->type=l->type;
	memcpy(s->data, l->data, ntohs(l->len));
	if (s->type==DDP_TYPE_NBP) {
		ddp_change_nbp_netid_to(s->data, 0);
	}
	return ntohs(s->len);
}

int ddp_print(void *ddp, int len, int isLong) {
	if (isLong) {
		ddp_long_t *l=(ddp_long_t*)ddp;
		printf("DDP: type %d, net%d,node%d,sock%d -> net%d,node%d,sock%d\n", l->type,
					ntohs(l->src_net), l->src_node, l->src_sock,
					ntohs(l->dest_net), l->dest_node, l->dest_sock);
		int plen=ntohs(l->len)&0x7FF;
		if (len!=plen) {
			printf("Len mismatch! Recved %d, packet says %d\n", len, plen);
		}
	} else {
		ddp_short_t *s=(ddp_short_t*)ddp;
		printf("DDP: type %d sock%d -> sock%d ", 
				s->type, s->src_sock, s->dest_sock);
		int plen=ntohs(s->len)&0x3FF;
		if (len!=plen) {
			printf("Len mismatch! Recved %d, packet says %d\n", len, plen);
		} else {
			printf("DDP len (inc ddp hdr): %d\n", len);
		}
	}
	return 1;
}



/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>


typedef struct pcap_hdr_s {
        uint32_t magic_number;   /* magic number */
        uint16_t version_major;  /* major version number */
        uint16_t version_minor;  /* minor version number */
        int32_t  thiszone;       /* GMT to local correction */
        uint32_t sigfigs;        /* accuracy of timestamps */
        uint32_t snaplen;        /* max length of captured packets, in octets */
        uint32_t network;        /* data link type */
} __attribute__((packed)) pcap_hdr_t;

typedef struct pcaprec_hdr_s {
        uint32_t ts_sec;         /* timestamp seconds */
        uint32_t ts_usec;        /* timestamp microseconds */
        uint32_t incl_len;       /* number of octets of packet saved in file */
        uint32_t orig_len;       /* actual length of packet */
} __attribute__((packed)) pcaprec_hdr_t;

static FILE *f=NULL;

void sniff_open(char *name) {
	f=fopen(name, "wb");
	if (f==NULL) {
		perror(name);
		exit(1);
	}
	pcap_hdr_t hdr={
		.magic_number=0xa1b2c3d4,
		.version_major=2,
		.version_minor=4,
		.thiszone=0,
		.sigfigs=0,
		.snaplen=65535,
		.network=1
	};
	fwrite(&hdr, sizeof(hdr), 1, f);
}

void sniff_write(uint8_t *buff, int len) {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	pcaprec_hdr_t hdr={
		.ts_sec=tv.tv_sec,
		.ts_usec=tv.tv_usec,
		.incl_len=len,
		.orig_len=len,
	};
	fwrite(&hdr, sizeof(hdr), 1, f);
	fwrite(buff, len, 1, f);
	fflush(f);
}


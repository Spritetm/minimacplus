//Unfinished appletalk-over-wifi attempt
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include<arpa/inet.h>
#include "basiliskif.h"

#define PORT 6066 //default BasiliskII network poer

int sock;

int basiliskIfInit() {
	//Make socket
	sock=socket(AF_INET, SOCK_DGRAM, 0);
	if (socket<0) {
		printf("Couldn't create socket\n");
		return 0;
	}

	int broadcastEnable=1;
	setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
	int reuse=1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	struct sockaddr_in si;
	memset((char *) &si, 0, sizeof(si));
	si.sin_family = AF_INET;
	si.sin_port = htons(PORT);
	si.sin_addr.s_addr = htonl(INADDR_ANY);
	//bind socket to port
	if (bind(sock, (struct sockaddr*)&si, sizeof(si)) == -1) {
		printf("Error binding to port\n");
		return 0;
	}

	return 1;
}

int basiliskIfRecv(uint8_t *buf, int len) {
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sock, &fds);
	struct timeval tout;
	memset(&tout, 0, sizeof(tout));
	int r=select(sock+1, &fds, NULL, NULL, &tout);
	if (r>0) {
		int l=read(sock, buf, len);
		return l;
	} else {
		return 0;
	}
}

int basiliskIfSend(uint8_t *buf, int len) {
	struct sockaddr_in sin;
	memset(&sin, 0, sizeof(struct sockaddr_in));
	sin.sin_family = AF_INET;
	sin.sin_port = (in_port_t)htons(PORT);
	sin.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	
	int r=sendto(sock, buf, len, 0, (struct sockaddr *)&sin, sizeof(struct sockaddr_in));
	if (r<0) {
		perror("sendto");
	}
}

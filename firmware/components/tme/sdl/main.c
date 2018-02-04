//Stuff for a host build of TME
/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Jeroen Domburg <jeroen@spritesmods.com> wrote this file. As long as you retain 
 * this notice you can do whatever you want with this stuff. If we meet some day, 
 * and you think this stuff is worth it, you can buy me a beer in return. 
 * ----------------------------------------------------------------------------
 */

#include <stdio.h>
#include <stdlib.h>
#include "emu.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "tmeconfig.h"
#include "snd.h"
#include "disp.h"

static void *loadRom(char *file) {
	int i;
	char *ret=malloc(TME_ROMSIZE);
	int f=open(file, O_RDONLY);
	i=read(f, ret, TME_ROMSIZE);
	if (i!=TME_ROMSIZE) {
		perror("reading rom");
		exit(1);
	}
	return ret;
}

void saveRtcMem(char *data) {
	printf("Saving RTC mem...\n");
	FILE *f=fopen("pram.dat", "wb");
	if (f!=NULL) {
		fwrite(data, 32, 1, f);
		fclose(f);
	}
}

int main(int argc, char **argv) {
	void *rom=loadRom("rom.bin");
	FILE *f=fopen("pram.dat", "r");
	if (f!=NULL) {
		char data[32];
		fread(data, 32, 1, f);
		rtcInit(data);
		fclose(f);
		printf("Loaded RTC data from pram.dat\n");
	}
	sdlDispAudioInit();
	tmeStartEmu(rom);
}

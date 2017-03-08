#include "stdio.h"
#include "stdint.h"
#include <stdlib.h>

#define MAXROMSZ 3*1024*1024

int main(int argc, char **argv) {
	FILE *f;
	int l, x;
	unsigned int oldchs, newchs;
	unsigned char rom[MAXROMSZ];
	if (argc!=2) {
		printf("Usage: %s mac-rom-image.bin\nRecalculate checksum on modified rom\n");
		exit(0);
	}

	f=fopen(argv[1], "r+");
	if (f<=0) {
		perror(argv[1]);
		exit(1);
	}
	l=fread(rom, 1, MAXROMSZ, f);

	oldchs=(rom[0]<<24)+(rom[1]<<16)+(rom[2]<<8)+(rom[3]);
	newchs=0;
	for (x=4; x<l; x+=2) {
		newchs+=(rom[x]<<8)+rom[x+1];
	}
	printf("ROM size: %X\n", l);
	printf("Old checksum: %04X\n", oldchs);
	printf("New checksum: %04X\n", newchs);
	if (oldchs!=newchs) {
		rom[0]=newchs>>24;
		rom[1]=newchs>>16;
		rom[2]=newchs>>8;
		rom[3]=newchs>>0;
		fseek(f, 0, SEEK_SET);
		fwrite(rom, 4, 1, f);
	}
	fclose(f);
}